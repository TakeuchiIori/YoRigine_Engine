#include "LightManager.h"
// C++
#include <algorithm>
#include <vector>
#include <limits>

// Engine
#include "DirectXCommon.h"
#include "Object3D/Object3dCommon.h"

// Math
#include "MathFunc.h"


#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG

namespace YoRigine {
	//=====================================================================
	// シングルトン取得
	//=====================================================================>
	LightManager* LightManager::GetInstance()
	{
		static LightManager instance;
		return &instance;
	}

	//=====================================================================
	// 初期化
	//=====================================================================
	void LightManager::Initialize()
	{
		// Object3d 共通処理取得
		this->object3dCommon_ = Object3dCommon::GetInstance();

		// デフォルトカメラを取得（Specular 用）
		this->camera_ = object3dCommon_->GetDefaultCamera();

		// 各ライト種類ごとの定数バッファを GPU 上に作成
		CreateDirectionalLightResource();
		CreatePointLightResource();
		CreateSpotLightResource();
		CreateShadowResource();
	}

	//=====================================================================
	// コマンドリストへ CBV 設定
	//=====================================================================
	void LightManager::SetCommandList()
	{
		// カメラ位置を毎フレーム更新（Specular 用）
		// 順番は RootSignature に合わせること（Object の場合 RootParam#3〜#6）
		object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
		object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());
		object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
	}

	//=====================================================================
	// 平行光源リソース作成
	//=====================================================================
	void LightManager::CreateDirectionalLightResource()
	{
		directionalLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));
		directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight_));

		directionalLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		directionalLight_->direction = Normalize({ 0.0f, -1.0f, 1.0f });
		directionalLight_->intensity = 1.0f;
		directionalLight_->enableDirectionalLight = true;
	}

	//=====================================================================
	// ポイントライトリソース作成
	//=====================================================================
	void LightManager::CreatePointLightResource()
	{
		pointLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(PointLight));
		pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLight_));

		pointLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		pointLight_->position = { 0.0f, 2.0f, 0.0f };
		pointLight_->intensity = 1.0f;
		pointLight_->radius = 10.0f;
		pointLight_->decay = 1.0f;
		pointLight_->enablePointLight = false;
	}


	//=====================================================================
	// スポットライトリソース作成
	//=====================================================================
	void LightManager::CreateSpotLightResource()
	{
		spotLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(SpotLight));
		spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLight_));

		spotLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		spotLight_->position = { 2.0f, 1.25f, 0.0f };
		spotLight_->distance = 7.0f;
		spotLight_->direction = Normalize(Vector3{ -1.0f,-1.0f,0.0f });
		spotLight_->intensity = 4.0f;
		spotLight_->decay = 2.0f;

		// cosAngle, cosFalloffStart は角度ではなく“コサイン値”に注意
		spotLight_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);      // 60°
		spotLight_->cosFalloffStart = std::cos(std::numbers::pi_v<float> / 4.0f); // 45°

		spotLight_->enableSpotLight = false;
	}

	/*==========================================================================
	影のリソース作成
	//========================================================================*/
	void LightManager::CreateShadowResource()
	{
		shadowResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(ShadowMatrix));
		shadowResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadow_));

		// 初期値は単位行列
		shadow_->lightViewProjection = MakeIdentity4x4();
	}


	/*==========================================================================
	影の計算処理（現状平行光源のみ）
	//========================================================================*/
	void LightManager::UpdateShadowMatrix(Camera* camera)
	{
		camera_ = camera;

		// ライト方向は必ず正規化して使う
		Vector3 lightDir = Normalize(directionalLight_->direction);

		// ライト用ビュー行列の計算に必要なターゲットとライト位置を計算 (View行列の基準点)
		Vector3 target = camera_->transform_.translate;
		// shadowDistance は、ライトビュー行列の基準点オフセットにのみ使用されます
		Vector3 lightPos = target - lightDir * shadowmapSettings_.shadowDistance;

		// ライト用ビュー行列（up は世界の Y 軸）
		Matrix4x4 lightView =
			MatrixLookAtLH(lightPos, target, Vector3(0.0f, 1.0f, 0.0f));
		Matrix4x4 cameraInverseVP = Inverse(camera_->viewProjectionMatrix_);

		// NDC空間の8つの角をワールド座標、さらにライト空間に変換し、AABBを計算
		std::vector<Vector3> frustumCornersNDC = {
			// Near Plane (Z=0.0f or -1.0f, depending on API. Assuming DirectX Z=[0, 1] based on context.)
			{ -1.0f, -1.0f, 0.0f }, { -1.0f,  1.0f, 0.0f },
			{  1.0f,  1.0f, 0.0f }, {  1.0f, -1.0f, 0.0f },
			// Far Plane (Z=1.0f)
			{ -1.0f, -1.0f, 1.0f }, { -1.0f,  1.0f, 1.0f },
			{  1.0f,  1.0f, 1.0f }, {  1.0f, -1.0f, 1.0f },
		};

		float minX = std::numeric_limits<float>::max();
		float minY = std::numeric_limits<float>::max();
		// Z軸は拡張するため、初期値にはカメラ視錐台の最も遠い点(maxZ)を基準として使う
		float minZ = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::min();
		float maxY = std::numeric_limits<float>::min();
		float maxZ = std::numeric_limits<float>::min();

		for (const auto& cornerNDC : frustumCornersNDC) {
			Vector3 cornerWorld = Transform(cornerNDC, cameraInverseVP);
			Vector3 cornerLight = Transform(cornerWorld, lightView);

			// AABB（軸並行バウンディングボックス）を更新
			minX = std::min(minX, cornerLight.x);
			maxX = std::max(maxX, cornerLight.x);
			minY = std::min(minY, cornerLight.y);
			maxY = std::max(maxY, cornerLight.y);
			minZ = std::min(minZ, cornerLight.z); // カメラ視錐台の最も手前と奥のZを記録
			maxZ = std::max(maxZ, cornerLight.z);
		}

		float lightFrustumFarZ = maxZ;
		float lightFrustumNearZ = lightFrustumFarZ - shadowmapSettings_.farZ;
		Matrix4x4 lightProj = MakeOrthographicMatrix(minX, maxY, maxX, minY, lightFrustumNearZ, lightFrustumFarZ);

		// 最終的なライトビュー射影行列
		shadow_->lightViewProjection = lightView * lightProj;
	}
	/*==========================================================================
	平行光源のセット
	//========================================================================*/
	void LightManager::SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity, bool enable)
	{
		directionalLight_->color = color;
		directionalLight_->direction = direction;
		directionalLight_->intensity = intensity;
		directionalLight_->enableDirectionalLight = enable;
	}
	/*==========================================================================
	ポイントライトのセット
	//========================================================================*/
	void LightManager::SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay, bool enable)
	{
		pointLight_->color = color;
		pointLight_->position = position;
		pointLight_->intensity = intensity;
		pointLight_->radius = radius;
		pointLight_->decay = decay;
		pointLight_->enablePointLight = enable;
	}

	//=====================================================================
	// ImGui ライティング編集ウィンドウ
	//=====================================================================
	void LightManager::ShowLightingEditor()
	{
#ifdef USE_IMGUI

		//------------------------------------------------------------
		// 平行光源
		//------------------------------------------------------------
		ImGui::Text("Directional Light");

		bool directionalLightEnabled = IsDirectionalLightEnabled();
		if (ImGui::Checkbox("Directional Enabled", &directionalLightEnabled)) {
			SetDirectionalLightEnabled(directionalLightEnabled);
		}

		Vector3 lightDirection = GetDirectionalLightDirection();
		if (ImGui::SliderFloat3("Direction", &lightDirection.x, -1.0f, 1.0f, "%.2f")) {
			SetDirectionalLightDirection(lightDirection);
		}

		Vector4 lightColor = GetDirectionalLightColor();
		if (ImGui::ColorEdit4("Color", &lightColor.x)) {
			SetDirectionalLightColor(lightColor);
		}

		float lightIntensity = GetDirectionalLightIntensity();
		if (ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 10.0f, "%.2f")) {
			SetDirectionalLightIntensity(lightIntensity);
		}

		//------------------------------------------------------------
		// ポイントライト
		//------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("Point Light");

		bool pointLightEnabled = IsPointLightEnabled();
		if (ImGui::Checkbox("Enabled", &pointLightEnabled)) {
			SetPointLightEnabled(pointLightEnabled);
		}

		Vector4 pointLightColor = GetPointLightColor();
		if (ImGui::ColorEdit4("Point Color", &pointLightColor.x)) {
			SetPointLightColor(pointLightColor);
		}

		Vector3 pointLightPosition = GetPointLightPosition();
		if (ImGui::SliderFloat3("Position", &pointLightPosition.x, -10.0f, 10.0f, "%.2f")) {
			SetPointLightPosition(pointLightPosition);
		}

		float pointLightIntensity = GetPointLightIntensity();
		if (ImGui::SliderFloat("Point Intensity", &pointLightIntensity, 0.0f, 10.0f, "%.2f")) {
			SetPointLightIntensity(pointLightIntensity);
		}

		float radius = GetPointLightRadius();
		if (ImGui::SliderFloat("Point Radius", &radius, 0.0f, 1000.0f, "%.2f")) {
			SetPointLightRadius(radius);
		}

		float decay = GetPointLightDecay();
		if (ImGui::SliderFloat("Point Decay", &decay, 0.0f, 10.0f, "%.2f")) {
			SetPointLightDecay(decay);
		}

		//------------------------------------------------------------
		// スポットライト
		//------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("Spot Light");

		bool spotLightEnabled = IsSpotLightEnabled();
		if (ImGui::Checkbox("Spot Enabled", &spotLightEnabled)) {
			SetSpotLightEnabled(spotLightEnabled);
		}

		Vector4 spotLightColor = GetSpotLightColor();
		if (ImGui::ColorEdit4("Spot Color", &spotLightColor.x)) {
			SetSpotLightColor(spotLightColor);
		}

		Vector3 spotLightPosition = GetSpotLightPosition();
		if (ImGui::SliderFloat3("Spot Position", &spotLightPosition.x, -10.0f, 10.0f, "%.2f")) {
			SetSpotLightPosition(spotLightPosition);
		}

		Vector3 spotLightDirection = GetSpotLightDirection();
		if (ImGui::SliderFloat3("Spot Direction", &spotLightDirection.x, -10.0f, 10.0f, "%.2f")) {
			SetSpotLightDirection(spotLightDirection);
		}

		float spotLightIntensity = GetSpotLightIntensity();
		if (ImGui::SliderFloat("Spot Intensity", &spotLightIntensity, 0.0f, 100.0f, "%.2f")) {
			SetSpotLightIntensity(spotLightIntensity);
		}

		float spotLightDistance = GetSpotLightDistance();
		if (ImGui::SliderFloat("Spot Distance", &spotLightDistance, 0.0f, 200.0f, "%.2f")) {
			SetSpotLightDistance(spotLightDistance);
		}

		float spotLightDecay = GetSpotLightDecay();
		if (ImGui::SliderFloat("Spot Decay", &spotLightDecay, 0.0f, 100.0f, "%.2f")) {
			SetSpotLightDecay(spotLightDecay);
		}

		float spotLightCosAngle = GetSpotLightCosAngle();
		if (ImGui::SliderFloat("Spot Angle", &spotLightCosAngle, 0.0f, 1.0f, "%.2f")) {
			SetSpotLightCosAngle(spotLightCosAngle);
		}

		float spotLightCosFalloffStart = spotLight_->cosFalloffStart;
		if (ImGui::SliderFloat("Spot Falloff Start", &spotLightCosFalloffStart, 0.0f, 1.0f, "%.2f")) {
			spotLight_->cosFalloffStart = spotLightCosFalloffStart;
		}

		//------------------------------------------------------------
		// シャドウマップ	
		//------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("Shadowmap Settings");
		float shadowDistance = shadowmapSettings_.shadowDistance;
		if (ImGui::DragFloat("Shadow Distance", &shadowDistance, 1.0f, 500.0f)) {
			shadowmapSettings_.shadowDistance = shadowDistance;
		}
		float orthoWidth = shadowmapSettings_.orthoWidth;
		if (ImGui::DragFloat("Shadow orthoWidth", &orthoWidth, 1.0f, 500.0f)) {
			shadowmapSettings_.orthoWidth = orthoWidth;
		}
		float orthoHeight = shadowmapSettings_.orthoHeight;
		if (ImGui::DragFloat("Shadow orthoHeight", &orthoHeight, 1.0f, 500.0f)) {
			shadowmapSettings_.orthoHeight = orthoHeight;
		}
		float nearZ = shadowmapSettings_.nearZ;
		if (ImGui::DragFloat("Shadow nearZ", &nearZ, 1.0f, 500.0f)) {
			shadowmapSettings_.nearZ = nearZ;
		}
		float farZ = shadowmapSettings_.farZ;
		if (ImGui::DragFloat("Shadow farZ", &farZ, 1.0f, 500.0f)) {
			shadowmapSettings_.farZ = farZ;
		}

#endif // _DEBUG
	}
}
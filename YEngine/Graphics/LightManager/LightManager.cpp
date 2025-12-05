#include "LightManager.h"
// C++
#include <algorithm>

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
	//=====================================================================
	/// <summary>
	/// LightManager のシングルトンインスタンスを取得
	/// </summary>
	LightManager* LightManager::GetInstance()
	{
		static LightManager instance;
		return &instance;
	}

	//=====================================================================
	// 初期化
	//=====================================================================
	/// <summary>
	/// ライト関連のGPUリソースを作成し、デフォルト値を設定
	/// </summary>
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
	/// <summary>
	/// 各ライト定数バッファをパイプラインにセット
	/// </summary>
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
	/// <summary>
	/// 平行光源用の定数バッファを作成し、初期値を設定
	/// </summary>
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
	/// <summary>
	/// ポイントライト用の定数バッファを作成し、初期値を設定
	/// </summary>
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

	void LightManager::CreateShadowResource()
	{
		shadowResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(ShadowMatrix));
		shadowResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadow_));

		// 初期値は単位行列
		shadow_->lightViewProjection = MakeIdentity4x4();
	}

	void LightManager::UpdateShadowMatrix(Camera* camera)
	{
		camera_ = camera;

		// 1. ライト方向は必ず正規化して使う
		Vector3 lightDir = Normalize(directionalLight_->direction);

		// 2. カメラ位置を中心にライトをオフセット
		Vector3 target = camera_->transform_.translate;
		float   distance = shadowmapSettings_.shadowDistance;
		Vector3 lightPos = target - lightDir * distance;

		// 3. ライト用ビュー行列（up は世界の Y 軸）shas
		Matrix4x4 lightView =
			MatrixLookAtLH(lightPos, target, Vector3(0.0f, 1.0f, 0.0f));

		// 4. near / far の関係が崩れないように補正（最悪でも far > near にする）
		float nearZ = shadowmapSettings_.nearZ;
		float farZ = shadowmapSettings_.farZ;
		if (farZ <= nearZ + 0.01f) {
			farZ = nearZ + 0.01f;
		}

		// 5. 正射影行列
		float w = shadowmapSettings_.orthoWidth;
		float h = shadowmapSettings_.orthoHeight;

		Matrix4x4 lightProj =
			MakeOrthographicMatrix(-w, h, w, -h, nearZ, farZ);

		// 6. 最終的なライトビュー射影行列
		shadow_->lightViewProjection = lightView * lightProj;
	}

	//=====================================================================
	// setter 関数群（summary 省略版）
	//=====================================================================
	void LightManager::SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity, bool enable)
	{
		directionalLight_->color = color;
		directionalLight_->direction = direction;
		directionalLight_->intensity = intensity;
		directionalLight_->enableDirectionalLight = enable;
	}

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
	/// <summary>
	/// ImGui 上でライトを操作できるエディタ
	/// </summary>
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
		// 鏡面反射（Specular）
		//------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("Specular Reflection");

		//bool specularEnabled = IsSpecularEnabled();
		//if (ImGui::Checkbox("Enable Specular", &specularEnabled)) {
		//	SetSpecularEnabled(specularEnabled);
		//}

		//bool isHalfVector = IsHalfVectorUsed();
		//if (ImGui::Checkbox("Use Half Vector", &isHalfVector)) {
		//	SetHalfVectorUsed(isHalfVector);
		//}

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
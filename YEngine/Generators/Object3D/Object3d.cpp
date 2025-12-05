#include "Object3d.h"
#include "Object3dCommon.h"

// C++
#include <assert.h>
#include <fstream>
#include <sstream>
#include <algorithm>

// Engine
#include "Loaders./Texture./TextureManager.h"
#include "ModelManager.h"
#include "Model.h"
#include "WorldTransform./WorldTransform.h"
#include "Debugger/Logger.h"
#include <LightManager/LightManager.h>
#include <PipelineManager/ShadowPipeline.h>


const std::string Object3d::defaultModelPath_ = "Resources/Models/";


/// <summary>
/// コンストラクタ
/// </summary>
Object3d::Object3d()
{
}

/// <summary>
/// Object3d の初期化
/// </summary>
void Object3d::Initialize()
{
	object3dCommon_ = Object3dCommon::GetInstance();

	CreateCameraResource();
	CreateShadowResources();
	materialColor_ = std::make_unique<MaterialColor>();    materialColor_->Initialize();
	materialLighting_ = std::make_unique<MaterialLighting>(); materialLighting_->Initialize();
	materialUV_ = std::make_unique<MaterialUV>();       materialUV_->Initialize();
}

/// <summary>
/// モデルのアニメーションを更新
/// </summary>
void Object3d::UpdateAnimation()
{
	if (model_) {
		model_->UpdateAnimation();
	}
}

/// <summary>
/// Object3d の描画
/// </summary>
void Object3d::Draw(Camera* camera, WorldTransform& worldTransform)
{

	cameraData_->viewProjection = camera->GetViewProjectionMatrix();

	UpdateUV();

	Matrix4x4 worldViewProjectionMatrix{};
	Matrix4x4 worldMatrix{};

	if (model_) {

		if (camera) {
			const Matrix4x4& vp = camera->GetViewProjectionMatrix();

			// ボーンの有無でルート行列の扱いが変わる
			if (!model_->GetHasBones()) {
				worldViewProjectionMatrix = worldTransform.GetMatWorld() * model_->GetRootNode().GetLocalMatrix() * vp;
				worldMatrix = worldTransform.GetMatWorld() * model_->GetRootNode().GetLocalMatrix();
			} else {
				worldViewProjectionMatrix = worldTransform.GetMatWorld() * vp;
				worldMatrix = worldTransform.GetMatWorld();
			}
		} else {
			// カメラ無し（デバッグ）
			worldViewProjectionMatrix = worldTransform.GetMatWorld();
			worldMatrix = worldTransform.GetMatWorld();
		}
	}

	worldTransform.SetMapWVP(worldViewProjectionMatrix);
	worldTransform.SetMapWorld(worldMatrix);

	auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

	// ワールド
	commandList->SetGraphicsRootConstantBufferView(1, worldTransform.GetConstBuffer()->GetGPUVirtualAddress());

	// カメラ
	commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(12, YoRigine::LightManager::GetInstance()->GetShadowResource()->GetGPUVirtualAddress());
	// マテリアル
	materialUV_->RecordDrawCommands(commandList.Get(), 0);
	materialColor_->RecordDrawCommands(commandList.Get(), 7);
	materialLighting_->RecordDrawCommands(commandList.Get(), 8);

	// モデル描画
	if (model_) {
		model_->Draw();
	}
}

/// <summary>
/// モデルボーンのデバッグ描画
/// </summary>
void Object3d::DrawBone(Line& line, const Matrix4x4& worldMatrix)
{
	if (model_) {
		model_->DrawBone(line, worldMatrix);
	}
}

void Object3d::DrawShadow(WorldTransform& worldTransform)
{
	auto commandList = object3dCommon_->GetDxCommon()->GetCommandList().Get();
	// シャドウ用パイプライン
	commandList->SetPipelineState(
		ShadowPipeline::GetInstance()->GetPipeLineStateObject("Shadowmap")
	);
	commandList->SetGraphicsRootSignature(
		ShadowPipeline::GetInstance()->GetRootSignature("Shadowmap"));

	// DrawShadow 内
	if (!model_->GetHasBones()) {
		objectData_->world =
			worldTransform.GetMatWorld() * model_->GetRootNode().GetLocalMatrix();
	} else {
		objectData_->world = worldTransform.GetMatWorld();
	}
	commandList->SetGraphicsRootConstantBufferView(0, objectCB_->GetGPUVirtualAddress());
	// b1: lightViewProj
	commandList->SetGraphicsRootConstantBufferView(1, YoRigine::LightManager::GetInstance()->GetShadowResource()->GetGPUVirtualAddress());

	// モデル描画（Shadow用）
	model_->DrawShadow();
}

/// <summary>
/// カメラ用リソースの生成
/// </summary>
void Object3d::CreateCameraResource()
{
	cameraResource_ =
		object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(CameraForGPU));

	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
	cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
}

void Object3d::CreateShadowResources()
{
	objectCB_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(ObjectTransform));
	objectCB_->Map(0, nullptr, reinterpret_cast<void**>(&objectData_));
	objectData_->world = MakeIdentity4x4();
}

/// <summary>
/// UVアニメーションの更新
/// </summary>
void Object3d::UpdateUV()
{
	Vector3 s = { uvScale.x,       uvScale.y,       1.0f };
	Vector3 r = { 0.0f,            0.0f,            uvRotate };
	Vector3 t = { uvTranslate.x,   uvTranslate.y,   0.0f };

	Matrix4x4 affine = MakeAffineMatrix(s, r, t);

	SetUvTransform(affine);
}

/// <summary>
/// モデルを読み込みセットする
/// </summary>
void Object3d::SetModel(const std::string& filePath, bool isAnimation, const std::string& animationName)
{
	// モデルのパスを解析して正しい形式に変換
	auto [basePath, fileName] = ModelManager::GetInstance()->ParseModelPath(filePath);

	ModelManager::GetInstance()->LoadModel(
		defaultModelPath_ + basePath, fileName, animationName, isAnimation
	);

	model_ = ModelManager::GetInstance()->FindModel(
		fileName, animationName, isAnimation
	);
}

/// <summary>
/// モデルのデバッグ情報を表示
/// </summary>
void Object3d::DebugInfo()
{
	if (model_) {
		model_->DebugInfo();
	}
}

/// <summary>
/// 別モーションへ切り替える
/// </summary>
void Object3d::SetChangeMotion(const std::string& filePath, MotionPlayMode playMode, const std::string& animationName)
{
	std::string basePath = filePath;
	std::string fileName;

	if (basePath.ends_with(".gltf")) {
		basePath = basePath.substr(0, basePath.size() - 5);
		fileName = basePath + ".gltf";
	}

	if (model_) {
		model_->SetChangeMotion(
			defaultModelPath_ + basePath, fileName, playMode, animationName
		);
	}
}

/// <summary>
//モーション速度の切り替え
/// </summary>
/// <param name="speed"></param>
void Object3d::SetMotionSpeed(float speed)
{
	if (model_) {
		if (model_->GetMotionSystem()) {
			model_->GetMotionSystem()->SetCurrentAnimationSpeed(speed);
		}
	}
}
/// <summary>
/// ファイル名から Object3d を生成
/// </summary>
std::unique_ptr<Object3d> Object3d::Create(const std::string& filePath, const std::string& animationName, bool isAnimation)
{
	// モデルのパスを解析して正しい形式に変換
	auto [basePath, fileName] = ModelManager::GetInstance()->ParseModelPath(filePath);

	ModelManager::GetInstance()->LoadModel(
		defaultModelPath_ + basePath, fileName, animationName, isAnimation
	);

	Model* model = ModelManager::GetInstance()->FindModel(fileName);
	if (!model) {
		return nullptr;
	}
	auto newObj = std::make_unique<Object3d>();
	newObj->Initialize();
	newObj->model_ = model;
	return newObj;
}
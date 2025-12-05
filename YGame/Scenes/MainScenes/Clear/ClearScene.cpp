#include "ClearScene.h"

// Engine
#include <SceneSystems/SceneManager.h>
#include "Systems./Input./Input.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Sprite/SpriteCommon.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include "Loaders/Json/JsonManager.h"
#include "ModelManipulator/ModelManipulator.h"
#include "OffScreen/PostEffectManager.h"
#include <Debugger/Logger.h>

/// <summary>
/// 初期化処理
/// </summary>
void ClearScene::Initialize() {
	sceneCamera_ = cameraManager_.AddCamera();

	//------------------------------------------------------------
	// システム初期化
	//------------------------------------------------------------
	YoRigine::GameTime::Initailzie();
	YoRigine::JsonManager::SetCurrentScene("ClearScene");
	YoRigine::CollisionManager::GetInstance()->Initialize();
	YoRigine::ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::ModelManipulator::GetInstance()->Initialize("ClearScene");
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());

	//------------------------------------------------------------
	// カメラ初期化
	//------------------------------------------------------------
	followCamera_.Initialize();
	debugCamera_.Initialize();
	splineCamera_.Initialize();
	topDownCamera_.Initialize();
	defaultCamera_.Initialize();
	cameraMode_ = CameraMode::CLEAR;

	//------------------------------------------------------------
	// クリア画面スプライトの生成と設定
	//------------------------------------------------------------
	clearUI_ = std::make_unique<ClearUI>();
	clearUI_->Initialize();

	//------------------------------------------------------------
	// オブジェクトの生成
	//------------------------------------------------------------

	player_ = std::make_unique<DemoPlayer>();
	player_->Initialize(sceneCamera_.get());
	player_->SetMotion("Idle2");

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(sceneCamera_.get(), "Resources/DDS/vz_sinister_land_cubemap_ue.dds");

	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_.get());

#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("カメラモード", [this]() { UpdateCameraMode(); }, "Clear");
	Editor::GetInstance()->RegisterGameUI("ライティング", [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); }, "Clear");
#endif
}

/// <summary>
/// 終了処理
/// </summary>
void ClearScene::Finalize() {
	YoRigine::JsonManager::ClearSceneInstances("ClearScene");
}

/// <summary>
/// 更新処理
/// </summary>
void ClearScene::Update() {
	YoRigine::GameTime::Update();
	UpdateCamera();
	if (YoRigine::Input::GetInstance()->IsPadPressed(0, GamePadButton::A)) {
		SceneManager::GetInstance()->ChangeScene("Title");
	}

	clearUI_->Update();

	player_->Update();
	skyBox_->Update();
	ground_->Update();



	YoRigine::ParticleManager::GetInstance()->Emit("ClearParticle", Vector3(0, 0, 0), 10);
	YoRigine::ModelManipulator::GetInstance()->Update();
	cameraManager_.UpdateAllCameras();
	YoRigine::CollisionManager::GetInstance()->Update();
	YoRigine::ParticleManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
}

/// <summary>
/// 描画処理
/// </summary>
void ClearScene::Draw() {

	//------------------------------------------------------------
	// 3D描画（背景・キャラ）
	//------------------------------------------------------------
	skyBox_->Draw();
	Object3dCommon::GetInstance()->DrawPreference();
	YoRigine::LightManager::GetInstance()->SetCommandList();
	DrawObject();

	//------------------------------------------------------------
	// 演出関連の描画（パーティクルなど）
	//------------------------------------------------------------
	YoRigine::ParticleManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// 2Dスプライト描画
	//------------------------------------------------------------
	SpriteCommon::GetInstance()->DrawPreference();
	clearUI_->DrawAll();

	//------------------------------------------------------------
	// 3Dオブジェクト描画（必要時に追加）
	//------------------------------------------------------------
	Object3dCommon::GetInstance()->DrawPreference();
}

/// <summary>
/// オフスクリーン外の描画処理
/// </summary>
void ClearScene::DrawNonOffscreen() {
	// 今のところ特に処理なし（タイトル演出など追加予定）
}

/// <summary>
/// 影の描画
/// </summary>
void ClearScene::DrawShadow()
{
}

/// <summary>
/// オブジェクト描画（地面・プレイヤー）
/// </summary>
void ClearScene::DrawObject() {
	ground_->Draw();
	player_->Draw();
	player_->DrawAnimation();
}

/// <summary>
/// ライン描画（デバッグ用）
/// </summary>
void ClearScene::DrawLine() {}

/// <summary>
/// UI描画（必要に応じて拡張）
/// </summary>
void ClearScene::DrawUI() {}

/// <summary>
/// カメラモード切り替えUI（ImGui）
/// </summary>
void ClearScene::UpdateCameraMode() {
#ifdef USE_IMGUI
	ImGui::Begin("Camera Mode");
	if (ImGui::Button("DEFAULT Camera")) cameraMode_ = CameraMode::CLEAR;
	if (ImGui::Button("Debug Camera")) cameraMode_ = CameraMode::DEBUG;
	ImGui::End();
#endif
}

void ClearScene::UpdateCamera() {
	switch (cameraMode_) {
	case CameraMode::CLEAR:
		sceneCamera_->SetFovY(defaultCamera_.GetFov());
		defaultCamera_.Update();
		sceneCamera_->viewMatrix_ = defaultCamera_.matView_;
		sceneCamera_->transform_.translate = defaultCamera_.translate_;
		sceneCamera_->transform_.rotate = defaultCamera_.rotate_;
		sceneCamera_->UpdateMatrix();
		break;

	case CameraMode::DEBUG:
		if (isDebugCamera_) {
			sceneCamera_->SetFovY(debugCamera_.GetFov());
			debugCamera_.Update();
			sceneCamera_->viewMatrix_ = debugCamera_.matView_;
			sceneCamera_->transform_.translate = debugCamera_.translate_;
			sceneCamera_->transform_.rotate = debugCamera_.rotate_;
			sceneCamera_->UpdateMatrix();
		}
		break;
	}
}
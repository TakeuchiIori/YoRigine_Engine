#include "GameScene.h"

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

// C++
#include <cstdlib>
#include <ctime>

#ifdef USE_IMGUI
#include "imgui.h"
#endif
#include <Systems/UI/UIManager.h>

/// <summary>
/// 初期化処理
/// </summary>
void GameScene::Initialize() {
	srand(static_cast<unsigned int>(time(nullptr)));
	sceneCamera_ = cameraManager_.AddCamera();

	//------------------------------------------------------------
	// システム初期化
	//------------------------------------------------------------
	YoRigine::GameTime::Initailzie();
	sceneCamera_ = cameraManager_.AddCamera();
	YoRigine::JsonManager::SetCurrentScene("GameScene");
	YoRigine::CollisionManager::GetInstance()->Initialize();
	YoRigine::ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::ModelManipulator::GetInstance()->Initialize("GameScene");
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::GpuEmitManager::GetInstance()->Initialize(sceneCamera_.get());

	//------------------------------------------------------------
	// カメラ初期化
	//------------------------------------------------------------
	followCamera_.Initialize();
	debugCamera_.Initialize();
	splineCamera_.Initialize();
	topDownCamera_.Initialize();
	battleStartCamera_.Initialize();

	//------------------------------------------------------------
	// 共通システム初期化
	//------------------------------------------------------------

	// AttackData の JSON を読み込む
	AttackDatabase::LoadFromFile("Resources/Json/Combo/AttackData.json");
#ifdef USE_IMGUI
	// エディターを初期化
	attackEditor_ = std::make_unique<AttackDataEditor>();
	attackEditor_->SetFilePath("Resources/Json/Combo/AttackData.json");
	attackEditor_->SetAutoReload(true);  // 自動リロード有効
	attackEditor_->SetOpen(true);

	// リロードコールバックを設定
	attackEditor_->SetReloadCallback([this]() {
		// プレイヤーのコンボシステムをリロード
		if (sharedPlayer_) {
			sharedPlayer_->GetCombat()->GetCombo()->ReloadAttacks();
		}
		Logger("[GameScene] Attack data reloaded from editor!\n");
		});
#endif

	//------------------------------------------------------------
	// 共通オブジェクト
	//------------------------------------------------------------

	sharedPlayer_ = std::make_unique<Player>();
	sharedPlayer_->Initialize(sceneCamera_.get());
	sharedPlayer_->SetFollowCamera(&followCamera_);

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(sceneCamera_.get(), "Resources/DDS/vz_classic_cubemap_ue.dds");
	//Resources / DDS / vz_sinister_land_cubemap_ue.dds
	
	//------------------------------------------------------------
	// インゲーム用UI
	//------------------------------------------------------------
	gameUI_ = std::make_unique<GameUI>();
	gameUI_->Initialize();

	//------------------------------------------------------------
	// サブシーン管理初期化
	//------------------------------------------------------------
	subSceneManager_ = std::make_unique<SubSceneManager>();
	subSceneManager_->Initialize(sceneCamera_.get(), sharedPlayer_.get());

	// フィールドシーン登録
	auto fieldScene = std::make_unique<FieldScene>();
	fieldScene->Initialize(sceneCamera_.get(), sharedPlayer_.get());
	subSceneManager_->RegisterSubScene("Field", std::move(fieldScene));

	// バトルシーン登録
	auto battleScene = std::make_unique<BattleScene>();
	battleScene->Initialize(sceneCamera_.get(), sharedPlayer_.get());

	//------------------------------------------------------------
	// バトル終了後にフィールドへ戻すコールバック
	//------------------------------------------------------------
	battleScene->SetBattleEndCallback([this](FieldReturnData fieldData, BattleResult result, const BattleStats& stats) {
		FieldReturnData returnData;
		returnData.playerWon = (result == BattleResult::Victory);
		returnData.expGained = stats.totalExpGained;
		returnData.goldGained = stats.totalGaldGained;
		returnData.itemsGained = stats.droppedItems;
		returnData.defeatedEnemyGroup = fieldData.defeatedEnemyGroup;

		// フィールド復帰処理
		auto* field = dynamic_cast<FieldScene*>(subSceneManager_->GetScene("Field"));
		if (field) {
			SubSceneTransitionRequest request;
			request.type = SubSceneTransitionType::TO_FIELD;
			request.transitionData = new FieldReturnData(returnData);

			subSceneManager_->HandleTransitionRequest(request);
			subSceneManager_->SwitchToSceneWithFade("Field");
			field->HandleBattleReturn(returnData);
		}
		});

	subSceneManager_->RegisterSubScene("Battle", std::move(battleScene));

	// 初期サブシーンをフィールドに設定
	subSceneManager_->SwitchToScene("Field");

	//------------------------------------------------------------
	// エディター用GUI登録
	//------------------------------------------------------------
#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("カメラモード", [this]() { UpdateCameraMode(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("ライティング", [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("GpuParticle", [this]() { YoRigine::GpuEmitManager::GetInstance()->DrawImGui(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("プレイヤー攻撃エディター", [this]() {attackEditor_->DrawImGui(); }, "Game");
#endif
}

/// <summary>
/// 更新処理
/// </summary>
void GameScene::Update() {
	YoRigine::GameTime::Update();
	Player* player = sharedPlayer_.get();
#ifdef _DEBUG
	// デバッグカメラ切り替え
	if (YoRigine::Input::GetInstance()->TriggerKey(DIK_LCONTROL) ||
		YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::R_Stick)) {
		isDebugCamera_ = !isDebugCamera_;
	}
	// スペースで強制死亡
	if (YoRigine::Input::GetInstance()->TriggerKey(DIK_P)) {
		player->GetCombat()->ChangeState(CombatState::Dead);
		gameUI_->ShowGameOverWithFade(3.0f);
	}
#endif

	bool isPlayerDead = player->GetCombat()->IsDead();
	//------------------------------------------------------------
	// プレイヤーが死亡したときの処理
	//------------------------------------------------------------
	if (isPlayerDead != wasPlayerDead_) {
		if (isPlayerDead) {
			// 死亡した瞬間:フェード開始
			gameUI_->ShowGameOverWithFade(3.0f);
		} else {
			// 復活した瞬間:リセット
			gameUI_->ResetGameOver();
		}
		wasPlayerDead_ = isPlayerDead;
	}

	//------------------------------------------------------------
	// ゲームオーバー時の選択処理
	//------------------------------------------------------------
	if (isPlayerDead && gameUI_->IsFadeCompleted()) {
		if (gameUI_->IsRetryRequested()) {
			// リトライ:フィールドシーンをリセットして再開
			HandleRetry();
			gameUI_->ClearRequests();
		} else if (gameUI_->IsReturnToTitleRequested()) {
			// タイトルへ戻る
			HandleReturnToTitle();
			gameUI_->ClearRequests();
		}
	}

	// カメラモード同期
	if (subSceneManager_) {
		cameraMode_ = subSceneManager_->GetCameraMode();
	}

	// サブシーン更新
	if (subSceneManager_) {
		subSceneManager_->Update();
	}

	// 空と共通システム更新
	skyBox_->Update();
	UpdateCamera();
	YoRigine::ModelManipulator::GetInstance()->Update();
	gameUI_->Update();

	cameraManager_.UpdateAllCameras();
	YoRigine::CollisionManager::GetInstance()->Update();
	YoRigine::ParticleManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
	YoRigine::LightManager::GetInstance()->UpdateShadowMatrix(sceneCamera_.get());
	YoRigine::GpuEmitManager::GetInstance()->Update();
}

/// <summary>
/// 描画処理
/// </summary>
void GameScene::Draw() {
	//------------------------------------------------------------
	// 3Dオブジェクト描画
	//------------------------------------------------------------
	skyBox_->Draw();
	Object3dCommon::GetInstance()->DrawPreference();
	YoRigine::LightManager::GetInstance()->SetCommandList();
	DrawObject();
	YoRigine::ModelManipulator::GetInstance()->Draw();

	//------------------------------------------------------------
	// パーティクル描画
	//------------------------------------------------------------
	YoRigine::ParticleManager::GetInstance()->Draw();
	DrawLine();
	YoRigine::GpuEmitManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// UI描画
	//------------------------------------------------------------
	SpriteCommon::GetInstance()->DrawPreference();
	DrawUI();
}

/// <summary>
/// オフスクリーン対象外のUI描画
/// </summary>
void GameScene::DrawNonOffscreen() {
	SpriteCommon::GetInstance()->DrawPreference();
	if (subSceneManager_) {
		subSceneManager_->DrawNonOffscreen();
	}
}

/// <summary>
/// 影の描画
/// </summary>
void GameScene::DrawShadow()
{
	if (subSceneManager_) {
		subSceneManager_->DrawShadow();
	}
}

/// <summary>
/// サブシーンのオブジェクト描画
/// </summary>
void GameScene::DrawObject() {
	if (subSceneManager_) {
		subSceneManager_->DrawObject();
	}
	splineCamera_.Draw(sceneCamera_.get());
}

/// <summary>
/// サブシーンのライン描画
/// </summary>
void GameScene::DrawLine() {
	if (subSceneManager_) {
		subSceneManager_->DrawLine();
	}
}

/// <summary>
/// UI描画（現在空）
/// </summary>
void GameScene::DrawUI() {
	gameUI_->DrawAll();
}

/// <summary>
/// カメラ更新処理
/// </summary>
void GameScene::UpdateCamera() {
	Player* player = sharedPlayer_.get();

	switch (cameraMode_) {
	case CameraMode::DEFAULT:
		sceneCamera_->DefaultCamera();
		break;

	case CameraMode::FOLLOW:
		if (player) {
			followCamera_.SetTarget(player->GetWT());
			followCamera_.SetIsCloseUp(player->GetCombat()->IsDead());
		}
		followCamera_.Update();
		sceneCamera_->viewMatrix_ = followCamera_.matView_;
		sceneCamera_->transform_.translate = followCamera_.translate_;
		sceneCamera_->transform_.rotate = followCamera_.rotate_;
		sceneCamera_->UpdateMatrix();
		break;

	case CameraMode::TOP_DOWN:
		if (player) topDownCamera_.SetTarget(player->GetWT());
		topDownCamera_.Update();
		sceneCamera_->viewMatrix_ = topDownCamera_.matView_;
		sceneCamera_->transform_.translate = topDownCamera_.translate_;
		sceneCamera_->transform_.rotate = topDownCamera_.rotate_;
		sceneCamera_->UpdateMatrix();
		break;

	case CameraMode::SPLINE:
		if (player) splineCamera_.SetTarget(player->GetWT());
		splineCamera_.RegisterControlPoints();
		splineCamera_.Update();
		sceneCamera_->viewMatrix_ = splineCamera_.matView_;
		sceneCamera_->transform_.translate = splineCamera_.translate_;
		sceneCamera_->transform_.rotate = splineCamera_.rotate_;
		sceneCamera_->UpdateMatrix();
		break;

	case CameraMode::BATTLE_START:
		//------------------------------------------------------------
		// バトルシーン進入時のカメラリセット
		//------------------------------------------------------------
		if (subSceneManager_->GetCurrentSceneName() == "Battle") {
			auto* battleScene = dynamic_cast<BattleScene*>(subSceneManager_->GetScene("Battle"));
			if (battleScene && battleScene->ShouldResetBattleCamera()) {
				battleStartCamera_.Initialize();
				battleScene->ClearBattleCameraResetFlag();
			}
		}

		if (player) battleStartCamera_.SetTarget(&player->GetWT());
		battleStartCamera_.Update();

		sceneCamera_->viewMatrix_ = battleStartCamera_.matView_;
		sceneCamera_->transform_.translate = battleStartCamera_.translate_;
		sceneCamera_->transform_.rotate = battleStartCamera_.rotate_;
		sceneCamera_->UpdateMatrix();

		// バトルカメラ終了通知
		if (battleStartCamera_.IsFinished()) {
			auto* battleScene = dynamic_cast<BattleScene*>(subSceneManager_->GetScene("Battle"));
			if (battleScene) battleScene->SetBattleCameraFinished(true);
		}
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

/// <summary>
/// カメラモード切り替え（ImGui）
/// </summary>
void GameScene::UpdateCameraMode() {
#ifdef USE_IMGUI
	if (ImGui::Button("DEFAULT Camera")) cameraMode_ = CameraMode::DEFAULT;
	if (ImGui::Button("Follow Camera")) cameraMode_ = CameraMode::FOLLOW;
	if (ImGui::Button("Top-Down Camera")) cameraMode_ = CameraMode::TOP_DOWN;
	if (ImGui::Button("Spline Camera")) cameraMode_ = CameraMode::SPLINE;
	if (ImGui::Button("Battle Start Camera")) cameraMode_ = CameraMode::BATTLE_START;
	if (ImGui::Button("Debug Camera")) cameraMode_ = CameraMode::DEBUG;

	if (subSceneManager_) subSceneManager_->SetCameraMode(cameraMode_);
#endif
}

/// <summary>
/// リトライ処理
/// </summary>
void GameScene::HandleRetry() {
	Logger("[GameScene] Retry requested - Restarting Field Scene\n");

	// プレイヤーをリセット
	if (sharedPlayer_) {
		sharedPlayer_->Reset();
	}

	// ゲームオーバーUIをリセット
	gameUI_->ResetGameOver();
	wasPlayerDead_ = false;

	// フィールドシーンを再初期化
	if (subSceneManager_) {
		auto* fieldScene = dynamic_cast<FieldScene*>(subSceneManager_->GetScene("Field"));
		if (fieldScene) {
			// フィールドシーンを再初期化
			fieldScene->Initialize(sceneCamera_.get(), sharedPlayer_.get());
		}

		// フィールドシーンに切り替え
		subSceneManager_->SwitchToSceneWithFade("Field");
	}

	// カメラをフォローカメラに戻す
	cameraMode_ = CameraMode::FOLLOW;

	// ゲーム時間を再開
	YoRigine::GameTime::Resume();

	Logger("[GameScene] Field Scene restarted\n");
}

/// <summary>
/// タイトルへ戻る処理
/// </summary>
void GameScene::HandleReturnToTitle() {
	Logger("[GameScene] Return to Title requested\n");

	// タイトルシーンへ遷移
	SceneManager::GetInstance()->ChangeScene("Title");

	Logger("[GameScene] Changing to Title Scene\n");
}

/// <summary>
/// ゲームクリア処理
/// </summary>
void GameScene::HandleGameClear() {
	Logger("[GameScene] Game Clear! All enemies defeated!\n");

	// クリアシーンへ遷移
	SceneManager::GetInstance()->ChangeScene("Clear");

	Logger("[GameScene] Changing to Clear Scene\n");
}

/// <summary>
/// 終了処理
/// </summary>
void GameScene::Finalize() {
	YoRigine::JsonManager::ClearSceneInstances("GameScene");
	cameraManager_.RemoveCamera(sceneCamera_);
	if (subSceneManager_) subSceneManager_->Finalize();
}
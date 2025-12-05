#include "FieldScene.h"
#include "SceneSyncData.h"

// Engine
#include "Systems./Input./Input.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include "Debugger/Logger.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// フィールドシーン初期化
/// </summary>
void FieldScene::Initialize(Camera* camera, Player* player) {

	sceneCamera_ = camera;
	player_ = player;
	player_->Reset();

	//------------------------------------------------------------
	// オブジェクト初期化
	//------------------------------------------------------------
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_);

	testjson_ = std::make_unique<LevelDataLoader>();
	testjson_->Initialize();

	emitter_ = std::make_unique<ParticleEmitter>("TestParticle", Vector3{ 0.0f, 0.0f, 0.0f }, 5);

	line_ = std::make_unique<Line>();
	line_->Initialize();
	line_->SetCamera(sceneCamera_);

	//------------------------------------------------------------
	// フィールド敵管理システム初期化
	//------------------------------------------------------------
	fieldEnemyManager_ = std::make_unique<FieldEnemyManager>();
	fieldEnemyManager_->Initialize(sceneCamera_);
	fieldEnemyManager_->SetPlayer(player_);

	// 敵エンカウント時の詳細コールバック登録
	fieldEnemyManager_->SetEncounterDetailCallback([this](const EncountInfo& encounterInfo) {
		HandleDetailedEncounter(encounterInfo);
		});

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize("Resources/Textures/GameScene/FieldScene.png");

#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("フィールドモード:デバッグ情報",
		[this]() { fieldEnemyManager_->ShowDebugInfo(); }, "Game");
#endif
}

/// <summary>
/// 更新処理
/// </summary>
void FieldScene::Update() {
	player_->Update();
	ground_->Update();
	fieldEnemyManager_->Update();
	testjson_->Update();
	//sprite_->Update();




	//------------------------------------------------------------
	// ログの出力用
	//------------------------------------------------------------
#ifdef _DEBUG
	// 変化検出用
	static bool prevFinalBattle = false;
	size_t remainingGroups = fieldEnemyManager_->GetActiveEncounterGroupCount();
	// 現在の判定
	bool isFinalBattle = (remainingGroups <= 1);
	// 状態が変わった時だけログを出す
	if (isFinalBattle != prevFinalBattle) {
		if (isFinalBattle) {
			Logger("[FieldScene] これは最後のバトルです！\n");
		} else {
			Logger("[FieldScene] まだ最後のバトルではありません。\n");
		}
	}
	prevFinalBattle = isFinalBattle;
#endif // _DEBUG
}

/// <summary>
/// 3Dオブジェクト描画
/// </summary>
void FieldScene::DrawObject() {
	player_->Draw();
	ground_->Draw();
	fieldEnemyManager_->Draw();
	player_->DrawAnimation();
}

/// <summary>
/// 当たり判定ラインやボーン描画
/// </summary>
void FieldScene::DrawLine() {
#ifdef USE_IMGUI
	player_->DrawCollision();
	fieldEnemyManager_->DrawCollision();
	player_->DrawBone(*line_.get());
#endif
}

/// <summary>
/// UI描画
/// </summary>
void FieldScene::DrawUI() {
	//sprite_->Draw();
}

void FieldScene::DrawNonOffscreen()
{
}

void FieldScene::DrawShadow()
{
	player_->DrawShadow();
	fieldEnemyManager_->DrawShadow();
}

/// <summary>
/// シーン遷移時に呼ばれる：フィールド開始処理
/// </summary>
void FieldScene::OnEnter() {
	BaseSubScene::OnEnter();

	Logger("[FieldScene] ===== OnEnter() START =====\n");

	// フィールドの敵を再開
	if (fieldEnemyManager_) {
		fieldEnemyManager_->SetAllEnemiesActive(true);
		fieldEnemyManager_->ResetEnCount();
		currentCameraMode_ = CameraMode::FOLLOW;
	}

	//------------------------------------------------------------
	// バトル終了後の復帰処理
	//------------------------------------------------------------
	auto* syncData = SceneSyncData::GetInstance();
	if (syncData->HasFieldReturnData()) {
		FieldReturnData returnData = syncData->LoadFieldReturnData();
		HandleBattleReturn(returnData);
		syncData->ClearFieldReturnData();
		Logger("[FieldScene] Field return data applied and cleared\n");
	} else {
		Logger("[FieldScene] No field return data (first time or fresh start)\n");
	}

	Logger("[FieldScene] ===== OnEnter() END =====\n");
}

/// <summary>
/// シーンを抜ける時の処理（バトル遷移前の保存）
/// </summary>
void FieldScene::OnExit() {
	BaseSubScene::OnExit();

	Logger("[FieldScene] ===== OnExit() START =====\n");

	auto* syncData = SceneSyncData::GetInstance();
	nlohmann::json fieldState;

	//------------------------------------------------------------
	// プレイヤー・カメラ状態の保存
	//------------------------------------------------------------
	if (player_) {
		Vector3 playerPos = player_->GetWorldPosition();
		fieldState["playerPosition"] = { {"x", playerPos.x}, {"y", playerPos.y}, {"z", playerPos.z} };
	}

	if (sceneCamera_) {
		Vector3 cameraPos = sceneCamera_->transform_.translate;
		fieldState["cameraPosition"] = { {"x", cameraPos.x}, {"y", cameraPos.y}, {"z", cameraPos.z} };
	}

	fieldState["cameraMode"] = static_cast<int>(currentCameraMode_);

	//------------------------------------------------------------
	// 現在アクティブな敵情報を保存
	//------------------------------------------------------------
	if (fieldEnemyManager_) {
		auto activeEnemies = fieldEnemyManager_->GetActiveFieldEnemies();
		nlohmann::json enemyList = nlohmann::json::array();
		for (auto* enemy : activeEnemies) {
			Vector3 pos = enemy->GetPosition();
			enemyList.push_back({
				{"id", enemy->GetEnemyGroupName()},
				{"position", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}}
				});
		}
		fieldState["activeEnemies"] = enemyList;
	}

	syncData->SaveCurrentSceneState("Field", fieldState);
	Logger("[FieldScene] Field state saved to JSON\n");
	Logger("[FieldScene] ===== OnExit() END =====\n");
}

/// <summary>
/// カメラモードの切り替え（デバッグ用）
/// </summary>
void FieldScene::UpdateCameraMode() {
#ifdef USE_IMGUI
	if (ImGui::Button("Follow Camera")) { currentCameraMode_ = CameraMode::FOLLOW; }
	if (ImGui::Button("Top-Down Camera")) { currentCameraMode_ = CameraMode::TOP_DOWN; }
	if (ImGui::Button("Spline Camera")) { currentCameraMode_ = CameraMode::SPLINE; }
	if (ImGui::Button("Debug Camera")) { currentCameraMode_ = CameraMode::DEBUG; }
#endif
}

/// <summary>
/// エンカウント発生時の詳細処理
/// </summary>
void FieldScene::HandleDetailedEncounter(const EncountInfo& encounterInfo) {
	Logger("[FieldScene] ===== 詳細エンカウント処理 START =====\n");

	//------------------------------------------------------------
	// バトル遷移データ作成
	//------------------------------------------------------------
	BattleTransitionData transitionData;
	transitionData.enemyGroup = encounterInfo.enemyGroup;
	transitionData.battleEnemyId = encounterInfo.battleEnemyId;
	transitionData.playerPosition = GetPlayerPosition();

	SaveCameraState(transitionData);

	// これが最後の敵かどうかをチェック
	bool isFinalBattle = false;
	size_t remainingGroups = 0;

	if (fieldEnemyManager_) {

		// 改善: エンカウント「グループ数」で判定（敵の個体数ではなく）
		remainingGroups = fieldEnemyManager_->GetActiveEncounterGroupCount();

		char debugBuffer[256];
		sprintf_s(debugBuffer, "[FieldScene] 残りのエンカウントグループ数: %zu\n", remainingGroups);
		Logger(debugBuffer);

		// 残りのグループが 1（=今戦うグループのみ）なら最終戦
		if (remainingGroups <= 1) {
			isFinalBattle = true;
			Logger("[FieldScene] ★★★ 最終エンカウントグループです！ ★★★\n");
		} else {
			sprintf_s(debugBuffer, "[FieldScene] まだ最終戦ではありません。残りグループ数: %zu\n", remainingGroups - 1);
			Logger(debugBuffer);
		}
	} else {
		Logger("[FieldScene] エラー: fieldEnemyManager_ が null です！\n");
	}

	transitionData.isFinalBattle = isFinalBattle;
	transitionData.totalRemainingFieldEnemies = remainingGroups; // ★追加

	auto* syncData = SceneSyncData::GetInstance();
	syncData->SaveBattleTransitionData(transitionData);

#ifdef _DEBUG
	char buffer[512];
	sprintf_s(buffer,
		"[FieldScene] バトル遷移データを保存しました - EnemyGroup: %s, BattleEnemyId: %s, 最終戦: %s\n",
		encounterInfo.enemyGroup.c_str(),
		encounterInfo.battleEnemyId.c_str(),
		isFinalBattle ? "はい" : "いいえ"
	);
	Logger(buffer);
#endif // _DEBUG

	// SubSceneManager へバトル遷移リクエスト
	RequestBattleTransition(transitionData);

#ifdef _DEBUG
	Logger("[FieldScene] ===== 詳細エンカウント処理 END =====\n");
	// 残りの敵グループ数をログ出力
	{
		size_t groups = fieldEnemyManager_->GetActiveEncounterGroupCount();
		char debugBuffer[256];
		sprintf_s(debugBuffer,
			"[FieldScene] 現在の残りエンカウントグループ数: %zu\n",
			groups);
		Logger(debugBuffer);
	}
#endif // _DEBUG
}


/// <summary>
/// カメラ状態の保存（バトル遷移用）
/// </summary>
void FieldScene::SaveCameraState(BattleTransitionData& data) {
	data.cameraPosition = sceneCamera_->transform_.translate;
	data.cameraMode = currentCameraMode_;
}

/// <summary>
/// バトル復帰時の処理（勝敗に応じた復元）
/// </summary>
void FieldScene::HandleBattleReturn(const FieldReturnData& data) {
	Logger("[FieldScene] ===== HandleBattleReturn() START =====\n");

	Vector3 returnPos = data.playerPosition;

	if (data.playerWon) {
		fieldEnemyManager_->RegisterDefeatedEnemy(data.defeatedEnemyGroup);
		char buffer[256];
		sprintf_s(buffer, "[FieldScene] Victory! Defeated enemy: %s\n", data.defeatedEnemyGroup.c_str());
		Logger(buffer);
	} else {
		returnPos += Vector3(0, 0, -2.0f);
		Logger("[FieldScene] Defeat! Player moved back\n");
	}

	player_->SetPosition(returnPos);

	if (data.expGained > 0 || data.goldGained > 0) {
		char buffer[256];
		sprintf_s(buffer, "[FieldScene] Battle rewards - EXP: %d, Gold: %d\n",
			data.expGained, data.goldGained);
		Logger(buffer);
	}

	fieldEnemyManager_->HandleBattleEnd(data.defeatedEnemyGroup, data.playerWon);

	Logger("[FieldScene] ===== HandleBattleReturn() END =====\n");
}

/// <summary>
/// カメラ状態の復元
/// </summary>
void FieldScene::RestoreCameraState(const FieldReturnData& data) {
	currentCameraMode_ = data.cameraMode;
}

/// <summary>
/// プレイヤー位置の取得
/// </summary>
Vector3 FieldScene::GetPlayerPosition() const {
	return player_ ? player_->GetWorldPosition() : Vector3(0.0f, 0.0f, 0.0f);
}

/// <summary>
/// すべての敵が撃破されたかチェック
/// </summary>
bool FieldScene::AreAllEnemiesDefeated() const {
	if (!fieldEnemyManager_) {
		return false;
	}

	// アクティブな敵を取得
	auto activeEnemies = fieldEnemyManager_->GetActiveFieldEnemies();

	// アクティブな敵がいる場合、まだクリアではない
	if (!activeEnemies.empty()) {
		return false;
	}

	// アクティブな敵がいない場合、
	// 撃破済み敵が1体以上いればクリア（敵が存在していた証拠）
	// 撃破済み敵がいなければ、まだ敵との戦闘が発生していない
	size_t totalEnemyCount = fieldEnemyManager_->GetActiveEnemyCount();

	// 初期配置された敵が全て撃破されたかチェック
	// （この判定はFieldEnemyManagerの初期スポーン数などで判定する必要がある）
	return totalEnemyCount == 0 && fieldEnemyManager_->HasAnyEnemiesBeenSpawned();
}

/// <summary>
/// 終了処理
/// </summary>
void FieldScene::Finalize() {
	if (fieldEnemyManager_) {
		fieldEnemyManager_->Finalize();
	}
}
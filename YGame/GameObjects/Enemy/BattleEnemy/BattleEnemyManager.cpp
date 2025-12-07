#include "BattleEnemyManager.h"
#include "Player/Player.h"

// C++
#include <algorithm>
#include <random>

// Engine
#include "Systems/GameTime/GameTime.h"
#include <Loaders/Json/JsonManager.h>
#include <Debugger/Logger.h>

// Math
#include "Vector3.h"
#include "MathFunc.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif
#include <Collision/AreaCollision/Base/AreaManager.h>
#include <SceneSystems/SceneManager.h>

/// <summary>
/// コンストラクタ
/// </summary>
BattleEnemyManager::BattleEnemyManager() = default;

/// <summary>
/// デストラクタ
/// </summary>
BattleEnemyManager::~BattleEnemyManager() = default;

/// <summary>
/// 敵マネージャーの初期化処理
/// </summary>
/// <param name="camera">使用するカメラのポインタ</param>
void BattleEnemyManager::Initialize(Camera* camera) {
	camera_ = camera;
	battleEnemies_.clear();
	enemyDataMap_.clear();
	encounterDataMap_.clear();
	formationMap_.clear();

	// 戦闘状態リセット
	isBattleActive_ = false;
	isBattlePaused_ = false;
	battleResult_ = BattleResult::None;
	battleTimer_ = 0.0f;
	currentEncounterName_.clear();

	// 統計リセット
	ResetBattleStats();
	// デフォルトフォーメーション設定
	LoadDefaultFormations();
	// 敵データの読み込み
	LoadEnemyData(enemyDataFilePath_);
}

/// <summary>
/// 戦闘中の全体更新処理
/// </summary>
void BattleEnemyManager::Update() {

	if (!isBattleActive_ || isBattlePaused_) return;

	float deltaTime = YoRigine::GameTime::GetDeltaTime();

	// 戦闘タイマー更新
	UpdateBattleTimer();

	// 各敵の更新
	UpdateBattleState();

	// 一定間隔でAIを更新
	aiUpdateTimer_ += deltaTime;
	if (aiUpdateTimer_ >= aiUpdateInterval_) {
		ProcessEnemyAI();
		aiUpdateTimer_ = 0.0f;
	}

	// 重要: 倒された敵を先に削除（OnEnemyDefeatedが先に実行される）
	CleanupDefeatedEnemies();

	// 終了条件チェック（OnEnemyDefeated後に実行）
	CheckBattleEndConditions();

	// 統計更新
	UpdateBattleStats();
}
/// <summary>
/// 各敵の状態を更新
/// </summary>
void BattleEnemyManager::UpdateBattleState() {
	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			enemy->Update();
		}
	}
}

/// <summary>
/// 戦闘タイマーを更新
/// </summary>
void BattleEnemyManager::UpdateBattleTimer() {
	if (isBattleActive_) {
		battleTimer_ += YoRigine::GameTime::GetDeltaTime();
		battleStats_.battleDuration = battleTimer_;
	}
}

/// <summary>
/// 敵AIの更新処理
/// </summary>
void BattleEnemyManager::ProcessEnemyAI() {
	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			// プレイヤーの位置を敵に通知
			if (player_) {
				// enemy->SetTargetPosition(player_->GetWorldPosition());
			}
			// AI処理は各敵のUpdate内で実行
		}
	}
}

/// <summary>
/// 戦闘終了条件を確認し、該当すれば終了処理を行う
/// </summary>
void BattleEnemyManager::CheckBattleEndConditions() {
	if (battleResult_ != BattleResult::None) return;

	// 最終バトルでクリアシーン遷移待ち中は終了処理をスキップ
	if (isWaitingForClearTransition_) {
		return;
	}

	// 全敵撃破チェック
	if (AreAllEnemiesDefeated()) {
		// 最終バトルの場合はOnEnemyDefeatedで既に処理済み
		if (isFinalBattle_) {
			Logger("[BattleEnemyManager] Final battle - victory handled in OnEnemyDefeated\n");
			return;
		}

		Logger("[BattleEnemyManager] 全敵撃破！勝利判定\n");
		EndBattle(BattleResult::Victory);
		return;
	}

	// プレイヤー敗北チェック
	if (IsPlayerDefeated()) {
		Logger("[BattleEnemyManager] プレイヤー敗北判定\n");
		EndBattle(BattleResult::Defeat);
		return;
	}

	// 他の終了条件があればここに追加
}

/// <summary>
/// 戦闘統計データを更新
/// </summary>
void BattleEnemyManager::UpdateBattleStats() {
	// 統計の更新処理
	// （倒された敵数などは OnEnemyDefeated 内で処理）
}

/// <summary>
/// 倒された敵を削除し、統計を更新
/// </summary>
void BattleEnemyManager::CleanupDefeatedEnemies() {
	battleEnemies_.erase(
		std::remove_if(battleEnemies_.begin(), battleEnemies_.end(),
			[this](const std::unique_ptr<BattleEnemy>& enemy) {
				if (enemy && !enemy->IsAlive()) {
					OnEnemyDefeated(enemy.get());
					return true;
				}
				return false;
			}),
		battleEnemies_.end()
	);
}

/// <summary>
/// 敵撃破時の処理（統計更新）
/// </summary>
/// <param name="enemy">撃破された敵</param>
void BattleEnemyManager::OnEnemyDefeated(BattleEnemy* enemy) {
	if (!enemy) return;

	const auto& enemyData = enemy->GetEnemyData();
	Logger("[BattleEnemyManager] 敵撃破: " + enemyData.enemyId + "\n");

	// 統計更新
	battleStats_.enemiesDefeated++;

	// 最終バトル時：残りの生存敵をカウント
	if (isFinalBattle_) {
		size_t aliveCount = 0;
		for (const auto& e : battleEnemies_) {
			if (e && e->IsAlive()) {
				aliveCount++;
			}
		}

		char debugBuffer[256];
		sprintf_s(debugBuffer, "[BattleEnemyManager] Final Battle - Remaining alive enemies: %zu\n", aliveCount);
		Logger(debugBuffer);

		// 生存敵が0になった = 全敵撃破！
		if (aliveCount == 0) {
			Logger("[BattleEnemyManager] ★★★ FINAL ENEMY DEFEATED! ★★★\n");

			// スローモーション演出開始
			// 最終バトルクリアフラグを立てる
			isFinalBattleCleared_ = true;

			// n秒後にクリアシーンへ遷移
			finalBattleSlowTimer_ = 0.0f;
			isWaitingForClearTransition_ = true;

			Logger("[BattleEnemyManager] Slow motion started, transitioning to Clear in 1 sec\n");
		}
	}
}

/// <summary>
/// 戦闘を開始（エンカウント名を指定）
/// </summary>
/// <param name="encounterName">エンカウント名</param>
void BattleEnemyManager::StartBattle(const std::string& encounterName) {
	auto it = encounterDataMap_.find(encounterName);
	if (it != encounterDataMap_.end()) {
		StartBattle(it->second);
	} else {
		Logger("[BattleEnemyManager] エラー: エンカウント名が見つかりません: " + encounterName + "\n");
	}
}

/// <summary>
/// 戦闘を開始（エンカウンターデータを指定）
/// </summary>
/// <param name="encounterData">エンカウンターデータ構造体</param>
void BattleEnemyManager::StartBattle(const EnemyEncounterData& encounterData) {
	// 既に戦闘中ならリセット
	if (isBattleActive_) {
		Logger("[BattleEnemyManager] 前回の戦闘が継続中、敵をクリア\n");
		RemoveAllBattleEnemies();
		EndBattle(BattleResult::None);
	}

	// 念のため、戦闘開始前に敵をクリア
	if (!battleEnemies_.empty()) {
		Logger("[BattleEnemyManager] 新規戦闘前に既存敵をクリア\n");
		RemoveAllBattleEnemies();
	}

	// データ設定
	currentEncounter_ = encounterData;
	currentEncounterName_ = encounterData.encounterName;

	// 状態初期化
	isBattleActive_ = true;
	isBattlePaused_ = false;
	battleResult_ = BattleResult::None;
	battleTimer_ = 0.0f;

	// 統計リセット
	ResetBattleStats();

	Logger("[BattleEnemyManager] 戦闘開始: " + encounterData.encounterName +
		" 敵数: " + std::to_string(encounterData.enemyIds.size()) + "\n");

	// 敵を生成
	SpawnEnemyGroup(encounterData.enemyIds, encounterData.formations);

	Logger("[BattleEnemyManager] " + std::to_string(battleEnemies_.size()) + "体の敵を生成\n");

	// 全敵にプレイヤーをターゲットとして設定
	SetAllEnemiesTarget(player_);
}

/// <summary>
/// 戦闘を終了し、結果を記録・後処理を実行する
/// </summary>
/// <param name="result">戦闘結果（勝利・敗北・逃走など）</param>
void BattleEnemyManager::EndBattle(BattleResult result) {
	if (!isBattleActive_) return;

	// 最終バトル勝利時はスロー演出処理に任せる
	if (isFinalBattle_ && result == BattleResult::Victory) {
		Logger("[BattleEnemyManager] Final battle victory - handled by slow motion transition\n");
		return;  // コールバックを呼ばずに終了
	}

	battleResult_ = result;
	isBattleActive_ = false;

	const char* resultStr = (result == BattleResult::Victory) ? "勝利" :
		(result == BattleResult::Defeat) ? "敗北" :
		(result == BattleResult::Escape) ? "逃走" : "不明";

	Logger("[BattleEnemyManager] 戦闘終了: " + std::string(resultStr) + "\n");

	// 最終統計計算
	CalculateBattleRewards();

	// 戦闘ログをファイルに保存
	//std::string logFileName = "Resources/Logs/battle_" + currentEncounterName_ + "_" +
	//	std::to_string(static_cast<int>(GameTime::GetTotalTime())) + ".log";
	//SaveBattleLog(logFileName);

	// コールバック実行
	if (battleEndCallback_) {
		battleEndCallback_(result, battleStats_);
	}

	// 戦闘後の後処理
	if (result == BattleResult::Victory || result == BattleResult::Defeat) {
		// 少し待ってから敵を削除
		RemoveAllBattleEnemies();
	}
}

/// <summary>
/// 強制的に戦闘を終了する（結果なし）
/// </summary>
void BattleEnemyManager::ForceBattleEnd() {
	if (isBattleActive_) {
		Logger("[BattleEnemyManager] 戦闘を強制終了\n");
		EndBattle(BattleResult::None);
		RemoveAllBattleEnemies();
	}
}


/// <summary>
/// 指定した敵IDと位置から敵を生成
/// </summary>
/// <param name="enemyId">敵ID</param>
/// <param name="position">生成位置</param>
void BattleEnemyManager::SpawnBattleEnemy(const std::string& enemyId, const Vector3& position) {
	if (!camera_) {
		ThrowError("[BattleEnemyManager] エラー: カメラが設定されていません\n");
		return;
	}

	// enemyDataMap_ に敵IDが存在するかチェックする
	auto it = enemyDataMap_.find(enemyId);
	if (it == enemyDataMap_.end()) {
		// データが事前ロードされていない場合はエラーとし、生成を中断する
		ThrowError(("[BattleEnemyManager] エラー: 敵データID \"" + enemyId + "\" がキャッシュにありません。LoadEnemyData()を事前に実行してください。\n").c_str());
		return;
	}

	// キャッシュからデータを取得する
	const BattleEnemyData& enemyData = it->second;
	Logger(("[BattleEnemyManager] キャッシュから敵データ取得: " + enemyId + "\n").c_str());

	// 敵オブジェクトの生成と初期化
	auto newEnemy = std::make_unique<BattleEnemy>();
	newEnemy->Initialize(camera_);
	newEnemy->SetPlayer(player_);

	// キャッシュから取得したデータと位置情報で初期化
	newEnemy->InitializeBattleData(enemyData, position);

	// エリアマネージャーに登録
	AreaManager::GetInstance()->RegisterObject(&newEnemy->GetWT(), ("Enemy_" + enemyId).c_str());
	battleEnemies_.push_back(std::move(newEnemy));

	Logger(("[BattleEnemyManager] 敵を生成: " + enemyId + " 位置: (" +
		std::to_string(position.x) + ", " + std::to_string(position.y) + ", " +
		std::to_string(position.z) + ") 合計: " + std::to_string(battleEnemies_.size()) + "体\n").c_str());
}
/// <summary>
/// 敵グループを生成
/// </summary>
/// <param name="enemyIds">生成する敵IDのリスト</param>
/// <param name="positions">配置位置リスト</param>
void BattleEnemyManager::SpawnEnemyGroup(const std::vector<std::string>& enemyIds, const std::vector<Vector3>& positions) {
	size_t enemyCount = enemyIds.size();

	Logger("[BattleEnemyManager] 敵グループ生成開始: " + std::to_string(enemyCount) + "体\n");

	for (size_t i = 0; i < enemyCount; ++i) {
		Vector3 spawnPos;

		if (i < positions.size()) {
			spawnPos = positions[i];
		} else {
			spawnPos = GetDefaultFormationPosition(i, enemyCount);
		}

		SpawnBattleEnemy(enemyIds[i], spawnPos);
	}
}

/// <summary>
/// 全ての戦闘中の敵を削除
/// </summary>
void BattleEnemyManager::RemoveAllBattleEnemies() {
	size_t count = battleEnemies_.size();
	auto* areaManager = AreaManager::GetInstance();

	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			// エリアの登録解除
			areaManager->UnregisterObject(&enemy->GetWT());
		}
	}

	if (count > 0) {
		Logger("[BattleEnemyManager] 全ての敵を削除: " + std::to_string(count) + "体\n");
	}

	battleEnemies_.clear();
}

/// <summary>
/// 全ての敵が撃破されているか確認
/// </summary>
/// <returns>全敵が倒されていれば true</returns>
bool BattleEnemyManager::AreAllEnemiesDefeated() const {
	return std::all_of(battleEnemies_.begin(), battleEnemies_.end(),
		[](const std::unique_ptr<BattleEnemy>& enemy) {
			return !enemy || !enemy->IsAlive();
		});
}

/// <summary>
/// プレイヤーが敗北状態か確認
/// </summary>
/// <returns>敗北状態なら true</returns>
bool BattleEnemyManager::IsPlayerDefeated() const {
	if (player_) {
		// TODO: 実際のHPチェック実装予定
		// return player_->GetCurrentHP() <= 0;
		return false;
	}
	return false;
}

/// <summary>
/// 全ての敵にプレイヤーをターゲットとして設定
/// </summary>
/// <param name="player">ターゲットに設定するプレイヤー</param>
void BattleEnemyManager::SetAllEnemiesTarget(Player* player) {
	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			enemy->SetPlayer(player);
		}
	}
}

/// <summary>
/// 全ての敵をスタン状態にする
/// </summary>
/// <param name="duration">スタンの継続時間</param>
void BattleEnemyManager::StunAllEnemies([[maybe_unused]] float duration) {
	Logger("[BattleEnemyManager] 全敵スタン: " + std::to_string(duration) + "秒\n");
}

/// <summary>
/// 全ての敵にダメージを与える
/// </summary>
/// <param name="damage">与えるダメージ量</param>
void BattleEnemyManager::DamageAllEnemies(int damage) {
	Logger("[BattleEnemyManager] 全敵にダメージ: " + std::to_string(damage) + "\n");

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			enemy->TakeDamage(damage);
		}
	}
}

/// <summary>
/// アクティブな敵を取得
/// </summary>
/// <returns>生存中の敵リスト</returns>
std::vector<BattleEnemy*> BattleEnemyManager::GetActiveBattleEnemies() {
	std::vector<BattleEnemy*> result;

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			result.push_back(enemy.get());
		}
	}
	return result;
}

/// <summary>
/// 指定範囲内の敵を取得
/// </summary>
/// <param name="center">中心座標</param>
/// <param name="range">範囲距離</param>
/// <returns>範囲内にいる敵リスト</returns>
std::vector<BattleEnemy*> BattleEnemyManager::GetEnemiesInRange(const Vector3& center, float range) {
	std::vector<BattleEnemy*> result;

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			float distance = Length(enemy->GetTranslate() - center);
			if (distance <= range) {
				result.push_back(enemy.get());
			}
		}
	}
	return result;
}

/// <summary>
/// 最も近い敵を取得
/// </summary>
/// <param name="position">基準位置</param>
/// <returns>最も近い敵へのポインタ</returns>
BattleEnemy* BattleEnemyManager::GetNearestEnemy(const Vector3& position) {
	BattleEnemy* nearest = nullptr;
	float minDistance = FLT_MAX;

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			float distance = Length(enemy->GetTranslate() - position);
			if (distance < minDistance) {
				minDistance = distance;
				nearest = enemy.get();
			}
		}
	}
	return nearest;
}

/// <summary>
/// 敵IDから敵を取得
/// </summary>
/// <param name="id">敵ID</param>
/// <returns>該当する敵へのポインタ（見つからない場合nullptr）</returns>
BattleEnemy* BattleEnemyManager::GetEnemyById(const std::string& id) {
	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->GetEnemyData().enemyId == id) {
			return enemy.get();
		}
	}
	return nullptr;
}

/// <summary>
/// 生存している敵の数を取得
/// </summary>
/// <returns>アクティブな敵数</returns>
size_t BattleEnemyManager::GetActiveEnemyCount() const {
	return std::count_if(battleEnemies_.begin(), battleEnemies_.end(),
		[](const std::unique_ptr<BattleEnemy>& enemy) {
			return enemy && enemy->IsAlive();
		});
}

/// <summary>
/// デフォルトフォーメーションを設定
/// </summary>
void BattleEnemyManager::LoadDefaultFormations() {
	BattleFormationData single;
	single.formationName = "single";
	single.description = "単体敵用の中央配置";
	single.positions = { Vector3(0.0f, 0.0f, 5.0f) };
	formationMap_["single"] = single;

	BattleFormationData dual;
	dual.formationName = "dual";
	dual.description = "2体の敵を左右に配置";
	dual.positions = {
		Vector3(-2.0f, 0.0f, 5.0f),
		Vector3(2.0f, 0.0f, 5.0f)
	};
	formationMap_["dual"] = dual;

	BattleFormationData triple;
	triple.formationName = "triple";
	triple.description = "3体の敵を横一列に配置";
	triple.positions = {
		Vector3(-3.0f, 0.0f, 5.0f),
		Vector3(0.0f, 0.0f, 5.0f),
		Vector3(3.0f, 0.0f, 5.0f)
	};
	formationMap_["triple"] = triple;

	BattleFormationData quad;
	quad.formationName = "quad";
	quad.description = "4体の敵を2x2で配置";
	quad.positions = {
		Vector3(-2.0f, 0.0f, 4.0f),
		Vector3(2.0f, 0.0f, 4.0f),
		Vector3(-2.0f, 0.0f, 6.0f),
		Vector3(2.0f, 0.0f, 6.0f)
	};
	formationMap_["quad"] = quad;

	Logger("[BattleEnemyManager] デフォルトフォーメーション読み込み完了\n");
}

/// <summary>
/// フォーメーションデータを外部ファイルから読み込む
/// </summary>
/// <param name="filePath">ファイルパス</param>
void BattleEnemyManager::LoadFormations([[maybe_unused]] const std::string& filePath) {
	Logger("[BattleEnemyManager] フォーメーションデータ読み込み: " + filePath + "\n");
}

/// <summary>
/// 現在使用するフォーメーションを設定
/// </summary>
/// <param name="formationName">フォーメーション名</param>
void BattleEnemyManager::SetFormation(const std::string& formationName) {
	currentFormation_ = formationName;
	Logger("[BattleEnemyManager] フォーメーション設定: " + formationName + "\n");
}

/// <summary>
/// 指定フォーメーション名に対応するデータを取得
/// </summary>
/// <param name="formationName">フォーメーション名</param>
/// <returns>フォーメーションデータ</returns>
BattleFormationData BattleEnemyManager::GetFormation(const std::string& formationName) const {
	auto it = formationMap_.find(formationName);
	if (it != formationMap_.end()) {
		return it->second;
	}
	Logger("[BattleEnemyManager] 警告: フォーメーション名が見つかりません: " + formationName + "\n");
	return BattleFormationData();
}

/// <summary>
/// 敵数に応じたフォーメーション位置を取得
/// </summary>
/// <param name="enemyCount">敵の数</param>
/// <returns>配置位置リスト</returns>
std::vector<Vector3> BattleEnemyManager::GetFormationPositions(size_t enemyCount) const {
	std::string formationName;

	switch (enemyCount) {
	case 1: formationName = "single"; break;
	case 2: formationName = "dual"; break;
	case 3: formationName = "triple"; break;
	case 4: formationName = "quad"; break;
	default: formationName = "single"; break;
	}

	auto it = formationMap_.find(formationName);
	if (it != formationMap_.end()) {
		return it->second.positions;
	}

	std::vector<Vector3> positions;
	for (size_t i = 0; i < enemyCount; ++i) {
		positions.push_back(GetDefaultFormationPosition(i, enemyCount));
	}
	return positions;
}
/// <summary>
/// デフォルトフォーメーション位置を取得
/// </summary>
/// <param name="index">インデックス</param>
/// <param name="totalCount">総数</param>
/// <returns>配置座標</returns>
Vector3 BattleEnemyManager::GetDefaultFormationPosition(size_t index, size_t totalCount) const {
	float spacing = 2.5f;
	float startX = -(spacing * (totalCount - 1)) / 2.0f;
	float x = startX + (spacing * index);
	return Vector3(x, 0.0f, 5.0f);
}

/// <summary>
/// 指定名のエンカウンターデータを取得
/// </summary>
/// <param name="encounterName">エンカウント名</param>
/// <returns>該当するデータ（なければ空データ）</returns>
EnemyEncounterData BattleEnemyManager::GetEncounterData(const std::string& encounterName) const {
	auto it = encounterDataMap_.find(encounterName);
	if (it != encounterDataMap_.end()) {
		return it->second;
	}
	Logger("[BattleEnemyManager] 警告: エンカウンターデータが見つかりません: " + encounterName + "\n");
	return EnemyEncounterData();
}

/// <summary>
/// 戦闘統計をリセット
/// </summary>
void BattleEnemyManager::ResetBattleStats() {
	battleStats_ = BattleStats();
}

/// <summary>
/// 戦闘報酬を計算
/// </summary>
void BattleEnemyManager::CalculateBattleRewards() {
	Logger("[BattleEnemyManager] 戦闘報酬計算\n");
	Logger("[BattleEnemyManager] 撃破数: " + std::to_string(battleStats_.enemiesDefeated) +
		" 経験値: " + std::to_string(battleStats_.totalExpGained) +
		" ゴールド: " + std::to_string(battleStats_.totalGaldGained) + "\n");
}

bool BattleEnemyManager::SaveEnemyData(const std::string& filePath) const {
	json j = json::object();
	json enemyArray = json::array();

	// enemyDataMap_ のデータをJSON配列に変換
	for (const auto& pair : enemyDataMap_) {
		const BattleEnemyData& data = pair.second;

		json enemyJson = {
			{"enemyId", data.enemyId},
			{"modelPath", data.modelPath},
			{"level", data.level},
			{"hp", data.hp}, // ベースHPを保存
			{"attack", data.attack},
			{"defense", data.defense},
			{"moveSpeed", data.moveSpeed},
			{"approachStateRange", data.approachStateRange},
			{"attackStateRange", data.attackStateRange},
			{"aiType", data.aiType},
			{"attackPatterns", data.attackPatterns}
			// currentHp_ と maxHp_ は実行時データのため保存しません
		};
		enemyArray.push_back(enemyJson);
	}

	j["battleEnemies"] = enemyArray;

	// ファイルに書き出し
	std::ofstream ofs(filePath);
	if (!ofs.is_open()) {
		ThrowError(("敵データ保存用のファイルを開けませんでした: " + filePath + "\n").c_str());
		return false;
	}

	try {
		// インデント4で整形して保存
		ofs << std::setw(4) << j;
		ofs.close();
		Logger((std::to_string(enemyDataMap_.size()) + "件の敵データを正常に保存しました。\n").c_str());
		return true;
	}
	catch (const std::exception& e) {
		Logger(("敵データの保存中にエラーが発生しました: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}
/// <summary>
/// 全ての敵のベースデータをJSONファイルから読み込み、キャッシュする
/// </summary>
bool BattleEnemyManager::LoadEnemyData(const std::string& filePath) {
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		ThrowError(("敵データファイルを開けませんでした: " + filePath + "\n").c_str());
		return false;
	}

	try {
		json j = json::parse(ifs);

		if (!j.contains("battleEnemies") || !j["battleEnemies"].is_array()) {
			ThrowError(("敵データファイルの形式が無効です: 'battleEnemies'配列が見つかりません。\n"));
			return false;
		}

		// 既存のデータをクリア
		enemyDataMap_.clear();

		for (const auto& enemyJson : j["battleEnemies"]) {
			if (!enemyJson.contains("enemyId")) {
				ThrowError("敵データエントリに'enemyId'がありません。スキップします。\n");
				continue;
			}

			BattleEnemyData data{};
			data.enemyId = enemyJson["enemyId"].get<std::string>();

			// JSONの値を取得。存在しない場合はデフォルト値を適用
			data.modelPath = enemyJson.value("modelPath", "default_enemy.obj");
			data.level = enemyJson.value("level", 1);
			data.hp = enemyJson.value("hp", 100);
			data.attack = enemyJson.value("attack", 15);
			data.defense = enemyJson.value("defense", 10);
			data.moveSpeed = enemyJson.value("moveSpeed", 5.0f);
			data.approachStateRange = enemyJson.value("approachStateRange", 15.0f);
			data.attackStateRange = enemyJson.value("attackStateRange", 10.0f);
			data.aiType = enemyJson.value("aiType", "aggressive");

			// 攻撃パターンを読み込み
			data.attackPatterns.clear();
			if (enemyJson.contains("attackPatterns") && enemyJson["attackPatterns"].is_array()) {
				for (const auto& pattern : enemyJson["attackPatterns"]) {
					data.attackPatterns.push_back(pattern.get<std::string>());
				}
			}

			// マップにデータを格納
			enemyDataMap_[data.enemyId] = data;
		}
		Logger((std::to_string(enemyDataMap_.size()) + "件の敵データを正常に読み込み、キャッシュしました。\n").c_str());
		return true;

	}
	catch (const json::parse_error& e) {
		ThrowError(("敵データファイル内でJSON解析エラーが発生しました: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
	catch (const std::exception& e) {
		ThrowError(("敵データの読み込み中にエラーが発生しました: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}

/// <summary>
/// デバッグ用の敵生成
/// </summary>
/// <param name="position">生成位置</param>
/// <param name="enemyId">敵ID</param>
void BattleEnemyManager::DebugSpawnEnemy(const Vector3& position, const std::string& enemyId) {
	Logger("[BattleEnemyManager] デバッグ: 敵生成 " + enemyId + "\n");
	SpawnBattleEnemy(enemyId, position);
}

/// <summary>
/// 敵の描画処理
/// </summary>
void BattleEnemyManager::Draw() {
	if (!isBattleActive_) return;

	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			enemy->Draw();
		}
	}
}

void BattleEnemyManager::DrawShadow()
{
	if (!isBattleActive_) return;
	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			enemy->DrawShadow();
		}
	}
}

/// <summary>
/// 当たり判定の描画（デバッグ用）
/// </summary>
void BattleEnemyManager::DrawCollision() {
	if (!isBattleActive_) return;

	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			enemy->DrawCollision();
		}
	}
}

/// <summary>
/// 終了処理
/// </summary>
void BattleEnemyManager::Finalize() {
	Logger("[BattleEnemyManager] 終了処理開始\n");

	RemoveAllBattleEnemies();
	encounterDataMap_.clear();
	formationMap_.clear();
	enemyDataMap_.clear();

	camera_ = nullptr;
	player_ = nullptr;
	battleEndCallback_ = nullptr;

	Logger("[BattleEnemyManager] 終了処理完了\n");
}

/// <summary>
/// 戦闘ログをファイルに保存
/// </summary>
/// <param name="filePath">出力先ファイルパス</param>
void BattleEnemyManager::SaveBattleLog(const std::string& filePath) {



	///
	///
	///		使っていない
	///
	/// 
	try {
		Logger("[BattleEnemyManager] 戦闘ログ保存: " + filePath + "\n");

		std::ofstream file(filePath);
		if (file.is_open()) {
			file << "=== 戦闘ログ ===" << std::endl;
			file << "エンカウント: " << currentEncounterName_ << std::endl;
			file << "結果: " << static_cast<int>(battleResult_) << std::endl;
			file << "戦闘時間: " << battleStats_.battleDuration << "秒" << std::endl;
			file << "撃破数: " << battleStats_.enemiesDefeated << std::endl;
			file << "===================" << std::endl;
			file.close();

			Logger("[BattleEnemyManager] 戦闘ログ保存完了\n");
		} else {
			Logger("[BattleEnemyManager] エラー: ログファイルを開けません: " + filePath + "\n");
		}
	}
	catch (const std::exception& e) {
		Logger("[BattleEnemyManager] エラー: 戦闘ログ保存失敗: " + std::string(e.what()) + "\n");
	}
}


/// <summary>
/// デバッグ情報をImGui上に表示
/// </summary>
void BattleEnemyManager::ShowDebugInfo() {
#ifdef USE_IMGUI
	if (ImGui::Button("敵データ読み込み")) {
		LoadEnemyData(enemyDataFilePath_);
	}
	ImGui::Text("戦闘中: %s", isBattleActive_ ? "はい" : "いいえ");
	ImGui::Text("一時停止: %s", isBattlePaused_ ? "はい" : "いいえ");
	ImGui::Text("アクティブな敵: %zu", GetActiveEnemyCount());
	ImGui::Text("戦闘時間: %.1f秒", battleTimer_);
	ImGui::Text("現在のエンカウント: %s", currentEncounterName_.c_str());

	const char* resultStrings[] = { "なし", "勝利", "敗北", "逃走", "進行中" };
	ImGui::Text("戦闘結果: %s", resultStrings[static_cast<int>(battleResult_)]);

	ImGui::Separator();

	ImGui::Text("=== 戦闘統計 ===");
	ImGui::Text("撃破数: %d", battleStats_.enemiesDefeated);
	ImGui::Text("戦闘時間: %.1f秒", battleStats_.battleDuration);

	ImGui::Separator();

	if (ImGui::Button("テスト戦闘開始")) {
		EnemyEncounterData testEncounter;
		testEncounter.encounterName = "テスト戦闘";
		testEncounter.enemyIds = { "goblin", "orc" };
		testEncounter.formations = { Vector3(-2.0f, 0.0f, 5.0f), Vector3(2.0f, 0.0f, 5.0f) };
		StartBattle(testEncounter);
	}

	ImGui::SameLine();
	if (ImGui::Button("戦闘終了")) {
		ForceBattleEnd();
	}

	if (isBattleActive_) {
		if (ImGui::Button("一時停止/再開")) {
			PauseBattle(!isBattlePaused_);
		}

		ImGui::SameLine();
		if (ImGui::Button("勝利")) {
			EndBattle(BattleResult::Victory);
		}
		ImGui::SameLine();
		if (ImGui::Button("敗北")) {
			EndBattle(BattleResult::Defeat);
		}
	}

	ImGui::Separator();

	if (ImGui::Button("全敵スタン(2秒)")) {
		StunAllEnemies(2.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("全敵ダメージ(50)")) {
		DamageAllEnemies(50);
	}

	ImGui::Separator();

	static char enemyIdBuffer[256] = "goblin";
	static float spawnPos[3] = { 0.0f, 0.0f, 5.0f };

	ImGui::InputText("敵ID", enemyIdBuffer, sizeof(enemyIdBuffer));
	ImGui::InputFloat3("生成位置", spawnPos);

	if (ImGui::Button("デバッグ生成")) {
		Vector3 position(spawnPos[0], spawnPos[1], spawnPos[2]);
		DebugSpawnEnemy(position, enemyIdBuffer);
	}

	ImGui::Separator();

	// ★★★ 敵ベースデータの調整セクション (Map Cache) ★★★
	if (ImGui::TreeNode("★★★ 敵ベースデータ編集 (Map Cache) ★★★")) {
		// enemyDataMap_ に格納されている全てのベースデータを表示・編集可能にする
		for (auto& pair : enemyDataMap_) {
			BattleEnemyData& data = pair.second;

			if (ImGui::TreeNode(data.enemyId.c_str())) {

				// HPやAttackなど、個体ごとのコピー元になるデータをここで編集
				ImGui::DragInt("Base HP", &data.hp, 1, 1, 999);
				ImGui::DragInt("Attack", &data.attack, 1, 1, 500);
				ImGui::DragInt("Defense", &data.defense, 1, 1, 500);

				// 移動速度なども、ベース値を編集したいならここに含める
				ImGui::DragFloat("Base Move Speed", &data.moveSpeed, 0.1f, 0.1f, 20.0f);
				ImGui::DragFloat("Approach Range", &data.approachStateRange, 0.1f, 1.0f, 100.0f);
				ImGui::DragFloat("Attack Range", &data.attackStateRange, 0.1f, 1.0f, 50.0f);

				char aiTypeBuffer[64];
				strncpy_s(aiTypeBuffer, data.aiType.c_str(), sizeof(aiTypeBuffer));
				if (ImGui::InputText("AI Type", aiTypeBuffer, sizeof(aiTypeBuffer))) {
					data.aiType = aiTypeBuffer;
				}

				ImGui::TreePop();
			}
		}

		ImGui::Separator();
		// 調整後のデータをファイルに保存するボタン
		if (ImGui::Button("Save Enemy Data to JSON")) {
			if (SaveEnemyData(enemyDataFilePath_)) {
				Logger("[BattleEnemyManager] 敵ベースデータをJSONファイルに保存しました。\n");
			}
			else {
				ThrowError("[BattleEnemyManager] 敵ベースデータの保存に失敗しました。\n");
			}
		}

		ImGui::TreePop();
	}
	// ★★★ ----------------------------------------------- ★★★

	ImGui::Separator();

	if (ImGui::TreeNode("アクティブな敵")) {
		for (size_t i = 0; i < battleEnemies_.size(); ++i) {
			auto& enemy = battleEnemies_[i];
			if (enemy) {
				std::string label = "敵 " + std::to_string(i) + " (" + enemy->GetEnemyData().enemyId + ")";
				if (ImGui::TreeNode(label.c_str())) {
					ImGui::Text("HP: %d / %d", enemy->GetCurrentHP(), enemy->GetMaxHP());
					ImGui::Text("生存: %s", enemy->IsAlive() ? "はい" : "いいえ");

					Vector3 pos = enemy->GetTranslate();
					ImGui::Text("位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

					// 非const参照で受け取り、個体のデータを直接操作する (テスト用)
					BattleEnemyData& enemyData = enemy->GetEnemyData();
					ImGui::Text("敵ID: %s", enemyData.enemyId.c_str());
					ImGui::Text("AIタイプ: %s", enemyData.aiType.c_str());
					ImGui::Text("モデル: %s", enemyData.modelPath.c_str());
					ImGui::Text("攻撃力: %d (Base:%d)", enemyData.attack, enemyData.attack); // Base値も表示すると分かりやすい
					ImGui::Text("防御力: %d (Base:%d)", enemyData.defense, enemyData.defense);

					// 個体の移動速度は、その場でテストするために編集可能にしておく
					ImGui::DragFloat("移動速度 (Current)", &enemyData.moveSpeed, 0.1f, 0.0f, 20.0f);
					ImGui::DragFloat("攻撃状態に入る距離 (Current)", &enemyData.attackStateRange, 0.1f, 0.0f, 100.0f);
					ImGui::DragFloat("追跡状態に入る距離 (Current)", &enemyData.approachStateRange, 0.1f, 0.0f, 100.0f);

					if (ImGui::Button("ダメージ(25)")) {
						enemy->TakeDamage(25);
					}
					ImGui::SameLine();
					if (ImGui::Button("回復(30)")) {
						enemy->Heal(30);
					}

					ImGui::TreePop();
				}
			}
		}
		// ★ここにあったSaveボタンのコードは削除しました★
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("フォーメーション")) {
		// ... (既存のフォーメーション表示コード)
		for (const auto& pair : formationMap_) {
			const auto& formation = pair.second;
			if (ImGui::TreeNode(formation.formationName.c_str())) {
				ImGui::Text("説明: %s", formation.description.c_str());
				ImGui::Text("位置数: %zu", formation.positions.size());
				for (size_t i = 0; i < formation.positions.size(); ++i) {
					const auto& pos = formation.positions[i];
					ImGui::Text("  %zu: (%.1f, %.1f, %.1f)", i, pos.x, pos.y, pos.z);
				}
				if (ImGui::Button("フォーメーション設定")) {
					SetFormation(formation.formationName);
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
#endif
}
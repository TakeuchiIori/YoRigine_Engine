#include "BattleEnemy.h"
#include "Player/Player.h"
#include "Systems/GameTime/GameTime.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include <Loaders/Json/JsonManager.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>

#include "States/BattleIdleState.h"
#include "States/BattleRushAttackState.h"
#include "States/BattleDamageState.h"
#include "States/BattleDownedState.h"
#include "States/BattleDeadState.h"

#include "Debugger/Logger.h"

/// <summary>
/// デストラクタ（OBBコライダーの破棄）
/// </summary>
BattleEnemy::~BattleEnemy() {
	if (obbCollider_) {
		obbCollider_->~OBBCollider();
	}
}

/// <summary>
/// 敵オブジェクトの初期化処理
/// </summary>
/// <param name="camera">描画カメラ</param>
void BattleEnemy::Initialize(Camera* camera) {
	camera_ = camera;
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	wt_.Initialize();
	wt_.useAnchorPoint_ = true;
	InitCollision();
}

/// <summary>
/// 戦闘用データを適用して初期化
/// </summary>
/// <param name="data">敵データ構造体</param>
/// <param name="position">初期位置</param>
void BattleEnemy::InitializeBattleData(const BattleEnemyData& data, Vector3 position)
{
	// データ適用
	enemyData_ = data;
	enemyData_.maxHp_ = data.hp;
	enemyData_.currentHp_ = enemyData_.maxHp_;

	// モデル設定
	if (obj_) {
		obj_->SetModel(data.modelPath);
	}

	// 初期位置設定
	wt_.translate_ = position;
	isAlive_ = true;

	// 初期ステートは Idle に設定
	ChangeState(std::make_unique<BattleIdleState>());
	Logger(("[BattleEnemy] Initialized from JSON: ID=" + data.enemyId + ", HP=" + std::to_string(data.hp) + "\n").c_str());
}

/// <summary>
/// コリジョン（当たり判定）初期化処理
/// </summary>
void BattleEnemy::InitCollision() {
	// OBBコライダー生成
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_, static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy));
}

/// <summary>
/// Jsonマネージャの初期化
/// </summary>
void BattleEnemy::InitJson() {
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("BattleEnemy", "Resources/Json/Objects/BattleEnemies");
	jsonManager_->SetCategory("BattleEnemies");
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
void BattleEnemy::Update() {
	float dt = YoRigine::GameTime::GetDeltaTime();
	stateTimer_ += dt;

	// 現在のステートを更新
	if (currentState_) {
		currentState_->Update(*this, dt);
	}

	// 死亡チェック
	if (enemyData_.currentHp_ == 0) {
		ChangeState(std::make_unique<BattleDeadState>());
		PlayDeathEffect();
	}

	// 行列とコリジョン更新
	wt_.UpdateMatrix();
	if (obbCollider_) obbCollider_->Update();
}

/// <summary>
/// ステートを切り替える
/// </summary>
/// <param name="newState">新しい状態オブジェクト</param>
void BattleEnemy::ChangeState(std::unique_ptr<IEnemyState<BattleEnemy>> newState) {
	if (currentState_) currentState_->Exit(*this);
	currentState_ = std::move(newState);
	if (currentState_) currentState_->Enter(*this);
	stateTimer_ = 0.0f;
}

/// <summary>
/// プレイヤーの現在位置を取得
/// </summary>
/// <returns>プレイヤーのワールド座標（存在しない場合は(0,0,0)）</returns>
Vector3 BattleEnemy::GetPlayerPosition() const {
	if (player_) {
		return player_->GetWorldPosition();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

///************************* 攻撃・エフェクト処理 *************************///

/// <summary>
/// 通常攻撃を実行（プレイヤーにダメージを与える）
/// </summary>
void BattleEnemy::PerformBasicAttack() {
	if (/*!hasValidTarget_ ||*/ !player_) return;
	player_->TakeDamage(enemyData_.attack);
}

/// <summary>
/// 敵死亡時のエフェクトを再生
/// </summary>
void BattleEnemy::PlayDeathEffect() {
	if (obj_) obj_->SetMaterialColor({ 0.0f,0.0f,0.0f,1.0f });
}

///************************* デバッグ用 *************************///

/// <summary>
/// ダメージを受けてHPを減らす
/// </summary>
/// <param name="damage">与えられたダメージ量</param>
void BattleEnemy::TakeDamage(int damage) {
	if (isInvincible_ || !IsAlive()) return;
	enemyData_.currentHp_ -= damage;
	if (enemyData_.currentHp_ < 0) enemyData_.currentHp_ = 0;
}

/// <summary>
/// HPを回復する
/// </summary>
/// <param name="amount">回復量</param>
void BattleEnemy::Heal(int amount)
{
	if (!IsAlive()) return;
	enemyData_.currentHp_ += amount;
	if (enemyData_.currentHp_ > enemyData_.maxHp_) enemyData_.currentHp_ = enemyData_.maxHp_;
}

/// <summary>
/// ダメージを受けた際の点滅演出を更新
/// </summary>
/// <param name="dt">経過時間</param>
void BattleEnemy::UpdateBlinking(float dt) {
	// ダメージ時の点滅中でなければ処理しない
	if (!isDamageBlinking_) return;
	blinkTimer_ += dt;

	// 点滅スピード
	float blinkSpeed = 50.0f;

	// サイン波を利用してα値を周期的に変化させる
	float alpha = 0.65f + 0.35f * std::sin(blinkTimer_ * blinkSpeed);

	if (obj_) {
		obj_->GetColor() = { 1.0f, 0.0f, 0.0f, alpha };
	}
}

/// <summary>
/// めまい・スタン時の処理更新
/// </summary>
/// <param name="dt">経過時間</param>
void BattleEnemy::UpdateDizziness([[maybe_unused]] float dt)
{
}

///************************* 当たり判定 *************************///

/// <summary>
/// 他コライダーと接触した瞬間の処理
/// </summary>
void BattleEnemy::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {

	// 攻撃を食らった時
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)) {
		if (isAlive_) {
			TakeDamage(static_cast<int>(player_->GetCombat()->GetCombo()->GetCurrentDamage()));
			ChangeState(std::make_unique<BattleDamageState>());
		}
	}

	// 盾に当たった時
	if (dynamic_cast<BattleRushAttackState*>(GetCurrentState()) != nullptr) {
		if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield)) {
			if (player_->GetCombat()->GetGuard()->GetState() == PlayerGuard::State::Active ||
				player_->GetCombat()->GetGuard()->GetState() == PlayerGuard::State::Recovery) {
				if (isAlive_) {
					ChangeState(std::make_unique<BattleDownedState>());
				}
			}
		}
	}

	// プレイヤー本体に当たった時
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) {
		PerformBasicAttack();
	}
}

/// <summary>
/// 他コライダーとの接触中処理
/// </summary>
void BattleEnemy::OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {

}

/// <summary>
/// 他コライダーとの離脱時処理
/// </summary>
void BattleEnemy::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

/// <summary>
/// 衝突方向付き当たり判定処理
/// </summary>
void BattleEnemy::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}

/// <summary>
/// 敵モデルの描画処理（死亡時はフェードアウト）
/// </summary>
void BattleEnemy::Draw() {
	// 死亡していたら半透明にする
	if (player_->GetCombat()->IsDead()) {

		// 現在のマテリアルカラー取得
		Vector4 currentColor = obj_->GetColor();

		// 目標のカラー
		Vector4 targetColor = { 1.0f, 1.0f, 1.0f, 0.0f };

		// フェード速度
		float fadeSpeed = 3.0f;
		float deltaTime = YoRigine::GameTime::GetUnscaledDeltaTime();

		// 線形補間 (Lerp)
		currentColor = Lerp(currentColor, targetColor, fadeSpeed * deltaTime);

		// 設定
		obj_->SetMaterialColor(currentColor);
		if (currentColor.w <= 0.01f) {
			canAct_ = false;
		}
	}
	if (obj_) obj_->Draw(camera_, wt_);
}

void BattleEnemy::DrawShadow()
{
	if (obj_) obj_->DrawShadow(wt_);
}

/// <summary>
/// コリジョンのデバッグ描画
/// </summary>
void BattleEnemy::DrawCollision() {
	if (obbCollider_) obbCollider_->Draw();
}

/// <summary>
/// 敵データをJSONファイルから読み込む
/// </summary>
/// <param name="enemyId">読み込む敵のID</param>
/// <returns>読み込まれた敵データ構造体</returns>
BattleEnemyData BattleEnemyData::LoadFromJson(const std::string& enemyId) {
	BattleEnemyData data;
	data.enemyId = enemyId;

	std::string filePath = "Resources/Json/BattleEnemies/enemy_data.json";

	try {
		// ファイルが存在しない場合は作成
		std::filesystem::path path(filePath);
		if (!std::filesystem::exists(path.parent_path())) {
			std::filesystem::create_directories(path.parent_path());
			OutputDebugStringA("[BattleEnemyData] Created directory: Resources/Json/BattleEnemies/\n");
		}

		if (!std::filesystem::exists(path)) {
			// デフォルトのJSONファイルを作成
			nlohmann::json defaultJson;
			defaultJson["battleEnemies"] = nlohmann::json::array();

			// デフォルト敵データを追加
			nlohmann::json alienData;
			alienData["enemyId"] = "alien";
			alienData["modelPath"] = "Alien.obj";
			alienData["level"] = 3;
			alienData["hp"] = 120;
			alienData["attack"] = 18;
			alienData["defense"] = 12;
			alienData["moveSpeed"] = 5.0f;
			alienData["aiType"] = "aggressive";
			defaultJson["battleEnemies"].push_back(alienData);

			nlohmann::json blobData;
			blobData["enemyId"] = "green_blob";
			blobData["modelPath"] = "GreenSpikyBlob.obj";
			blobData["level"] = 5;
			blobData["hp"] = 200;
			blobData["attack"] = 25;
			blobData["defense"] = 20;
			blobData["moveSpeed"] = 3.0f;
			blobData["aiType"] = "defensive";
			defaultJson["battleEnemies"].push_back(blobData);

			nlohmann::json mushnubData;
			mushnubData["enemyId"] = "mushnub";
			mushnubData["modelPath"] = "Mushnub_Evolved.obj";
			mushnubData["level"] = 4;
			mushnubData["hp"] = 100;
			mushnubData["attack"] = 22;
			mushnubData["defense"] = 8;
			mushnubData["moveSpeed"] = 7.0f;
			mushnubData["aiType"] = "aggressive";
			defaultJson["battleEnemies"].push_back(mushnubData);

			std::ofstream outFile(filePath);
			outFile << defaultJson.dump(4);
			outFile.close();

			OutputDebugStringA("[BattleEnemyData] Created default enemy_data.json\n");
		}

		std::ifstream file(filePath);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open enemy_data.json");
		}

		nlohmann::json json;
		file >> json;
		file.close();

		if (!json.contains("battleEnemies") || !json["battleEnemies"].is_array()) {
			throw std::runtime_error("Invalid format in enemy_data.json");
		}

		bool found = false;
		for (const auto& enemyJson : json["battleEnemies"]) {
			if (enemyJson["enemyId"].get<std::string>() == enemyId) {
				data.modelPath = enemyJson.value("modelPath", "default_enemy.obj");
				data.level = enemyJson.value("level", 1);
				data.hp = enemyJson.value("hp", 100);
				data.attack = enemyJson.value("attack", 15);
				data.defense = enemyJson.value("defense", 10);
				data.moveSpeed = enemyJson.value("moveSpeed", 5.0f);
				data.aiType = enemyJson.value("aiType", "aggressive");

				found = true;
				OutputDebugStringA(("[BattleEnemyData] Loaded from JSON: " + enemyId + ", HP=" + std::to_string(data.hp) + "\n").c_str());
				break;
			}
		}

		if (!found) {
			OutputDebugStringA(("[BattleEnemyData] Warning: EnemyId '" + enemyId + "' not found in JSON, using defaults\n").c_str());
			data.modelPath = enemyId + ".obj";
		}

	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[BattleEnemyData] Error: " + std::string(e.what()) + "\n").c_str());
		data.modelPath = enemyId + ".obj";
	}

	return data;
}


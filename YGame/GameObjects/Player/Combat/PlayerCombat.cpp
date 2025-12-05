#include "PlayerCombat.h"

// App
#include "../Player.h"

// State
#include "../IdleCombat/IdleCombatState.h"
#include "../AttackingCombat/AttackingCombatState.h"
#include "../GuardingCombat/GuardingCombatState.h"
#include "../DodgingCombat/DodgingCombatState.h"
#include "../StunnedCombat/StunnedCombatState.h"
#include "../DeadCombat/DeadCombatState.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// コンストラクタ：戦闘システムの初期化
/// </summary>
/// <param name="owner">プレイヤーの所有者</param>
PlayerCombat::PlayerCombat(Player* owner) : owner_(owner) {
	//---------------------------------------------------------------------------------------------
	// システム初期化
	//---------------------------------------------------------------------------------------------
	combo_ = std::make_unique<PlayerCombo>(owner);
	guard_ = std::make_unique<PlayerGuard>(owner);

	// guardEmitter_ = std::make_unique<ParticleEmitter>("PlayerParticle", wt_.translate_, 5);
	// parryEmitter_ = std::make_unique<ParticleEmitter>("PlayerHitParticle", wt_.translate_, 5);

	//---------------------------------------------------------------------------------------------
	// StateMachine初期化（各Stateがコールバック設定）
	//---------------------------------------------------------------------------------------------
	InitializeStateMachine();
}

/// <summary>
/// ステートマシンの初期化
/// </summary>
void PlayerCombat::InitializeStateMachine() {
	//---------------------------------------------------------------------------------------------
	// 各状態を登録
	//---------------------------------------------------------------------------------------------
	stateMachine_.RegisterState<IdleCombatState>(CombatState::Idle, this);
	stateMachine_.RegisterState<AttackingCombatState>(CombatState::Attacking, this);
	stateMachine_.RegisterState<GuardingCombatState>(CombatState::Guarding, this);
	stateMachine_.RegisterState<DodgingCombatState>(CombatState::Dodging, this);
	stateMachine_.RegisterState<StunnedCombatState>(CombatState::Stunned, this);
	stateMachine_.RegisterState<DeadCombatState>(CombatState::Dead, this);

	// 初期状態を設定
	stateMachine_.SetInitialState(CombatState::Idle);

	// オーナーを設定
	stateMachine_.SetOwner(this);
}

/// <summary>
/// 毎フレームの更新処理
/// </summary>
/// <param name="deltaTime">フレーム経過時間</param>
void PlayerCombat::Update(float deltaTime) {
	//---------------------------------------------------------------------------------------------
	// HPが0以下なら死亡状態に遷移
	//---------------------------------------------------------------------------------------------
	if (owner_->GetHP() <= 0 && GetCurrentState() != CombatState::Dead) {
		ChangeState(CombatState::Dead);
		return; // 死亡状態では以降の処理をスキップ
	}

	//---------------------------------------------------------------------------------------------
	// ステートマシン更新
	//---------------------------------------------------------------------------------------------
	stateMachine_.Update(deltaTime);

	//---------------------------------------------------------------------------------------------
	// システム更新（コンボ・ガード）
	//---------------------------------------------------------------------------------------------
	combo_->Update(deltaTime);
	guard_->Update(deltaTime);
}

/// <summary>
/// 戦闘状態をリセットする
/// </summary>
void PlayerCombat::Reset() {
	combo_->ResetCombo();
	guard_->Reset();
	stateMachine_.ChangeState(CombatState::Idle);
}

/// <summary>
/// 攻撃を試みる
/// </summary>
/// <param name="type">攻撃タイプ</param>
/// <returns>攻撃が成功したか</returns>
bool PlayerCombat::TryAttack(AttackType type) {
	// 他の行動中は攻撃不可
	if (!CanAct()) return false;

	// 任意の攻撃タイプを直接実行
	if (combo_->TryAttack(type)) {
		// 攻撃成功時はIdleから攻撃状態に遷移
		if (GetCurrentState() == CombatState::Idle) {
			ChangeState(CombatState::Attacking);
		}
		return true;
	}

	return false;
}

/// <summary>
/// 回避を試みる
/// </summary>
/// <returns>回避が成功したか</returns>
bool PlayerCombat::TryDodge() {
	if (!CanAct()) return false;

	// 回避状態に遷移
	ChangeState(CombatState::Dodging);

	// 回避成功時はCC回復
	combo_->OnDodgeSuccess();
	return true;
}

/// <summary>
/// ガードを試みる
/// </summary>
/// <returns>ガードが成功したか</returns>
bool PlayerCombat::TryGuard() {
	if (!CanAct()) return false;

	if (guard_->StartGuard()) {
		ChangeState(CombatState::Guarding);
		return true;
	}
	return false;
}

/// <summary>
/// 特殊攻撃を試みる（未実装）
/// </summary>
bool PlayerCombat::TrySpecial() {
	if (!CanAct()) return false;

	// 現状は未実装
	return false;
}

/// <summary>
/// 行動をキャンセルする（コンボキャンセル）
/// </summary>
/// <returns>キャンセルが成功したか</returns>
bool PlayerCombat::TryCancel() {
	if (combo_->GetCurrentAttack() && combo_->GetCurrentAttack()->canCancel) {
		combo_->CancelCombo();
		ChangeState(CombatState::Idle);
		return true;
	}
	return false;
}

/// <summary>
/// Idle状態か判定
/// </summary>
bool PlayerCombat::IsIdle() const {
	return GetCurrentState() == CombatState::Idle;
}

/// <summary>
/// 攻撃中か判定
/// </summary>
bool PlayerCombat::IsAttacking() const {
	return GetCurrentState() == CombatState::Attacking;
}

/// <summary>
/// 回避中か判定
/// </summary>
bool PlayerCombat::IsDodging() const {
	return GetCurrentState() == CombatState::Dodging;
}

/// <summary>
/// スタン中か判定
/// </summary>
bool PlayerCombat::IsStunned() const {
	return GetCurrentState() == CombatState::Stunned;
}

/// <summary>
/// 死亡状態か判定
/// </summary>
bool PlayerCombat::IsDead() const {
	return GetCurrentState() == CombatState::Dead;
}

/// <summary>
/// 移動可能か判定
/// </summary>
bool PlayerCombat::CanMove() const {
	ComboState state = combo_->GetCurrentState();
	return GetCurrentState() == CombatState::Idle ||
		(state == ComboState::CanContinue && !IsStunned());
}

/// <summary>
/// 行動可能か判定
/// </summary>
bool PlayerCombat::CanAct() const {
	return !IsStunned();
}

/// <summary>
/// デバッグ表示（ImGui）
/// デバッグ情報・コンボ状態・操作ボタンの確認用
/// </summary>
void PlayerCombat::ShowDebugImGui() {
#ifdef USE_IMGUI
	if (ImGui::Begin("プレイヤーコンボ")) {

		//---------------------------------------------------------------------------------------------
		// 現在のステート情報
		//---------------------------------------------------------------------------------------------
		ImGui::Text("=== 戦闘状態 (StateMachine) ===");
		const char* stateNames[] = { "Idle", "Attacking", "Guarding", "Dodging", "Stunned" };
		ImGui::Text("Current State: %s", stateNames[static_cast<int>(GetCurrentState())]);
		ImGui::Text("Previous State: %s", stateNames[static_cast<int>(GetPreviousState())]);
		ImGui::Text("State Changed: %s", StateChanged() ? "Yes" : "No");

		//---------------------------------------------------------------------------------------------
		// 行動可能性
		//---------------------------------------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("=== 行動可能性 ===");
		ImGui::Text("Can Move: %s", CanMove() ? "Yes" : "No");
		ImGui::Text("Can Act: %s", CanAct() ? "Yes" : "No");

		//---------------------------------------------------------------------------------------------
		// CC情報
		//---------------------------------------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("=== CC情報 ===");
		ImGui::Text("Current CC: %d / %d", GetCurrentCC(), GetMaxCC());
		ImGui::ProgressBar(static_cast<float>(GetCurrentCC()) / GetMaxCC());

		//---------------------------------------------------------------------------------------------
		// コンボ情報
		//---------------------------------------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("=== コンボ情報 ===");
		ImGui::Text("Combo Count: %d", GetComboCount());
		ImGui::Text("Damage Multiplier: x%.2f", GetComboDamageMultiplier());
		ImGui::Text("Combo State: %s", combo_->GetStateString(GetComboState()));

		//---------------------------------------------------------------------------------------------
		// 操作テスト
		//---------------------------------------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("=== 操作テスト ===");
		if (ImGui::Button("A攻撃")) { TryAttack(AttackType::A_Arte); }
		ImGui::SameLine();
		if (ImGui::Button("B攻撃")) { TryAttack(AttackType::B_Arte); }
		ImGui::SameLine();
		if (ImGui::Button("奥義")) { TryAttack(AttackType::Arcane_Arte); }

		if (ImGui::Button("回避")) { TryDodge(); }
		ImGui::SameLine();
		if (ImGui::Button("ガード")) { TryGuard(); }
		ImGui::SameLine();
		if (ImGui::Button("キャンセル")) { TryCancel(); }

		//---------------------------------------------------------------------------------------------
		// コンボシステムデバッグ
		//---------------------------------------------------------------------------------------------
		ImGui::Separator();
		if (ImGui::CollapsingHeader("コンボシステム")) {
			combo_->ShowDebugImGui();
		}
	}
	ImGui::End();
	guard_->ShowDebugImGui();
#endif
}

#include "BattleAttackState.h"
#include "BattleIdleState.h"
#include <cmath>

/// <summary>
/// 攻撃状態開始処理
/// </summary>
void BattleAttackState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
	dirLocked_ = false;
}

/// <summary>
/// 攻撃状態更新処理
/// </summary>
void BattleAttackState::Update(BattleEnemy& enemy, float dt) {
	float t = enemy.GetStateTimer();

	// 溜め
	if (t < 1.0f) {
		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}
	}
	// 突進
	else if (t < 1.5f) {
		enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * 9.0f * dt);
	}
	// クールダウン → 終了
	else if (t >= 2.7f) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

/// <summary>
/// 攻撃状態終了処理
/// </summary>
void BattleAttackState::Exit(BattleEnemy& enemy) {
	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}

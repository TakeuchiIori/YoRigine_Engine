#include "BattleDamageState.h"
#include "BattleRushAttackState.h"

/// <summary>
/// ダメージ状態開始処理
/// </summary>
void BattleDamageState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.IsDamageBlinking() = true;
	enemy.ResetStateTimer();
}

/// <summary>
/// ダメージ状態更新処理
/// </summary>
void BattleDamageState::Update(BattleEnemy& enemy, float dt) {
	// 点滅させる
	enemy.UpdateBlinking(dt);

	// 一定時間経過で攻撃状態へ
	if (enemy.GetStateTimer() > 1.0f) {
		enemy.ChangeState(std::make_unique<BattleRushAttackState>());
	}
}

/// <summary>
/// ダメージ状態終了処理
/// </summary>
void BattleDamageState::Exit(BattleEnemy& enemy) {
	enemy.SetCanAct(true);
	enemy.IsDamageBlinking() = false;
	enemy.SetColor({ 1, 1, 1, 1 });
}

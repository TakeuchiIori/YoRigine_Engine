#include "BattleComboAttackState.h"

void BattleComboAttackState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
	comboCount_ = 0;
}

void BattleComboAttackState::Update(BattleEnemy& enemy, float dt)
{
	float t = enemy.GetStateTimer();

	// 1段目 (0.0 ~ 0.8秒)
	if (t < 0.8f) {
		UpdateComboPhase(enemy, t, 0.0f, 0.8f, dt);
	}
	// 2段目 (0.8 ~ 1.6秒)
	else if (t < 1.6f) {
		if (comboCount_ == 0) comboCount_ = 1;
		UpdateComboPhase(enemy, t, 0.8f, 1.6f, dt);
	}
	// 3段目 (1.6 ~ 2.4秒)
	else if (t < 2.4f) {
		if (comboCount_ == 1) comboCount_ = 2;
		UpdateComboPhase(enemy, t, 1.6f, 2.4f, dt);
	}
	// クールダウン (2.4 ~ 3.2秒)
	else if (t >= 3.2f) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleComboAttackState::Exit(BattleEnemy& enemy)
{
	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}

void BattleComboAttackState::UpdateComboPhase(BattleEnemy& enemy, float totalTime, float phaseStart, float phaseEnd, float dt)
{
	float phaseTime = totalTime - phaseStart;
	float phaseDuration = phaseEnd - phaseStart;
	float phaseT = phaseTime / phaseDuration;

	// 溜め (前半30%)
	if (phaseT < 0.3f) {
		if (enemy.GetPlayer()) {
			Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(toPlayer) > 0.01f) {
				toPlayer = Normalize(toPlayer);
				enemy.SetRotationY(std::atan2(toPlayer.x, toPlayer.z));
			}
		}
	}
	// 突進 (後半70%)
	else {
		if (enemy.GetPlayer()) {
			Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(toPlayer) > 0.01f) {
				toPlayer = Normalize(toPlayer);
				// コンボが進むほど速くなる
				float speedMultiplier = 5.0f + comboCount_ * 2.0f;
				enemy.AddTranslate(toPlayer * enemy.GetEnemyData().moveSpeed * speedMultiplier * dt);
			}
		}
	}
}

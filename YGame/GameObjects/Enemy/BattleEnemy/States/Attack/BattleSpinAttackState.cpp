#include "BattleSpinAttackState.h"

void BattleSpinAttackState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
	startRotation_ = enemy.GetRotationY();
}

void BattleSpinAttackState::Update(BattleEnemy& enemy, float dt)
{
	float t = enemy.GetStateTimer();

	// 溜め (0.0 ~ 0.3秒)
	if (t < 0.3f) {
		// 準備動作
	}
	// 回転 (0.3 ~ 1.3秒)
	else if (t < 1.3f) {
		float spinT = (t - 0.3f) / 1.0f;

		// 2回転する
		const float PI = 3.14159f;
		enemy.SetRotationY(startRotation_ + spinT * 2.0f * 2.0f * PI);

		// 回転しながら少し前進
		if (enemy.GetPlayer()) {
			Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(dir) > 0.01f) {
				dir = Normalize(dir);
				enemy.AddTranslate(dir * enemy.GetEnemyData().moveSpeed * 2.0f * dt);
			}
		}
	}
	// クールダウン (1.3 ~ 2.0秒)
	else if (t >= 2.0f) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleSpinAttackState::Exit(BattleEnemy& enemy)
{	
	// プレイヤーの方を向く
	if (enemy.GetPlayer()) {
		Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(dir) > 0.01f) {
			enemy.SetRotationY(std::atan2(dir.x, dir.z));
		}
	}

	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}

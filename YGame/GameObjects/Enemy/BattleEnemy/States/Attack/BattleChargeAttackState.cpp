#include "BattleChargeAttackState.h"

void BattleChargeAttackState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.5f, 0.0f, 1 }); // オレンジ色で警告

	if (enemy.GetPlayer()) {
		attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(attackDir_) > 0.01f) {
			attackDir_ = Normalize(attackDir_);
			enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
		}
	}
}

void BattleChargeAttackState::Update(BattleEnemy& enemy, float dt)
{
	float t = enemy.GetStateTimer();

	// 長い溜め (0.0 ~ 1.5秒)
	if (t < 1.5f) {
		// プレイヤーを追尾
		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}

		// 色を点滅させて警告
		float blink = std::sin(t * 10.0f) * 0.3f + 0.7f;
		enemy.SetColor({ 1, 0.5f * blink, 0.0f, 1 });
	}
	// 高速突進 (1.5 ~ 2.0秒)
	else if (t < 2.0f) {
		enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
		// 通常の突進より速い
		enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * 12.0f * dt);
	}
	// クールダウン (2.0 ~ 3.0秒)
	else if (t >= 3.0f) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleChargeAttackState::Exit(BattleEnemy& enemy)
{
	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}

#include "BattleLeapAttackState.h"

void BattleLeapAttackState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.0f, 0.0f, 1 });

	startPos_ = enemy.GetTranslate();
	startY_ = startPos_.y;

	// ターゲット位置を決定
	if (enemy.GetPlayer()) {
		targetPos_ = enemy.GetPlayerPosition();
		Vector3 dir = targetPos_ - startPos_;
		if (Length(dir) > 0.01f) {
			enemy.SetRotationY(std::atan2(dir.x, dir.z));
		}
	}
	else {
		targetPos_ = startPos_;
	}
}

void  BattleLeapAttackState::Update(BattleEnemy& enemy, [[maybe_unused]]float dt)
{
	float t = enemy.GetStateTimer();

	// 溜め (0.0 ~ 0.5秒) - しゃがむ
	if (t < 0.5f) {
		float chargeT = t / 0.5f;
		Vector3 pos = startPos_;
		pos.y = startY_ - 0.3f * chargeT;
		enemy.SetTranslate(pos);
	}
	// ジャンプ (0.5 ~ 1.2秒)
	else if (t < 1.2f) {
		float leapT = (t - 0.5f) / 0.7f;

		Vector3 pos;
		pos.x = startPos_.x + (targetPos_.x - startPos_.x) * leapT;
		pos.z = startPos_.z + (targetPos_.z - startPos_.z) * leapT;

		// Y座標は放物線
		float heightCurve = 4.0f * leapT * (1.0f - leapT);
		pos.y = startY_ + 2.5f * heightCurve;

		enemy.SetTranslate(pos);
	}
	// 着地 (1.2 ~ 1.8秒)
	else if (t < 1.8f) {
		Vector3 pos = targetPos_;
		pos.y = startY_;
		enemy.SetTranslate(pos);
	}
	// 終了
	else {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleLeapAttackState::Exit(BattleEnemy& enemy)
{	
	// 元の高さに戻す
	Vector3 pos = enemy.GetTranslate();
	pos.y = startY_;
	enemy.SetTranslate(pos);

	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}

#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"
#include <random>

/// <summary>
/// 攻撃状態
/// </summary>
class BattleRushAttackState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;
	void Update(BattleEnemy& enemy, float dt) override;
	void Exit(BattleEnemy& enemy) override;

private:
	Vector3 attackDir_{ 0,0,0 };
	bool dirLocked_ = false;
};


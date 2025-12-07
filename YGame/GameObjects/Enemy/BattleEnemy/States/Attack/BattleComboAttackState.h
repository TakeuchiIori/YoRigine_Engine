#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"
#include "../BattleIdleState.h"

/// <summary>
/// コンボ攻撃状態（短い突進を3回連続）
/// </summary>
class BattleComboAttackState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;

	void Update(BattleEnemy& enemy, float dt) override;

	void Exit(BattleEnemy& enemy) override;

private:
	void UpdateComboPhase(BattleEnemy& enemy, float totalTime, float phaseStart, float phaseEnd, float dt);

private:
	int comboCount_ = 0;

};
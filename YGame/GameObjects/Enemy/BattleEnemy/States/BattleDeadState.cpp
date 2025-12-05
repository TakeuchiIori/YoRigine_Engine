#include "BattleDeadState.h"
#include "Vector3.h"

void BattleDeadState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
}


void BattleDeadState::Update(BattleEnemy& enemy, float dt)
{
	// 小さくなりながら死ぬように
	enemy.GetWT().scale_ -= dt * 1.0f;
	if (enemy.GetWT().scale_.y < 0.0f) {
		enemy.IsAlive() = false;
	}
}

void BattleDeadState::Exit([[maybe_unused]] BattleEnemy& enemy)
{
}

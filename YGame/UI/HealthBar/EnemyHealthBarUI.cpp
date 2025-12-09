#include "EnemyHealthBarUI.h"
#include "Enemy/BattleEnemy/BattleEnemy.h"
#include <Systems/GameTime/GameTime.h>
#include <Systems/UI/UIManager.h>

EnemyHealthBarUI::EnemyHealthBarUI(const BattleEnemy* enemy, Camera* camera)
{
	(void)enemy;
	camera_ = camera;
	bgHP_ = YoRigine::UIManager::GetInstance()->GetUI("EnemyHPBg");
	barHP_ = YoRigine::UIManager::GetInstance()->GetUI("EnemyHPBar");
}

void EnemyHealthBarUI::Initialize()
{
}

void EnemyHealthBarUI::Update()
{
}

void EnemyHealthBarUI::Draw()
{
}

#include "EnemyHealthBarUI.h"
#include "Enemy/BattleEnemy/BattleEnemy.h"
#include <Systems/GameTime/GameTime.h>
#include <Systems/UI/UIManager.h>

EnemyHealthBarUI::EnemyHealthBarUI(const BattleEnemy* enemy, Camera* camera)
{
	targetEnemy_ = enemy;
	camera_ = camera;
}

void EnemyHealthBarUI::Initialize()
{
	bgHP_ = std::make_unique<Sprite>();
	barHP_ = std::make_unique<Sprite>();
	bgHP_->SetCamera(camera_);
	barHP_->SetCamera(camera_);


	bgHP_->Initialize("Resources/Textures/GameScene/EnemyHPBg.png");
	barHP_->Initialize("Resources/Textures/GameScene/EnemyHPBar.png");

	bgHP_->SetAnchorPoint(Vector2{ 0.5f,0.5f });
	barHP_->SetAnchorPoint(Vector2{ 0.5f,0.5f });
	InitJson();
}

void EnemyHealthBarUI::Update()
{
	bgHP_->SetSize(size_);
	barHP_->SetSize(size_);
	worldPosition_ = targetEnemy_->GetTranslate();
	bgHP_->SetTranslate(worldPosition_ + offset_);
	barHP_->SetTranslate(worldPosition_+ offset_);

	bgHP_->Update();
	barHP_->Update();
}

void EnemyHealthBarUI::Draw()
{
	bgHP_->Draw();
	barHP_->Draw();
}

void EnemyHealthBarUI::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("EnemyHealthBarUI", "Resources/Json/UI/");
	jsonManager_->SetCategory("UI");
	jsonManager_->Register("offset", &offset_);
	jsonManager_->Register("Size", &size_);
}

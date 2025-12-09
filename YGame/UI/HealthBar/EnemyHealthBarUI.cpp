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


	bgHP_->Initialize("Resources/Textures/GameScene/EnemyHPBg.png");
	barHP_->Initialize("Resources/Textures/GameScene/EnemyHPBar.png");

	bgHP_->SetAnchorPoint(Vector2{ 0.0f,0.5f });
	barHP_->SetAnchorPoint(Vector2{ 0.0f,0.5f });
	InitJson();
}

void EnemyHealthBarUI::Update()
{
	// HP割合
	float ratio = (float)targetEnemy_->GetCurrentHP() / (float)targetEnemy_->GetMaxHP();
	ratio = std::clamp(ratio, 0.0f, 1.0f);
	currentRatio_ = ratio;

	// 背景サイズ
	bgHP_->SetSize(size_);

	// ゲージサイズは横だけ縮める
	barHP_->SetSize({ size_.x * ratio, size_.y });

	// ワールド座標（頭の位置）
	Vector3 head = targetEnemy_->GetTranslate() + offset_;
	// スクリーンオフセットを加算
	Vector3 screenOffset = targetEnemy_->GetTranslate() + screenOffset_;

	// ★どちらも同じ座標（ズラさない！）
	bgHP_->SetTranslate(head);
	barHP_->SetTranslate(screenOffset);

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
	jsonManager_->Register("screenOffset", &screenOffset_);
	jsonManager_->Register("Size", &size_);
}

#include "FadeTransition.h"

/// <summary>
/// フェードトランジションの初期化
/// </summary>
void FadeTransition::Initialize() {

	//------------------------------------------------------------
	// フェード用インスタンス生成
	//------------------------------------------------------------
	fade_ = std::make_unique<Fade>();
	fade_->Initialize("Resources/images/white.png");
}

/// <summary>
/// 更新処理
/// </summary>
void FadeTransition::Update() {
	fade_->Update();
}

/// <summary>
/// 描画処理
/// </summary>
void FadeTransition::Draw() {
	SpriteCommon::GetInstance()->DrawPreference();
	fade_->Draw();
}

/// <summary>
/// トランジション完了判定
/// </summary>
bool FadeTransition::IsFinished() const {
	return fade_->IsFinished();
}

/// <summary>
/// 明け始める（フェードイン）
/// </summary>
void FadeTransition::StartReveal() {
	fade_->Start(Fade::Status::FadeIn, FADE_DURATION);
}

/// <summary>
/// 覆い始める（フェードアウト）
/// </summary>
void FadeTransition::StartCover() {
	fade_->Start(Fade::Status::FadeOut, FADE_DURATION);
}

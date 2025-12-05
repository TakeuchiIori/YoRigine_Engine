#include "GameUI.h"
#include <Systems/GameTime/GameTime.h>
#include <Systems/Input/Input.h>
#include <Systems/GameTime/GameTime.h>

/// <summary>
/// 初期化処理
/// </summary>
void GameUI::Initialize()
{
	YoRigine::UIManager::GetInstance()->LoadScene("GameScene");

	// ゲームオーバーUI取得
	gameOver_ = YoRigine::UIManager::GetInstance()->GetUI("GameOver");
	bacground_ = YoRigine::UIManager::GetInstance()->GetUI("BackGround");
	retryButton_ = YoRigine::UIManager::GetInstance()->GetUI("Retry");
	titleButton_ = YoRigine::UIManager::GetInstance()->GetUI("ToTitle");

	// 操作用UI取得
	padA_ = YoRigine::UIManager::GetInstance()->GetUI("A");
	padB_ = YoRigine::UIManager::GetInstance()->GetUI("B");
	padLB_ = YoRigine::UIManager::GetInstance()->GetUI("LB");
	strong_ = YoRigine::UIManager::GetInstance()->GetUI("Strong");
	weak_ = YoRigine::UIManager::GetInstance()->GetUI("Weak");
	guard_ = YoRigine::UIManager::GetInstance()->GetUI("Guard");
	uiBackGround_ = YoRigine::UIManager::GetInstance()->GetUI("UIBackGround");
	startButton_ = YoRigine::UIManager::GetInstance()->GetUI("startButton");

	// ゲームオーバーUIを初期状態で非表示
	if (gameOver_) gameOver_->SetVisible(false);
	if (bacground_) bacground_->SetVisible(false);
	if (retryButton_) {
		retryButton_->SetVisible(false);
		// 元のサイズを保存
		retryButtonOriginalSize_ = retryButton_->GetScale();
	}
	if (titleButton_) {
		titleButton_->SetVisible(false);
		// 元のサイズを保存
		titleButtonOriginalSize_ = titleButton_->GetScale();
	}

	// 念のため初期状態をクリア
	ResetGameOver();
}

/// <summary>
/// 更新処理
/// </summary>
void GameUI::Update()
{
	// スタートボタンで操作UI表示
	if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::Start)) {
		if (!goVisible_) {
			isVisble_ = !isVisble_;
		}
	}
	padA_->SetVisible(isVisble_);
	padB_->SetVisible(isVisble_);
	padLB_->SetVisible(isVisble_);
	strong_->SetVisible(isVisble_);
	weak_->SetVisible(isVisble_);
	guard_->SetVisible(isVisble_);
	uiBackGround_->SetVisible(isVisble_);

	// 操作用UIの表示中はゲーム停止
	if (isVisble_) {
		YoRigine::GameTime::Pause();
	} else {
		//GameTime::Resume();
	}

	// UI全体の更新
	YoRigine::UIManager::GetInstance()->UpdateAll();

	// ゲームオーバーUIのフェード処理
	if (goFadingIn_) {
		goFadeTimer_ += YoRigine::GameTime::GetUnscaledDeltaTime();
		float t = (goFadeDuration_ > 0.0f) ? (goFadeTimer_ / goFadeDuration_) : 1.0f;

		// フェード完了判定
		if (t >= 1.0f) {
			t = 1.0f;
			goFadingIn_ = false;
			goFadeCompleted_ = true;

			// フェード完了後にボタンを表示し、初期スケールを設定
			if (retryButton_) {
				retryButton_->SetVisible(true);
				// 初期選択はリトライなので拡大表示
				retryButton_->SetScale(Vector2(retryButtonOriginalSize_.x * selectionScale_, retryButtonOriginalSize_.y * selectionScale_));
			}
			if (titleButton_) {
				titleButton_->SetVisible(true);
				// 非選択なので通常サイズ
				titleButton_->SetScale(Vector2(titleButtonOriginalSize_.x * normalScale_, titleButtonOriginalSize_.y * normalScale_));
			}
		}
		goAlpha_ = t;
	}

	// ゲームオーバー時のボタン入力チェック（フェード完了後のみ）
	if (goFadeCompleted_ && goVisible_) {
		// 左スティックで選択切り替え
		const float stickThreshold = 0.5f;
		static bool stickProcessed = false;

		float stickX = YoRigine::Input::GetInstance()->GetLeftStickX(0);

		if (abs(stickX) > stickThreshold) {
			if (!stickProcessed) {
				if (stickX > stickThreshold) {
					// 右に倒す → タイトルを選択
					currentSelection_ = GameOverSelection::TITLE;
				} else if (stickX < -stickThreshold) {
					// 左に倒す → リトライを選択
					currentSelection_ = GameOverSelection::RETRY;
				}
				stickProcessed = true;
			}
		} else {
			stickProcessed = false;
		}

		// スケールを選択状態に応じて更新
		if (retryButton_) {
			float scale = (currentSelection_ == GameOverSelection::RETRY) ? selectionScale_ : normalScale_;
			retryButton_->SetScale(Vector2(retryButtonOriginalSize_.x * scale, retryButtonOriginalSize_.y * scale));
		}
		if (titleButton_) {
			float scale = (currentSelection_ == GameOverSelection::TITLE) ? selectionScale_ : normalScale_;
			titleButton_->SetScale(Vector2(titleButtonOriginalSize_.x * scale, titleButtonOriginalSize_.y * scale));
		}

		// Aボタンで決定
		if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::A)) {
			if (currentSelection_ == GameOverSelection::RETRY) {
				retryRequested_ = true;
			} else {
				returnToTitleRequested_ = true;
			}
		}

		// キーボード操作も維持（スペースでリトライ、ESCでタイトル）
		if (YoRigine::Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			currentSelection_ = GameOverSelection::RETRY;
			retryRequested_ = true;
		}
		if (YoRigine::Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
			currentSelection_ = GameOverSelection::TITLE;
			returnToTitleRequested_ = true;
		}
	}

	// フェード後のアルファ反映
	ApplyAlpha_(goAlpha_);
}


/// <summary>
/// UI描画（全部）
/// </summary>
void GameUI::DrawAll()
{
	YoRigine::UIManager::GetInstance()->DrawAll();
}

/// <summary>
/// UI描画（個別）
/// </summary>
void GameUI::Draw()
{
	// 表示フラグに応じて描画
	if (goVisible_ && gameOver_) {
		bacground_->Draw();
		gameOver_->Draw();
	}

}

/// <summary>
/// ゲームオーバーUIをフェード表示する
/// </summary>
/// <param name="duration">フェード時間（秒）</param>
void GameUI::ShowGameOverWithFade(float duration)
{
	goVisible_ = true;
	goFadingIn_ = true;
	goFadeCompleted_ = false;

	// UIを表示状態にする（ボタンはフェード完了後に表示）
	if (bacground_) bacground_->SetVisible(true);
	if (gameOver_) gameOver_->SetVisible(true);
	startButton_->SetVisible(false); // スタートボタンは非表示に

	goFadeDuration_ = (duration > 0.0f) ? duration : 0.001f;
	goFadeTimer_ = 0.0f;
	goAlpha_ = 0.0f;

	// 選択状態をリトライに初期化
	currentSelection_ = GameOverSelection::RETRY;

	// すぐ反映（0から開始）
	ApplyAlpha_(goAlpha_);
}

/// <summary>
/// ゲームオーバーUIの状態をリセットし、描画を停止する
/// </summary>
void GameUI::ResetGameOver()
{
	goVisible_ = false;
	goFadingIn_ = false;
	goFadeCompleted_ = false;
	goFadeTimer_ = 0.0f;
	goFadeDuration_ = 0.6f;
	goAlpha_ = 0.0f;

	// UIを非表示にする
	if (bacground_) bacground_->SetVisible(false);
	if (gameOver_) gameOver_->SetVisible(false);
	if (retryButton_) {
		retryButton_->SetVisible(false);
		retryButton_->SetScale(retryButtonOriginalSize_);
	}
	if (titleButton_) {
		titleButton_->SetVisible(false);
		titleButton_->SetScale(titleButtonOriginalSize_);
	}
	startButton_->SetVisible(true);

	if (gameOver_) {
		ApplyAlpha_(0.0f);
	}

	// 選択状態をリセット
	currentSelection_ = GameOverSelection::RETRY;

	// リクエストフラグもリセット
	ClearRequests();
}

/// <summary>
/// UIBaseへのアルファ値を適用
/// </summary>
/// <param name="a">アルファ値（0〜1）</param>
void GameUI::ApplyAlpha_(float a)
{
	if (!gameOver_) return;

	a = std::clamp(a, 0.0f, 1.0f);

	// 色を適用
	Vector4 col = gameOver_->GetColor();
	Vector4 colBG = { 0.0f, 0.0f, 0.0f, 0.0f };

	colBG.w = a * 0.95f;
	col.w = a;

	gameOver_->SetColor(col);
	bacground_->SetColor(colBG);
}

/// <summary>
/// リクエストフラグをリセット
/// </summary>
void GameUI::ClearRequests()
{
	retryRequested_ = false;
	returnToTitleRequested_ = false;
}
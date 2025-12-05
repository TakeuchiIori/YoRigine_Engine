#pragma once
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>

/// <summary>
/// ゲームシーンで表示するUIクラス
/// </summary>
class GameUI
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Update();
	void DrawAll();
	void Draw();

	/// <summary>
	///  ゲームオーバーUIをフェード表示する
	/// </summary>
	/// <param name="duration">フェード時間（秒）</param>
	void ShowGameOverWithFade(float duration = 0.6f);

	/// <summary>
	///  ゲームオーバーUIの状態をリセットし、描画を停止する
	/// </summary>
	void ResetGameOver();

	/// <summary>
	///  フェードが完了したかどうかを取得
	/// </summary>
	bool IsFadeCompleted() const { return goFadeCompleted_; }

	/// <summary>
	///  リトライボタンが押されたかチェック
	/// </summary>
	bool IsRetryRequested() const { return retryRequested_; }

	/// <summary>
	///  タイトルに戻るボタンが押されたかチェック
	/// </summary>
	bool IsReturnToTitleRequested() const { return returnToTitleRequested_; }

	/// <summary>
	///  リクエストフラグをリセット
	/// </summary>
	void ClearRequests();

public:
	///************************* アクセッサ *************************///
	void ApplyAlpha_(float a);
private:
	///************************* メンバ変数 *************************///
	UIBase* gameOver_ = nullptr;
	UIBase* bacground_ = nullptr;
	UIBase* padA_ = nullptr;
	UIBase* padB_ = nullptr;
	UIBase* padLB_ = nullptr;
	UIBase* strong_ = nullptr;
	UIBase* weak_ = nullptr;
	UIBase* guard_ = nullptr;
	UIBase* uiBackGround_ = nullptr;
	UIBase* startButton_ = nullptr;

	// ゲームオーバー関連UI
	UIBase* retryButton_ = nullptr;
	UIBase* titleButton_ = nullptr;

	// フェード管理
	bool   goVisible_ = false;  // 表示中か（描画のON/OFF）
	bool   goFadingIn_ = false;  // フェード中か
	bool   goFadeCompleted_ = false;  // フェード完了フラグ
	float  goFadeTimer_ = 0.0f;   // 経過時間
	float  goFadeDuration_ = 1.6f;   // フェード時間
	float  goAlpha_ = 0.0f;   // 現在アルファ（0～1）

	// ゲームオーバー選択関連
	bool retryRequested_ = false;
	bool returnToTitleRequested_ = false;

	// 選択UI管理
	enum class GameOverSelection {
		RETRY,
		TITLE
	};
	GameOverSelection currentSelection_ = GameOverSelection::RETRY;
	float selectionScale_ = 1.5f;  // 選択時の拡大率
	float normalScale_ = 1.0f;     // 通常時のスケール
	Vector2 retryButtonOriginalSize_ = { 0.0f, 0.0f };  // リトライボタンの元のサイズ
	Vector2 titleButtonOriginalSize_ = { 0.0f, 0.0f };  // タイトルボタンの元のサイズ

	// 操作用UI関連
	bool isVisble_ = false;
};
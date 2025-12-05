#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include "Sprite/Sprite.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "json.hpp" 

// 前方宣言
class SpriteCommon;
class Camera;

///************************* アニメーション構造体 *************************///
struct UIAnimation {
	enum class Type {
		None,
		Position,
		Scale,
		Color,
		Alpha
	};

	Type type = Type::None;
	float duration = 1.0f;      // アニメーション時間
	float elapsed = 0.0f;       // 経過時間
	bool loop = false;          // ループするか

	// 開始・終了値
	Vector3 startPos, endPos;
	Vector2 startScale, endScale;
	Vector4 startColor, endColor;
	float startAlpha, endAlpha;
};

///************************* UI基本クラス *************************///
class UIBase {
public:
	///************************* 基本関数 *************************///

	// コンストラクタ
	UIBase(const std::string& name = "UIBase");

	// デストラクタ
	virtual ~UIBase();

	// JSON設定ファイルから初期化
	void Initialize(const std::string& jsonConfigPath);

	// 更新処理
	virtual void Update();

	// 描画処理
	virtual void Draw();

	// ImGui表示
	void ImGUi();

private:
	///************************* 内部処理 *************************///

	// ホットリロードの有効/無効を設定
	void EnableHotReload(bool enable);

	// JSONファイルの変更を検知
	void CheckForChanges();

	// JSONデータを読み込み
	bool LoadFromJSON(const std::string& jsonPath);

public:
	///************************* 基本アクセッサ *************************///

	// 現在の設定をJSONに保存
	bool SaveToJSON(const std::string& jsonPath = "");

	// 位置を設定
	void SetPosition(const Vector3& position);

	// 位置を取得
	Vector3 GetPosition() const;

	// 回転を設定
	void SetRotation(const Vector3& rotation);

	// 回転を取得
	Vector3 GetRotation() const;

	// スケールを設定
	void SetScale(const Vector2& scale);

	// スケールを取得
	Vector2 GetScale() const;

	// 色を設定
	void SetColor(const Vector4& color);

	// 色を取得
	Vector4 GetColor() const;

	// アルファ値を設定
	void SetAlpha(float alpha);

	// アルファ値を取得
	float GetAlpha() const;

	// テクスチャを設定
	void SetTexture(const std::string& texturePath);

	// テクスチャパスを取得
	std::string GetTexturePath() const;

	// カメラを設定
	void SetCamera(Camera* camera);

	// 名前を設定
	void SetName(const std::string& name);

	// 名前を取得
	std::string GetName() const;

	// X方向反転設定
	void SetFlipX(bool flipX);

	// Y方向反転設定
	void SetFlipY(bool flipY);

	// X方向反転状態を取得
	bool GetFlipX() const;

	// Y方向反転状態を取得
	bool GetFlipY() const;

	// スプライトを取得
	Sprite* GetSprite() { return sprite_.get(); }

	// テクスチャ左上座標を設定
	void SetTextureLeftTop(const Vector2& leftTop);

	// テクスチャ左上座標を取得
	Vector2 GetTextureLeftTop() const;

	// テクスチャサイズを設定
	void SetTextureSize(const Vector2& size);

	// テクスチャサイズを取得
	Vector2 GetTextureSize() const;

	// アンカーポイントを設定
	void SetAnchorPoint(const Vector2& anchor);

	// アンカーポイントを取得
	Vector2 GetAnchorPoint() const;

	///************************* UV SRT制御 *************************///

	// UV変換を設定
	void SetUVTranslation(const Vector2& translation);

	// UV変換を取得
	Vector2 GetUVTranslation() const;

	// UV回転を設定
	void SetUVRotation(float rotation);

	// UV回転を取得
	float GetUVRotation() const;

	// UVスケールを設定
	void SetUVScale(const Vector2& scale);

	// UVスケールを取得
	Vector2 GetUVScale() const;

	///************************* グリッド・スナップ機能 *************************///

	// グリッド機能の有効/無効を設定
	void SetGridEnabled(bool enabled) { gridEnabled_ = enabled; }

	// グリッド機能が有効か取得
	bool IsGridEnabled() const { return gridEnabled_; }

	// グリッドサイズを設定
	void SetGridSize(float size) { gridSize_ = size; }

	// グリッドサイズを取得
	float GetGridSize() const { return gridSize_; }

	// 座標をグリッドにスナップ
	Vector3 SnapToGrid(const Vector3& position) const;

	///************************* 表示制御 *************************///

	// 表示/非表示を設定
	void SetVisible(bool visible) { visible_ = visible; }

	// 表示状態を取得
	bool IsVisible() const { return visible_; }

	// 描画レイヤーを設定
	void SetLayer(int layer) { layer_ = layer; }

	// 描画レイヤーを取得
	int GetLayer() const { return layer_; }

	///************************* アニメーション制御 *************************///

	// 位置アニメーションを再生
	void PlayPositionAnimation(const Vector3& from, const Vector3& to, float duration, bool loop = false);

	// スケールアニメーションを再生
	void PlayScaleAnimation(const Vector2& from, const Vector2& to, float duration, bool loop = false);

	// アルファアニメーションを再生
	void PlayAlphaAnimation(float from, float to, float duration, bool loop = false);

	// カラーアニメーションを再生
	void PlayColorAnimation(const Vector4& from, const Vector4& to, float duration, bool loop = false);

	// アニメーションを停止
	void StopAnimation();

	// アニメーション再生中か取得
	bool IsAnimating() const { return currentAnimation_.type != UIAnimation::Type::None; }

	///************************* プリセット機能 *************************///

	// 現在の設定をプリセットとして保存
	bool SaveAsPreset(const std::string& presetName);

	// プリセットを読み込み
	bool LoadPreset(const std::string& presetName);

	// 利用可能なプリセット一覧を取得
	std::vector<std::string> GetAvailablePresets() const;

	///************************* 複製機能 *************************///

	// 他のUIからプロパティを複製
	void CopyPropertiesFrom(const UIBase* other);

	// プリセットディレクトリ
	static const std::string PRESET_DIRECTORY;

protected:
	///************************* メンバ変数 *************************///

	std::unique_ptr<Sprite> sprite_;
	std::string configPath_;
	std::filesystem::file_time_type lastModTime_;
	std::string name_;
	std::string texturePath_;
	bool hotReloadEnabled_;

	bool visible_ = true;
	int layer_ = 0;

	bool gridEnabled_ = false;
	float gridSize_ = 10.0f;

	UIAnimation currentAnimation_;

	// UV SRT
	Vector2 uvTranslation_ = { 0.0f, 0.0f };
	float uvRotation_ = 0.0f;
	Vector2 uvScale_ = { 1.0f, 1.0f };

	///************************* 内部関数 *************************///

	// 現在の状態をJSON化
	nlohmann::json CreateJSONFromCurrentState();

	// JSONデータを適用
	void ApplyJSONToState(const nlohmann::json& json);

	// ファイル変更を監視
	void WatchFileChanges();

	// アニメーション更新
	void UpdateAnimation(float deltaTime);

	///************************* ImGui関連 *************************///

	// グリッド設定UI
	void ImGuiGridSettings();

	// アニメーション設定UI
	void ImGuiAnimationSettings();

	// プリセット設定UI
	void ImGuiPresetSettings();

	// クイック整列UI
	void ImGuiQuickAlignment();

	// UV SRT設定UI
	void ImGuiUVSRTSettings();
};
#pragma once

// C++
#ifdef USE_IMGUI
#include "imgui.h"
#include <d3d12.h>
#include <wrl.h>
#endif

// Forward Declaration
class WinApp;

namespace YoRigine {
class DirectXCommon;
}

// =============================================================================
//  ImGuiManager
//  ImGui の初期化・描画・終了処理を一括管理するシングルトン
//  テーマ : Unreal 風の落ち着いたダークグレー × 青 (CustomizeColor)
// =============================================================================
class ImGuiManager {
public:
  ///======================== シングルトン ========================///
  static ImGuiManager *GetInstance();
  ImGuiManager() = default;
  ~ImGuiManager() = default;

  ///======================== 基本関数 ============================///
  void Initialize(WinApp *winApp, YoRigine::DirectXCommon *dxCommon);
  void Begin();
  void End();
  void Draw();
  void Finalize();

#ifdef USE_IMGUI
  // --------- フォントアクセッサ -----------------------------------------
  // 通常フォント (FiraMono + 日本語 + アイコン)
  ImFont *GetFontNormal() const { return fontNormal_; }
  // 大フォント (ツールバー / 見出し用)
  ImFont *GetFontLarge() const { return fontLarge_; }
  // 小フォント (デバッグ情報 / ステータスバー用)
  ImFont *GetFontSmall() const { return fontSmall_; }

  // フォントサイズ (px)。見た目の大きさはここだけで調整する
  static constexpr float kFontSizeNormal = 15.0f;
  static constexpr float kFontSizeLarge = 18.0f;
  static constexpr float kFontSizeSmall = 12.0f;

  // --------- テーマカラー取得 (外部 UI でも色統一したいとき) --------------
  static ImVec4 GetAccentColor() {
    return ImVec4(0.20f, 0.45f, 0.85f, 1.00f);
  } // アクセント (青)
  static ImVec4 GetAccentColorDim() {
    return ImVec4(0.20f, 0.45f, 0.85f, 0.30f);
  } // アクセント (薄)
  static ImVec4 GetBgDeep() {
    return ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
  } // 入力欄・ボタンの地 (トグルのオフ状態など)

  // --------- 意味を持つ色 -------------------------------------------------
  // 「削除は赤」「保存・再生は緑」のように意味で色を付けるときはここから引く。
  // パネルごとに RGB
  // を直書きすると、彩度も明るさもバラバラになってテーマから浮く。
  // どれもアクセントと同じくらいの明るさ・控えめな彩度に揃えてある。
  static ImVec4 GetDangerColor() {
    return ImVec4(0.62f, 0.22f, 0.22f, 1.00f);
  } // 削除・停止・リセットなど取り返しのつかない操作
  static ImVec4 GetSuccessColor() {
    return ImVec4(0.22f, 0.50f, 0.30f, 1.00f);
  } // 保存・再生・追加
  static ImVec4 GetWarningColor() {
    return ImVec4(0.70f, 0.48f, 0.16f, 1.00f);
  } // 読み込み・上書きなど注意が要る操作
  // ホバー / 押し込み用に明るさだけ変える (a は 1
  // より大きいと明るく、小さいと暗く)
  static ImVec4 Brighten(const ImVec4 &c, float a) {
    auto f = [a](float v) { return v * a > 1.0f ? 1.0f : v * a; };
    return ImVec4(f(c.x), f(c.y), f(c.z), c.w);
  }
  static ImVec4 WithAlpha(const ImVec4 &c, float alpha) {
    return ImVec4(c.x, c.y, c.z, alpha);
  }
#endif

private:
  ///======================== 内部処理 ============================///
  void InitialzeDX12();
  void CustomizeColor();
  void CustomizeEditor();

  ///======================== シングルトン制御 =====================///
  ImGuiManager(ImGuiManager &) = delete;
  ImGuiManager &operator=(ImGuiManager &) = delete;

#ifdef USE_IMGUI
  ///======================== メンバ変数 ==========================///
  YoRigine::DirectXCommon *dxCommon_ = nullptr;
  WinApp *winApp_ = nullptr;

  ImFont *fontNormal_ = nullptr; //  14px 通常
  ImFont *fontLarge_ = nullptr;  //  18px 大見出し
  ImFont *fontSmall_ = nullptr;  //  11px 小テキスト
#endif
};
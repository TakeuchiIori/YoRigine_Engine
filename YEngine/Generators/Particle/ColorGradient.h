#pragma once
#include "Vector4.h"
#include <array>
#include <json.hpp>
#include <vector>

/// <summary>
/// 正規化時刻 [0,1] に対する色グラデーション（アルファ込み・個数無制限）。
/// エディタ表示（imgui_gradient）と実行時の色を完全に一致させるため、
/// 補間はエディタと同じ式で行い、編集時に LUT
/// へ焼いてから毎フレームはそれを引く。 ImGui に依存しないので Release
/// でも使える（CPU / GPU / VFX 共通で使う想定）。
/// </summary>
struct ColorGradient {
  /// 補間する色空間
  enum class Space : int {
    LinearRGB = 0, // 値をそのまま直線補間（旧モジュール互換。途中が濁りやすい）
    Oklab =
        1, // 知覚的に均一な空間で補間（途中が濁らず、透明へのフェードで色相が保たれる）
  };

  struct Stop {
    float t = 0.0f;                           // 0.0〜1.0
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // sRGB / ストレートアルファ
  };

  std::vector<Stop> stops = {{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
                             {1.0f, {1.0f, 1.0f, 1.0f, 0.0f}}};
  Space space = Space::Oklab;

  /// 正規化時刻 t の色を LUT から返す（毎フレーム・粒子ごとに呼ぶ用。軽い）
  Vector4 Sample(float t) const;

  /// 正規化時刻 t の色を厳密に計算する（LUT 焼き込み・プレビュー用。重い）
  Vector4 Evaluate(float t) const;

  /// stops / space を書き換えたら呼ぶ。次の Sample で LUT を焼き直す
  void MarkDirty() { lutDirty_ = true; }

  /// ストップを t 昇順に並べ直す
  void SortStops();

  /// 同じ間隔で色を並べたグラデーションを作る（旧「グラデーション」モジュールの互換読み込み用）
  static ColorGradient FromEvenColors(const Vector4 *colors, int count,
                                      Space space);

  /// {"space": int, "stops": [{"t": float, "color": [r,g,b,a]}]}
  /// "space" が無い旧データは LinearRGB として読む（見た目を変えないため）
  void SaveToJson(nlohmann::json &json) const;
  void LoadFromJson(const nlohmann::json &json);

  friend bool operator==(const ColorGradient &a, const ColorGradient &b);

private:
  static constexpr int kLutSize = 128;

  void BakeLut() const;

  // 実行時キャッシュ（シリアライズ対象外）
  mutable std::array<Vector4, kLutSize> lut_{};
  mutable bool lutDirty_ = true;
};

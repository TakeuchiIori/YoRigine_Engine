#pragma once
// ===========================================================
// YEditorWidget_Gradient.h
//
// 色グラデーション編集ウィジェット（imgui_gradient ベース）。
// ColorGradient を直接編集する。CPU / GPU / VFX のエディタ共通で使う。
//
// 操作:
//   バーをクリック      … マーカー追加（その位置の色で追加）
//   マーカーをドラッグ  … 位置移動
//   下へドラッグ / Del  … マーカー削除
//   右側の色ボタン      … 選択マーカーの色・アルファ編集
//
// 使い方:
//   if (YEditorWidget::Gradient("寿命カラー", gradient_)) { /* 変更あり */ }
// ===========================================================
#ifdef USE_IMGUI
#include "Particle/ColorGradient.h"
#include <imgui.h>

namespace YEditorWidget {

// グラデーションを編集する。stops / space が変わったら true（LUT
// の焼き直し指示も内部で行う）
bool Gradient(const char *label, ColorGradient &gradient);

} // namespace YEditorWidget
#endif // USE_IMGUI

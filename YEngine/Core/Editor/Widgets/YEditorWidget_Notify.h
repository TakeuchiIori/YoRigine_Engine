#pragma once
// ===========================================================
// YEditorWidget_Notify.h
//
// 画面右下のトースト通知（ImGuiNotify の薄いラッパー）。
// 保存・読込・エラーの結果は必ずここに出す（std::cout
// や独自トーストは使わない）。
//
// ImGuiNotify は通知リストをヘッダ内の inline 変数で持つため、呼び出し元ごとに
// 別インスタンスになりうる。エンジン内の呼び出しはこのラッパー経由に統一する。
//
// 使い方:
//   YEditorWidget::NotifySuccess("保存しました: %s", path.c_str());
//   YEditorWidget::RenderNotifications(); // Editor が毎フレーム最後に 1 回呼ぶ
// ===========================================================
#ifdef USE_IMGUI

namespace YEditorWidget {

void NotifySuccess(const char *fmt, ...);
void NotifyWarning(const char *fmt, ...);
void NotifyError(const char *fmt, ...);
void NotifyInfo(const char *fmt, ...);

// 溜まっている通知を描画する（全ウィンドウを描いた後に 1 回だけ）
void RenderNotifications();

} // namespace YEditorWidget
#endif // USE_IMGUI

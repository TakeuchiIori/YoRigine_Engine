// ===========================================================
// YEditorWidget_Notify.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Notify.h"
// サードパーティのヘッダなので警告（引数名のメンバー隠蔽・size_t→int）はここで抑える
#pragma warning(push)
#pragma warning(disable : 4458 4267)
#include <ImGuiNotify/ImGuiNotify.hpp>
#pragma warning(pop)
#include <cstdarg>
#include <cstdio>

namespace YEditorWidget {

namespace {

// 表示時間（ミリ秒）。エラーは読み切れるよう長めにする
constexpr int kDismissMs = 3000;
constexpr int kErrorDismissMs = 6000;

void Push(ImGuiToastType type, int dismissMs, const char *fmt, va_list args) {
  char buf[512];
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  ImGui::InsertNotification({type, dismissMs, "%s", buf});
}

} // namespace

void NotifySuccess(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Push(ImGuiToastType::Success, kDismissMs, fmt, args);
  va_end(args);
}

void NotifyWarning(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Push(ImGuiToastType::Warning, kErrorDismissMs, fmt, args);
  va_end(args);
}

void NotifyError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Push(ImGuiToastType::Error, kErrorDismissMs, fmt, args);
  va_end(args);
}

void NotifyInfo(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Push(ImGuiToastType::Info, kDismissMs, fmt, args);
  va_end(args);
}

void RenderNotifications() {
  // トースト自体の角丸・背景をエディタの見た目に合わせる
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.11f, 0.95f));
  ImGui::RenderNotifications();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

} // namespace YEditorWidget
#endif // USE_IMGUI

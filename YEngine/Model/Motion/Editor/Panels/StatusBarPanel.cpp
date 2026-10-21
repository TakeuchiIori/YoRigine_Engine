#include "StatusBarPanel.h"

#include "../../Authoring/MotionEditorSession.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void StatusBarPanel::DrawImGui() {
#ifdef USE_IMGUI
  constexpr float kBarHeight = 20.0f;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
  ImGui::BeginChild("##status", ImVec2(0, kBarHeight), false);

  ImGui::SetCursorPosY(3);
  ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "  %s",
                     session_->Status().c_str());

  // 右寄せでショートカットの案内
  const char *hints =
      "Space 再生  ←→ 1F  Shift+←→ キー移動  I キー  Del 削除  Ctrl+C/V  Ctrl+Z/Y";
  const float hintsWidth = ImGui::CalcTextSize(hints).x;
  const float hintsX = ImGui::GetContentRegionAvail().x - hintsWidth - 8.0f;
  if (hintsX > 300.0f) {
    ImGui::SameLine(hintsX);
    ImGui::TextDisabled("%s", hints);
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
#endif
}

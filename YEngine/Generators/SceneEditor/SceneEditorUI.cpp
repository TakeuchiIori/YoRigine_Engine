#ifdef USE_IMGUI

#include "SceneEditorUI.h"
#include "Localization/Localization.h"

#include <imgui.h>

namespace YoRigine {

SceneMenuBar::WindowFlags SceneEditorUI::BuildWindowFlags() {
  SceneMenuBar::WindowFlags flags;
  flags.outliner = &showOutliner_;
  flags.inspector = &showInspector_;
  flags.duplicate = &showDuplicate_;
  flags.prefab = &showPrefab_;
  flags.colliderOverview = &showColliderOverview_;
  flags.modelBrowser = &showModelBrowser_;
  return flags;
}

void SceneEditorUI::DrawMenuBar() {
  if (!context_.IsValid()) {
    return;
  }
  menuBar_.Draw(context_, BuildWindowFlags());
}

void SceneEditorUI::DrawPanels() {
  if (!context_.IsValid()) {
    return;
  }

  // アウトライナ → インスペクタの順に描く。
  // Editor 側のドックスペースに登録された 1 枚のパネル内で縦に並ぶため、
  // 「何を選ぶか」→「選んだものを編集する」の視線の流れに合わせている。
  if (showOutliner_) {
    if (ImGui::CollapsingHeader(loc::TrId("アウトライナ"), ImGuiTreeNodeFlags_DefaultOpen)) {
      outlinerPanel_.Draw(context_);
    }
  }

  if (showInspector_) {
    if (ImGui::CollapsingHeader(loc::TrId("インスペクタ"), ImGuiTreeNodeFlags_DefaultOpen)) {
      inspectorPanel_.Draw(context_);
    }
  }

  // ── 独立ウィンドウ ──
  if (showDuplicate_) {
    if (ImGui::Begin(loc::TrId("複製ツール"), &showDuplicate_)) {
      duplicatePanel_.Draw(context_, &showDuplicate_);
    }
    ImGui::End();
  }

  if (showPrefab_) {
    if (ImGui::Begin(loc::TrId("プレファブ"), &showPrefab_)) {
      prefabPanel_.Draw(context_, &showPrefab_);
    }
    ImGui::End();
  }

  if (showColliderOverview_) {
    if (ImGui::Begin(loc::TrId("コライダー一覧"), &showColliderOverview_)) {
      colliderPanel_.DrawOverviewWindow(context_, &showColliderOverview_);
    }
    ImGui::End();
  }
}

} // namespace YoRigine

#endif // USE_IMGUI

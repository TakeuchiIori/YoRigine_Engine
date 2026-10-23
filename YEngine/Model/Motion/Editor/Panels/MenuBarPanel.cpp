#include "MenuBarPanel.h"

#include "../../Authoring/MotionEditorSession.h"
#include "../MotionEditorUIState.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void MenuBarPanel::DrawImGui() {
#ifdef USE_IMGUI
  if (!ImGui::BeginMenuBar()) {
    return;
  }

  if (ImGui::BeginMenu("ファイル")) {
    if (ImGui::MenuItem("glTF からアニメーションを読み込む...")) {
      ui_->showSourceDialog = true;
    }
    if (ImGui::MenuItem(".anim の保存 / 読み込み...")) {
      ui_->showFileDialog = true;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("編集")) {
    if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, session_->CanUndo())) {
      session_->Undo();
    }
    if (ImGui::MenuItem("やり直す", "Ctrl+Y", false, session_->CanRedo())) {
      session_->Redo();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("選択ボーンにキーを打つ", "I", false,
                        !session_->SelectedBone().empty())) {
      session_->InsertKeyForSelectedBone();
    }
    if (ImGui::MenuItem("全ボーンにキーを打つ", "Shift+I", false,
                        session_->CurrentMotion() != nullptr)) {
      session_->InsertKeyForAllBones();
    }
    if (ImGui::MenuItem("キーを削除", "Del")) {
      session_->DeleteSelectedKey();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("このモーションの履歴をクリア")) {
      session_->ClearHistory();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("表示")) {
    ImGui::MenuItem("ボーンを表示", "B", &ui_->drawBones);
    ImGui::SliderFloat("タイムライン高さ", &ui_->timelineHeight, 80.0f, 600.0f,
                       "%.0f px");
    ImGui::SliderFloat("ボーンリスト幅", &ui_->boneListWidth, 80.0f, 450.0f,
                       "%.0f px");
    ImGui::EndMenu();
  }

  // メニューバー右側の Undo / Redo ボタン
  ImGui::Separator();
  ImGui::BeginDisabled(!session_->CanUndo());
  if (ImGui::SmallButton(" << ")) {
    session_->Undo();
  }
  ImGui::EndDisabled();
  ImGui::SameLine(0, 2);
  ImGui::BeginDisabled(!session_->CanRedo());
  if (ImGui::SmallButton(" >> ")) {
    session_->Redo();
  }
  ImGui::EndDisabled();

  ImGui::EndMenuBar();
#endif
}

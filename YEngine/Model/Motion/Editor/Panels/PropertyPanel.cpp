#include "PropertyPanel.h"

#include "../../Authoring/MotionEditorSession.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>

void PropertyPanel::DrawImGui() {
#ifdef USE_IMGUI
  if (!ImGui::CollapsingHeader("ボーン姿勢", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  const std::string &bone = session_->SelectedBone();
  if (bone.empty()) {
    ImGui::TextDisabled("左のリストか 3D ビューでボーンを選択してください");
    return;
  }

  ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "選択中: %s",
                     bone.c_str());
  ImGui::Separator();

  // ------------------------------------------------------------
  // 姿勢の編集（変更したら即ボーンへ一時適用）
  // ------------------------------------------------------------
  PoseBuffer &pose = session_->Pose();
  bool changed = false;

  // 位置は表示単位で編集する（Mixamo は cm → m 表示）
  const float displayScale = std::max(0.0001f, session_->TranslateDisplayScale());
  float displayT[3] = {pose.translate[0] / displayScale,
                       pose.translate[1] / displayScale,
                       pose.translate[2] / displayScale};
  if (ImGui::DragFloat3("位置 (T)", displayT, 0.01f)) {
    for (int i = 0; i < 3; ++i) {
      pose.translate[i] = displayT[i] * displayScale;
    }
    changed = true;
  }
  changed |= ImGui::DragFloat3("回転 (R)", pose.rotateDeg, 0.5f);
  changed |= ImGui::DragFloat3("拡縮 (S)", pose.scale, 0.01f, 0.001f, 100.0f);

  if (changed) {
    session_->PushPoseToJoint();
  }

  ImGui::Spacing();
  ImGui::BeginDisabled(!session_->CanRevertLiveEdit());
  if (ImGui::Button("一時編集を元に戻す", ImVec2(-1, 0))) {
    session_->RevertLiveEdit();
  }
  ImGui::EndDisabled();

  if (!session_->CurrentMotion()) {
    return;
  }

  // ------------------------------------------------------------
  // キー操作
  // ------------------------------------------------------------
  ImGui::Spacing();
  ImGui::Text("現在時刻: %.4f 秒 (%d f)", session_->Playback().Time(),
              MotionOps::TimeToFrame(session_->Playback().Time(),
                                     MotionEditorSession::kFps));
  if (ImGui::Button("キーを打つ  [I]", ImVec2(-1, 0))) {
    session_->InsertKeyForSelectedBone();
  }

  // ------------------------------------------------------------
  // コピー & ペースト
  // ------------------------------------------------------------
  ImGui::Spacing();
  ImGui::Separator();
  const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 4.0f;
  if (ImGui::Button("コピー (Ctrl+C)", ImVec2(halfWidth, 0))) {
    session_->CopyPose();
  }
  ImGui::SameLine(0, 8);
  const bool canPaste = session_->Clipboard().hasData;
  ImGui::BeginDisabled(!canPaste);
  if (ImGui::Button("ペースト (Ctrl+V)", ImVec2(-1, 0))) {
    session_->PastePose(false);
  }
  if (ImGui::Button("ペースト + キー (Ctrl+Shift+V)", ImVec2(-1, 0))) {
    session_->PastePose(true);
  }
  ImGui::EndDisabled();

  if (canPaste) {
    ImGui::TextDisabled("クリップボード: %s @ %.3fs",
                        session_->Clipboard().sourceBone.c_str(),
                        session_->Clipboard().sourceTime);
  }
#endif
}

#include "ToolbarPanel.h"

#include "../../Authoring/MotionEditorSession.h"
#include "../../Core/Motion.h"
#include "../MotionEditorUIState.h"
#include "Model.h"

#ifdef USE_IMGUI
#include <IconsFontAwesome5.h>
#include <imgui.h>
#endif

#include <string>

void ToolbarPanel::DrawImGui() {
#ifdef USE_IMGUI
  DrawPlaybackRow();
  DrawSettingsRow();
  if (session_->CurrentMotion()) {
    DrawUtilityRow();
  }
#endif
}

// ============================================================
// 1行目: 再生コントロール
// ============================================================
void ToolbarPanel::DrawPlaybackRow() {
#ifdef USE_IMGUI
  MotionPlayback &playback = session_->Playback();

  if (YoRigine::Model *model = session_->TargetModel()) {
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "[%s]",
                       model->GetName().c_str());
    ImGui::SameLine(0, 12);
  }

  if (ImGui::Button(ICON_FA_STEP_BACKWARD)) {
    session_->JumpToPrevKey();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("前のキーへ (Shift+←)");
  ImGui::SameLine(0, 2);

  if (playback.IsPlaying()) {
    if (ImGui::Button(ICON_FA_PAUSE " 一時停止")) {
      session_->Pause();
    }
  } else {
    if (ImGui::Button(ICON_FA_PLAY " 再生")) {
      session_->Play();
    }
  }
  ImGui::SameLine(0, 2);

  if (ImGui::Button(ICON_FA_STEP_FORWARD)) {
    session_->JumpToNextKey();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("次のキーへ (Shift+→)");
  ImGui::SameLine(0, 2);

  if (ImGui::Button(ICON_FA_STOP " 停止")) {
    session_->StopPlayback();
  }

  // トグル系はオンのとき色を付ける
  auto toggleButton = [](const char *label, bool on, const ImVec4 &color) {
    if (on) ImGui::PushStyleColor(ImGuiCol_Button, color);
    const bool pressed = ImGui::Button(label);
    if (on) ImGui::PopStyleColor();
    return pressed;
  };

  ImGui::SameLine(0, 12);
  if (toggleButton(ICON_FA_SYNC_ALT " ループ", playback.IsLoop(),
                   ImVec4(0.2f, 0.45f, 0.25f, 1.0f))) {
    playback.SetLoop(!playback.IsLoop());
  }
  ImGui::SameLine(0, 4);
  if (toggleButton(ICON_FA_UNDO " 逆再生", playback.IsReverse(),
                   ImVec4(0.7f, 0.2f, 0.2f, 1.0f))) {
    playback.SetReverse(!playback.IsReverse());
  }
  ImGui::SameLine(0, 4);
  if (toggleButton(ICON_FA_EXCHANGE_ALT " ピンポン", playback.IsPingPong(),
                   ImVec4(0.2f, 0.5f, 0.7f, 1.0f))) {
    playback.SetPingPong(!playback.IsPingPong());
  }

  ImGui::SameLine(0, 12);
  ImGui::Checkbox("ボーン表示", &ui_->drawBones);
  ImGui::SameLine(0, 12);
  if (ImGui::Button(ICON_FA_SAVE " 保存/読込")) {
    ui_->showFileDialog = true;
  }
#endif
}

// ============================================================
// 2行目: 速度 / トリム
// ============================================================
void ToolbarPanel::DrawSettingsRow() {
#ifdef USE_IMGUI
  MotionPlayback &playback = session_->Playback();

  float speed = playback.Speed();
  ImGui::SetNextItemWidth(120);
  if (ImGui::DragFloat("再生速度", &speed, 0.01f, 0.0f, 3.0f, "%.2fx")) {
    playback.SetSpeed(speed);
  }
  ImGui::SameLine(0, 4);
  for (float preset : {0.25f, 0.5f, 1.0f}) {
    const std::string label = std::to_string(preset).substr(0, 4) + "x";
    if (ImGui::SmallButton(label.c_str())) {
      playback.SetSpeed(preset);
    }
    ImGui::SameLine(0, 2);
  }

  ImGui::SameLine(0, 20);
  ImGui::Checkbox("トリム", &ui_->useTrim);
  if (ui_->useTrim) {
    if (Motion *motion = session_->CurrentMotion()) {
      const float duration = motion->GetDuration();
      if (ui_->trimEnd <= 0.0f) {
        ui_->trimEnd = duration;
      }
      ImGui::SameLine(0, 8);
      ImGui::SetNextItemWidth(200);
      ImGui::DragFloatRange2("範囲", &ui_->trimStart, &ui_->trimEnd, 0.01f,
                             0.0f, duration, "%.3f s", "%.3f s");
      ImGui::SameLine(0, 8);
      if (ImGui::Button("切り出し")) {
        session_->TrimMotion(ui_->trimStart, ui_->trimEnd);
        ui_->useTrim = false;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("範囲を別モーションとして切り出します");
      }
    }
  }
#endif
}

// ============================================================
// 3行目: モーション加工
// ============================================================
void ToolbarPanel::DrawUtilityRow() {
#ifdef USE_IMGUI
  if (ImGui::Button("ミラー生成 (左右反転)")) {
    session_->MirrorMotion();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("X軸の位置とY/Z回転を反転し、左右のボーンを入れ替えた\n"
                      "ミラーモーションを生成します");
  }

  ImGui::SameLine(0, 8);
  if (ImGui::Button("ピンポン結合")) {
    session_->MakePingPongMotion();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("正再生+逆再生を結合した 1 つのモーションを生成します");
  }

  if (ImGui::TreeNode("区間移動の焼き込み")) {
    ImGui::SetNextItemWidth(110);
    ImGui::InputInt("開始f##offsetStart", &offsetStartFrame_);
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(110);
    ImGui::InputInt("終了f##offsetEnd", &offsetEndFrame_);
    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(220);
    ImGui::DragFloat3("移動量XYZ##offsetTranslate", offsetTranslate_, 0.01f);
    ImGui::SameLine(0, 8);

    ImGui::BeginDisabled(session_->SelectedBone().empty());
    if (ImGui::Button("選択ボーンへ焼き込み")) {
      session_->BakeTranslateOffset(
          offsetStartFrame_, offsetEndFrame_,
          {offsetTranslate_[0], offsetTranslate_[1], offsetTranslate_[2]});
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("開始直前/開始/終了/終了直後に Translate キーを作り、\n"
                        "指定区間だけ移動量を足します");
    }
    ImGui::TreePop();
  }
#endif
}

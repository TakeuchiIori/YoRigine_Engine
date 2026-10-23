#include "SavePanel.h"

#include "../../Authoring/MotionEditorSession.h"
#include "../../Authoring/MotionFiles.h"
#include "../MotionEditorUIState.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <cstring>
#include <exception>

namespace {
#ifdef USE_IMGUI
// std::string を ImGui::InputText で編集する（変更されたら true）
bool InputString(const char *label, std::string &value, float width) {
  char buffer[512];
  strncpy_s(buffer, sizeof(buffer), value.c_str(), _TRUNCATE);
  ImGui::SetNextItemWidth(width);
  if (ImGui::InputText(label, buffer, sizeof(buffer))) {
    value = buffer;
    return true;
  }
  return false;
}

// 成功は緑、失敗は赤でメッセージを出す
void DrawResultMessage(const std::string &message) {
  if (message.empty()) {
    return;
  }
  const bool isError = message.find("失敗") != std::string::npos ||
                       message.find("見つかりません") != std::string::npos ||
                       message.find("開けません") != std::string::npos;
  ImGui::TextColored(isError ? ImVec4(1, 0.3f, 0.3f, 1)
                             : ImVec4(0.3f, 1, 0.3f, 1),
                     "%s", message.c_str());
}
#endif
} // namespace

void SavePanel::OnInitialize() {
#ifdef USE_IMGUI
  animBrowser_ = YoRigine::FileBrowser(MotionFiles::kAnimDirectory, {".anim"},
                                       YoRigine::FileBrowser::DisplayMode::List);
  // ダブルクリックで選んだら、そのまま読み込む
  animBrowser_.SetOnFileSelected([this](const std::string &fullPath) {
    animPath_ = fullPath;
    animBrowserOpen_ = false;
    session_->LoadAnimFile(fullPath);
  });

  sourceBrowser_ = YoRigine::FileBrowser(MotionFiles::kModelDirectory, {".gltf"},
                                         YoRigine::FileBrowser::DisplayMode::List);
  sourceBrowser_.SetOnFileSelected([this](const std::string &fullPath) {
    SelectSourceModel(fullPath);
    sourceBrowserOpen_ = false;
    ui_->showSourceDialog = true;
  });
#endif
}

void SavePanel::DrawImGui() {
#ifdef USE_IMGUI
  if (animBrowserOpen_) {
    DrawAnimBrowserWindow();
  }
  if (sourceBrowserOpen_) {
    DrawSourceBrowserWindow();
  }
  DrawSourceDialog();
  DrawFileDialog();
#endif
}

void SavePanel::SelectSourceModel(const std::string &path) {
  sourceModelPath_ = path;
  sourceObjectPath_ = MotionFiles::ToObjectModelPath(path);
  sourceAnimationNames_.clear();
  sourceAnimationName_.clear();
  try {
    sourceAnimationNames_ = MotionFiles::ListGltfAnimationNames(path);
    sourceAnimationName_ = sourceAnimationNames_.front();
    sourceMessage_ =
        "候補取得: " + std::to_string(sourceAnimationNames_.size()) + "件";
  } catch (const std::exception &e) {
    sourceMessage_ = std::string("候補取得失敗: ") + e.what();
  }
}

// ============================================================
// .anim の保存 / 読み込み
// ============================================================
void SavePanel::DrawFileDialog() {
#ifdef USE_IMGUI
  const char *popupId = ".anim の保存 / 読み込み##motionFile";
  if (ui_->showFileDialog && !ImGui::IsPopupOpen(popupId)) {
    ImGui::OpenPopup(popupId);
  }

  ImGui::SetNextWindowSize(ImVec2(560, 300), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(popupId, &ui_->showFileDialog)) {
    return;
  }

  ImGui::Text("編集中: %s%s", session_->CurrentDisplayName().c_str(),
              (session_->Document() && session_->Document()->IsDirty())
                  ? "  (未保存の変更あり)"
                  : "");
  ImGui::Spacing();

  ImGui::Text("ファイル (.anim):");
  InputString("##animPath", animPath_, ImGui::GetContentRegionAvail().x - 90);
  ImGui::SameLine();
  if (ImGui::Button("参照##anim")) {
    animBrowser_.Scan();
    animBrowser_.SetSelectedPath("");
    animBrowserOpen_ = true;
    ui_->showFileDialog = false;
    ImGui::CloseCurrentPopup();
  }

  // ランタイムが自動で読み込むファイル名を案内する
  // （キャッシュキー "<glTFパス>#<アニメ名>" から Model::LoadMotionFile と同じ規則で作る）
  const std::string &key = session_->CurrentKey();
  if (const auto hash = key.find('#'); hash != std::string::npos) {
    const std::string runtimePath = MotionFiles::RuntimeAnimPath(
        key.substr(0, hash), key.substr(hash + 1));
    ImGui::TextDisabled("ゲームが読む名前: %s", runtimePath.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("これにする")) {
      animPath_ = runtimePath;
    }
  }

  ImGui::Spacing();
  ImGui::Checkbox("時間を反転して保存（逆再生版を書き出す）", &saveTimeReversed_);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::BeginDisabled(session_->CurrentMotion() == nullptr);
  if (ImGui::Button("保存", ImVec2(130, 0))) {
    session_->SaveAnimFile(animPath_, saveTimeReversed_);
  }
  ImGui::EndDisabled();
  ImGui::SameLine(0, 10);
  if (ImGui::Button("読み込み", ImVec2(130, 0))) {
    session_->LoadAnimFile(animPath_);
  }

  ImGui::Spacing();
  DrawResultMessage(session_->Status());

  ImGui::Spacing();
  ImGui::Separator();
  if (ImGui::Button("閉じる", ImVec2(100, 0))) {
    ui_->showFileDialog = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
#endif
}

// ============================================================
// glTF からの読み込み
// ============================================================
void SavePanel::DrawSourceDialog() {
#ifdef USE_IMGUI
  const char *popupId = "glTF からアニメーションを読み込む##motionSource";
  if (ui_->showSourceDialog && !ImGui::IsPopupOpen(popupId)) {
    ImGui::OpenPopup(popupId);
  }

  ImGui::SetNextWindowSize(ImVec2(560, 330), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(popupId, &ui_->showSourceDialog)) {
    return;
  }

  ImGui::Text("モデルファイル (.gltf):");
  if (InputString("##sourceModelPath", sourceModelPath_,
                  ImGui::GetContentRegionAvail().x - 90)) {
    sourceObjectPath_ = MotionFiles::ToObjectModelPath(sourceModelPath_);
    sourceAnimationNames_.clear();
  }
  ImGui::SameLine();
  if (ImGui::Button("参照##source")) {
    sourceBrowser_.Scan();
    sourceBrowser_.SetSelectedPath("");
    sourceBrowserOpen_ = true;
    ui_->showSourceDialog = false;
    ImGui::CloseCurrentPopup();
  }

  ImGui::Spacing();
  if (ImGui::Button("候補取得", ImVec2(100, 0))) {
    SelectSourceModel(sourceModelPath_);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%s", sourceObjectPath_.c_str());

  ImGui::Spacing();
  ImGui::Text("アニメーション:");
  if (sourceAnimationNames_.empty()) {
    ImGui::TextDisabled("候補なし");
  } else {
    const char *preview =
        sourceAnimationName_.empty() ? "(未選択)" : sourceAnimationName_.c_str();
    if (ImGui::BeginCombo("##sourceAnimationName", preview)) {
      for (const auto &name : sourceAnimationNames_) {
        const bool selected = (name == sourceAnimationName_);
        if (ImGui::Selectable(name.c_str(), selected)) {
          sourceAnimationName_ = name;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
  }
  InputString("名前を直接入力##sourceAnimInput", sourceAnimationName_,
              ImGui::GetContentRegionAvail().x);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const bool hasTarget = session_->HasTarget();
  ImGui::BeginDisabled(!hasTarget || sourceObjectPath_.empty() ||
                       sourceAnimationName_.empty());
  if (ImGui::Button("読み込んで適用", ImVec2(140, 0))) {
    session_->LoadSourceAnimation(sourceObjectPath_, sourceAnimationName_);
    sourceMessage_ = session_->Status();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(!hasTarget || sourceObjectPath_.empty() ||
                       sourceAnimationNames_.empty());
  if (ImGui::Button("全て読み込む", ImVec2(120, 0))) {
    session_->LoadAllSourceAnimations(sourceObjectPath_, sourceAnimationNames_,
                                      sourceAnimationName_);
    sourceMessage_ = session_->Status();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  if (ImGui::Button("閉じる", ImVec2(100, 0))) {
    ui_->showSourceDialog = false;
    sourceMessage_.clear();
    ImGui::CloseCurrentPopup();
  }

  ImGui::Spacing();
  DrawResultMessage(sourceMessage_);
  ImGui::EndPopup();
#endif
}

// ============================================================
// ファイルブラウザ（独立ウィンドウ）
// ============================================================
void SavePanel::DrawAnimBrowserWindow() {
#ifdef USE_IMGUI
  ImGui::SetNextWindowSize(ImVec2(540, 420), ImGuiCond_FirstUseEver);
  bool open = true;
  if (ImGui::Begin(".anim を選択##animBrowser", &open)) {
    animBrowser_.Draw("##animBrowserList", ImVec2(0, -50));
    ImGui::Separator();

    const std::string &selected = animBrowser_.GetSelectedPath();
    ImGui::TextUnformatted(selected.empty() ? "(未選択)" : selected.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120.0f);

    ImGui::BeginDisabled(selected.empty());
    if (ImGui::Button("OK", ImVec2(55, 0))) {
      animPath_ = selected;
      animBrowserOpen_ = false;
      ui_->showFileDialog = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("キャンセル", ImVec2(60, 0))) {
      animBrowserOpen_ = false;
      ui_->showFileDialog = true;
    }
  }
  ImGui::End();
  if (!open) {
    animBrowserOpen_ = false;
  }
#endif
}

void SavePanel::DrawSourceBrowserWindow() {
#ifdef USE_IMGUI
  ImGui::SetNextWindowSize(ImVec2(560, 430), ImGuiCond_FirstUseEver);
  bool open = true;
  if (ImGui::Begin("glTF を選択##sourceBrowser", &open)) {
    sourceBrowser_.Draw("##sourceBrowserList", ImVec2(0, -50));
    ImGui::Separator();

    const std::string &selected = sourceBrowser_.GetSelectedPath();
    ImGui::TextUnformatted(selected.empty() ? "(未選択)" : selected.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120.0f);

    ImGui::BeginDisabled(selected.empty());
    if (ImGui::Button("OK", ImVec2(55, 0))) {
      SelectSourceModel(selected);
      sourceBrowserOpen_ = false;
      ui_->showSourceDialog = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("キャンセル", ImVec2(60, 0))) {
      sourceBrowserOpen_ = false;
      ui_->showSourceDialog = true;
    }
  }
  ImGui::End();
  if (!open) {
    sourceBrowserOpen_ = false;
  }
#endif
}

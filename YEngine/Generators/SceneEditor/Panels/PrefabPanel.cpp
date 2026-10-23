#ifdef USE_IMGUI

#include "PrefabPanel.h"
#include "Localization/Localization.h"

// Engine
#include "../ObjectSelector.h"
#include "../PrefabManager.h"

// C++
#include <cstring>

namespace YoRigine {

void PrefabPanel::DrawCreateSection(const ScenePanelContext &context) {
  PrefabManager &prefabManager = *context.scene->prefabManager;
  ObjectSelector &selector = *context.scene->selector;

  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##prefabName", TR("プレファブ名"), newPrefabName_,
                           sizeof(newPrefabName_));

  const bool hasName = (std::strlen(newPrefabName_) > 0);
  if (!hasName) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button(TR("選択オブジェクトから作成"))) {
    prefabManager.CreatePrefabFromObject(newPrefabName_,
                                         selector.GetPrimaryId());
    std::memset(newPrefabName_, 0, sizeof(newPrefabName_));
  }
  ImGui::SameLine();
  if (ImGui::Button(TR("シーン全体から作成"))) {
    prefabManager.CreatePrefabFromAllObjects(newPrefabName_);
    std::memset(newPrefabName_, 0, sizeof(newPrefabName_));
  }

  if (!hasName) {
    ImGui::EndDisabled();
    ImGui::TextDisabled(TR("名前を入力すると作成できる"));
  }
}

void PrefabPanel::DrawLibrarySection(const ScenePanelContext &context) {
  PrefabManager &prefabManager = *context.scene->prefabManager;

  if (ImGui::Button(TR("一覧を更新"))) {
    prefabManager.ScanPrefabFolder();
  }
  ImGui::SameLine();
  searchBox_.Draw("##prefabSearch", TR("絞り込み..."), 180.0f);

  if (ImGui::BeginListBox("##prefabList", ImVec2(-1.0f, 180.0f))) {
    for (const auto &name : prefabManager.GetPrefabList()) {
      if (!searchBox_.Matches(name)) {
        continue;
      }
      if (ImGui::Selectable(name.c_str(), selectedPrefabName_ == name)) {
        selectedPrefabName_ = name;
      }
    }
    ImGui::EndListBox();
  }

  if (selectedPrefabName_.empty()) {
    ImGui::TextDisabled(TR("プレファブを選ぶと配置・削除できる"));
    return;
  }

  if (ImGui::Button(TR("シーンに配置"))) {
    prefabManager.LoadPrefab(selectedPrefabName_);
  }
  ImGui::SameLine();
  if (ImGui::Button(TR("削除"))) {
    ImGui::OpenPopup("##confirmPrefabDelete");
  }
  if (ImGui::BeginPopup("##confirmPrefabDelete")) {
    ImGui::Text(TR("%s を削除します。"), selectedPrefabName_.c_str());
    if (ImGui::Button(TR("削除する"))) {
      prefabManager.DeletePrefab(selectedPrefabName_);
      selectedPrefabName_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("やめる"))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void PrefabPanel::Draw(const ScenePanelContext &context, bool *isOpen) {
  if (!context.IsValid() || !context.scene->prefabManager ||
      !context.scene->selector) {
    return;
  }

  ImGui::TextUnformatted(TR("プレファブ"));
  ImGui::Separator();

  if (ImGui::CollapsingHeader(loc::TrId("作成"), ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawCreateSection(context);
  }
  if (ImGui::CollapsingHeader(loc::TrId("ライブラリ"), ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawLibrarySection(context);
  }

  ImGui::Separator();
  if (ImGui::Button(TR("閉じる")) && isOpen) {
    *isOpen = false;
  }
}

} // namespace YoRigine

#endif // USE_IMGUI

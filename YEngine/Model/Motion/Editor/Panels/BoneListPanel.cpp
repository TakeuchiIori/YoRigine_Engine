#include "BoneListPanel.h"

#include "../../Authoring/MotionEditorSession.h"
#include "Skeleton/Joint.h"
#include "Skeleton/Skeleton.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cctype>

namespace {
// 大文字小文字を無視した部分一致
bool ContainsIgnoreCase(const std::string &text, const char *pattern) {
  if (!pattern || pattern[0] == '\0') {
    return true;
  }
  std::string lowerText = text;
  std::string lowerPattern = pattern;
  auto toLower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
  std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), toLower);
  std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(),
                 toLower);
  return lowerText.find(lowerPattern) != std::string::npos;
}
} // namespace

void BoneListPanel::DrawImGui() {
#ifdef USE_IMGUI
  if (!ImGui::CollapsingHeader("ボーン", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  Skeleton *skeleton = session_->TargetSkeleton();
  if (!skeleton) {
    ImGui::TextDisabled("スケルトンなし");
    return;
  }
  auto &joints = skeleton->GetJoints();

  // 絞り込み
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##boneFilter", "ボーン名で絞り込み", filter_,
                           sizeof(filter_));

  ImGui::BeginChild("BoneTreeRegion",
                    ImVec2(0, ImGui::GetContentRegionAvail().y - 30.0f), true,
                    ImGuiWindowFlags_HorizontalScrollbar);

  if (filter_[0] != '\0') {
    // 絞り込み中は階層をやめて一致したボーンだけを平らに並べる
    for (const Joint &joint : joints) {
      const std::string &name = joint.GetName();
      if (!ContainsIgnoreCase(name, filter_)) {
        continue;
      }
      if (ImGui::Selectable(name.c_str(), session_->SelectedBone() == name)) {
        session_->SelectBone(name);
      }
    }
  } else {
    // 親子関係のマップを作ってツリーで描く
    std::vector<std::vector<int>> childrenMap(joints.size());
    std::vector<int> rootJoints;
    for (int i = 0; i < static_cast<int>(joints.size()); ++i) {
      auto parent = joints[i].GetParent();
      if (parent.has_value()) {
        childrenMap[parent.value()].push_back(i);
      } else {
        rootJoints.push_back(i);
      }
    }
    for (int rootIndex : rootJoints) {
      DrawBoneNode(joints, rootIndex, childrenMap);
    }
  }

  ImGui::EndChild();

  ImGui::Separator();
  if (ImGui::SmallButton("選択解除")) {
    session_->ClearBoneSelection();
  }
#endif
}

void BoneListPanel::DrawBoneNode(std::vector<Joint> &joints, int jointIndex,
                                 const std::vector<std::vector<int>> &childrenMap) {
#ifdef USE_IMGUI
  const std::string &name = joints[jointIndex].GetName();
  const bool isSelected = (session_->SelectedBone() == name);
  const auto &children = childrenMap[jointIndex];

  // 矢印クリックで開閉、名前クリックで選択
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                             ImGuiTreeNodeFlags_SpanAvailWidth;
  if (isSelected) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  if (children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  } else {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  if (isSelected) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
  }
  const bool isOpen = ImGui::TreeNodeEx(
      reinterpret_cast<void *>(static_cast<intptr_t>(jointIndex)), flags, "%s",
      name.c_str());
  if (isSelected) {
    ImGui::PopStyleColor();
    // 3D ビューで選んだときもリスト上で見えるよう、選択が変わった時だけスクロールする
    if (scrolledToBone_ != name) {
      if (!ImGui::IsItemVisible()) {
        ImGui::SetScrollHereY();
      }
      scrolledToBone_ = name;
    }
  }

  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    session_->SelectBone(name);
  }

  if (isOpen && !children.empty()) {
    for (int childIndex : children) {
      DrawBoneNode(joints, childIndex, childrenMap);
    }
    ImGui::TreePop();
  }
#else
  (void)joints;
  (void)jointIndex;
  (void)childrenMap;
#endif
}

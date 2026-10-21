#pragma once
#include "IMotionEditorPanel.h"

#include <string>
#include <vector>

class Joint;

// スケルトンの階層をツリー表示し、編集するボーンを選ぶパネル
class BoneListPanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;

private:
  void DrawBoneNode(std::vector<Joint> &joints, int jointIndex,
                    const std::vector<std::vector<int>> &childrenMap);

  // ボーン名の絞り込み
  char filter_[64] = {};
  // 最後に自動スクロールしたボーン（選択変化の検出用）
  std::string scrolledToBone_;
};

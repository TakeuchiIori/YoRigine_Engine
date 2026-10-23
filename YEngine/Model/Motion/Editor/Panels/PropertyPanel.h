#pragma once
#include "IMotionEditorPanel.h"

// 選択ボーンの姿勢（位置・回転・拡縮）を編集するパネル
class PropertyPanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;
};

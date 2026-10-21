#pragma once
#include "IMotionEditorPanel.h"

// 下端のステータスバー（直近の操作結果・時刻・ショートカット案内）
class StatusBarPanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;
};

#pragma once
#include "IMotionEditorPanel.h"

// 再生コントロールと、モーション加工（ミラー/トリム/ピンポン/区間移動）のツールバー
class ToolbarPanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;

private:
  void DrawPlaybackRow();
  void DrawSettingsRow();
  void DrawUtilityRow();

  // 区間移動の入力値
  int offsetStartFrame_ = 2;
  int offsetEndFrame_ = 10;
  float offsetTranslate_[3] = {0.0f, 1.0f, 0.0f};
};

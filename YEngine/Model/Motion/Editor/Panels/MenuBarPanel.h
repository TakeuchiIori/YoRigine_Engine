#pragma once
#include "IMotionEditorPanel.h"

// メニューバー（ファイル / 編集 / 表示）と Undo/Redo ボタン
class MenuBarPanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;
};

#pragma once

class MotionEditorSession;
struct MotionEditorUIState;

// ============================================================
// モーションエディタのパネル基底
//
// ・パネルは「描画」と「入力を Session の関数呼び出しに変換する」ことだけを行う
// ・モーションデータやスケルトンを直接書き換えない
// ============================================================
class IMotionEditorPanel {
public:
  virtual ~IMotionEditorPanel() = default;

  void Initialize(MotionEditorSession *session, MotionEditorUIState *ui) {
    session_ = session;
    ui_ = ui;
    OnInitialize();
  }

  // ImGui による描画（USE_IMGUI が無いビルドでは何もしない）
  virtual void DrawImGui() = 0;

protected:
  // 派生クラス固有の初期化
  virtual void OnInitialize() {}

  MotionEditorSession *session_ = nullptr;
  MotionEditorUIState *ui_ = nullptr;
};

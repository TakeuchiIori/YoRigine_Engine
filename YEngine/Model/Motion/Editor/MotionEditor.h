#pragma once

// Engine
#include "Object3d/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include <Graphics/Drawer/LineManager/Line.h>
#ifdef USE_IMGUI
#include "Debugger/Gizmo/GizmoController.h"
#endif

// Motion
#include "../Authoring/MotionEditorSession.h"
#include "MotionEditorUIState.h"

// Standard
#include <memory>
#include <string>
#include <vector>

// ============================================================
// 前方宣言
// ============================================================
namespace YoRigine {
class Camera;
}
class MenuBarPanel;
class SavePanel;
class ToolbarPanel;
class BoneListPanel;
class PropertyPanel;
class TimelinePanel;
class SpeedCurvePanel;
class StatusBarPanel;
#ifdef USE_IMGUI
class BoneGizmable;
#endif

// ============================================================
// モーションエディタ
//
// ・編集処理はすべて MotionEditorSession が持つ
// ・このクラスはウィンドウの組み立て、ショートカット、3D ビューでの
//   ボーン表示・ピッキング・ギズモだけを担当する
// ============================================================
class MotionEditor {
public:
  MotionEditor();
  ~MotionEditor();

  void Initialize(YoRigine::Camera *camera);
  void Update();
  void Draw();
  void DrawGizmo();
  void DrawBone();
  void ShowEditor();

  // シーンエディタ側から「このエディタを使うかどうか」を切り替える。
  // 無効にすると UI・ボーン描画・ギズモを止め、編集対象を手放す。
  void SetEnabled(bool enabled);
  bool IsEnabled() const { return isEnabled_; }

  void SetTargetObjectId(int id) { session_.SetTargetObjectId(id); }
  int GetTargetObjectId() const { return session_.TargetObjectId(); }
  void SetCamera(YoRigine::Camera *camera);
  bool IsDrawBone() const { return isEnabled_ && ui_.drawBones; }

  // BoneGizmable から呼ばれる
  Matrix4x4 GetJointWorldMatrix(const std::string &boneName) const;
  void ApplyBoneGizmoTransform(const std::string &boneName,
                               const Matrix4x4 &newWorldMat);

private:
  void HandleShortcuts();
  void HandleBonePicking();
  void DrawLayout();

  MotionEditorSession session_;
  MotionEditorUIState ui_;

  // パネル（描画順に意味があるので名前で持つ）
  std::unique_ptr<MenuBarPanel> menuBar_;
  std::unique_ptr<SavePanel> filePanel_;
  std::unique_ptr<ToolbarPanel> toolbar_;
  std::unique_ptr<SpeedCurvePanel> speedCurve_;
  std::unique_ptr<BoneListPanel> boneList_;
  std::unique_ptr<PropertyPanel> property_;
  std::unique_ptr<TimelinePanel> timeline_;
  std::unique_ptr<StatusBarPanel> statusBar_;

  YoRigine::Camera *camera_ = nullptr;
  std::unique_ptr<YoRigine::Line> lineDrawer_;

#ifdef USE_IMGUI
  std::unique_ptr<YoRigine::Object3d> boneObj_;
  GizmoController gizmoCtrl_;
  // ピッキング候補（クリックのたびに作り直す）
  std::vector<std::unique_ptr<BoneGizmable>> pickables_;
  // 選択ボーンのギズモ
  std::unique_ptr<BoneGizmable> selectedGizmo_;
  std::vector<YoRigine::WorldTransform> boneWorldTransforms_;
#endif

  // シーンエディタから明示的に有効化されるまで何もしない
  bool isEnabled_ = false;
};

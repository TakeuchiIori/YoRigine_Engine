#include "MotionEditor.h"

// Engine
#include "../Core/Motion.h"
#include "Model.h"
#include "Skeleton/BoneGizmable.h"
#include "Skeleton/Joint.h"
#include "Skeleton/Skeleton.h"
#include "Systems/Camera/Camera.h"
#include <Editor/Editor.h>

// Panels
#include "Panels/BoneListPanel.h"
#include "Panels/MenuBarPanel.h"
#include "Panels/PropertyPanel.h"
#include "Panels/SavePanel.h"
#include "Panels/SpeedCurvePanel.h"
#include "Panels/StatusBarPanel.h"
#include "Panels/TimelinePanel.h"
#include "Panels/ToolbarPanel.h"

// External
#ifdef USE_IMGUI
#include "ImGuizmo.h"
#include <imgui.h>
#endif

// Standard
#include <algorithm>

namespace {
constexpr float kPi = 3.14159265f;
} // namespace

MotionEditor::MotionEditor() = default;
MotionEditor::~MotionEditor() = default;

// ============================================================
// 初期化
// ============================================================
void MotionEditor::Initialize(YoRigine::Camera *camera) {
  camera_ = camera;

  lineDrawer_ = std::make_unique<YoRigine::Line>();
  lineDrawer_->SetCamera(camera_);
  lineDrawer_->Initialize();
  lineDrawer_->SetColor({0.5f, 0.5f, 0.5f, 1.0f});

#ifdef USE_IMGUI
  // 3D ビューでボーン位置を示す球
  boneObj_ = YoRigine::Object3d::Create("ICO.obj");
  gizmoCtrl_.Initialize();
  gizmoCtrl_.SetMode(GizmoController::Mode::Local);
#endif

  menuBar_ = std::make_unique<MenuBarPanel>();
  filePanel_ = std::make_unique<SavePanel>();
  toolbar_ = std::make_unique<ToolbarPanel>();
  speedCurve_ = std::make_unique<SpeedCurvePanel>();
  boneList_ = std::make_unique<BoneListPanel>();
  property_ = std::make_unique<PropertyPanel>();
  timeline_ = std::make_unique<TimelinePanel>();
  statusBar_ = std::make_unique<StatusBarPanel>();

  IMotionEditorPanel *panels[] = {
      menuBar_.get(),  filePanel_.get(), toolbar_.get(),  speedCurve_.get(),
      boneList_.get(), property_.get(),  timeline_.get(), statusBar_.get()};
  for (IMotionEditorPanel *panel : panels) {
    panel->Initialize(&session_, &ui_);
  }
}

void MotionEditor::SetCamera(YoRigine::Camera *camera) {
  camera_ = camera;
  if (lineDrawer_) {
    lineDrawer_->SetCamera(camera);
  }
}

// ============================================================
// 更新
// ============================================================
void MotionEditor::Update() {
  if (!isEnabled_) {
    return;
  }
  session_.Update();
}

// ============================================================
// 有効・無効の切り替え
// ============================================================
void MotionEditor::SetEnabled(bool enabled) {
  if (isEnabled_ == enabled) {
    return;
  }
  isEnabled_ = enabled;

  if (!isEnabled_) {
    // 一時編集で崩したポーズを戻してから対象を手放す
    session_.RevertLiveEdit();
    session_.SetTargetObjectId(-1);
    ui_.drawBones = false;
#ifdef USE_IMGUI
    selectedGizmo_.reset();
    pickables_.clear();
#endif
  }
}

// ============================================================
// エディタウィンドウ
// ============================================================
void MotionEditor::ShowEditor() {
#ifdef USE_IMGUI
  if (!isEnabled_) {
    return;
  }

  // タイトルに編集中のモーション名と未保存マークを出す
  std::string title = "Motion Editor";
  if (session_.CurrentMotion()) {
    title += " - " + session_.CurrentDisplayName();
    if (session_.Document()->IsDirty()) {
      title += " *";
    }
  }
  title += "###MotionEditor";

  bool open = true;
  ImGui::SetNextWindowSize(ImVec2(1200, 750), ImGuiCond_FirstUseEver);
  ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_MenuBar);

  HandleBonePicking();
  HandleShortcuts();

  menuBar_->DrawImGui();
  filePanel_->DrawImGui();

  if (!session_.HasTarget()) {
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40);
    const char *message =
        "シーンエディタでアニメーション付きのモデルを選択してください";
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((w - ImGui::CalcTextSize(message).x) * 0.5f);
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", message);
  } else {
    DrawLayout();
  }

  ImGui::End();

  // × で閉じたら無効化する
  if (!open) {
    SetEnabled(false);
  }
#endif
}

void MotionEditor::DrawLayout() {
#ifdef USE_IMGUI
  toolbar_->DrawImGui();
  speedCurve_->DrawImGui();
  ImGui::Separator();

  constexpr float kSplitter = 4.0f;
  constexpr float kStatusBar = 24.0f;
  const ImU32 splitterColor = IM_COL32(100, 130, 200, 180);
  const float contentW = ImGui::GetContentRegionAvail().x;
  const float availH = ImGui::GetContentRegionAvail().y;
  ui_.timelineHeight =
      std::clamp(ui_.timelineHeight, 80.0f, std::max(80.0f, availH - 120.0f));
  const float upperH = availH - ui_.timelineHeight - kSplitter - kStatusBar;

  // ------------------------------------------------------------
  // 上段: ボーンリスト | プロパティ
  // ------------------------------------------------------------
  ImGui::BeginChild("##upper", ImVec2(contentW, upperH), false);
  {
    ImGui::BeginChild("##bones", ImVec2(ui_.boneListWidth, 0), true);
    boneList_->DrawImGui();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::InvisibleButton("##vsplit", ImVec2(kSplitter, upperH));
    if (ImGui::IsItemActive()) {
      ui_.boneListWidth =
          std::clamp(ui_.boneListWidth + ImGui::GetIO().MouseDelta.x, 80.0f,
                     contentW - 100.0f);
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), splitterColor);
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();

    ImGui::BeginChild("##props", ImVec2(0, 0), true);
    property_->DrawImGui();
    ImGui::EndChild();
  }
  ImGui::EndChild();

  // ------------------------------------------------------------
  // 上下の仕切り（タイムラインの高さ調整）
  // ------------------------------------------------------------
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::InvisibleButton("##hsplit", ImVec2(contentW, kSplitter));
  if (ImGui::IsItemActive()) {
    ui_.timelineHeight -= ImGui::GetIO().MouseDelta.y;
  }
  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), splitterColor);
  }
  ImGui::PopStyleVar();

  // ------------------------------------------------------------
  // 下段: タイムライン + ステータスバー
  // ------------------------------------------------------------
  ImGui::BeginChild("##timeline", ImVec2(contentW, ui_.timelineHeight), false);
  timeline_->DrawImGui();
  ImGui::EndChild();

  statusBar_->DrawImGui();
#endif
}

// ============================================================
// ショートカット
// ============================================================
void MotionEditor::HandleShortcuts() {
#ifdef USE_IMGUI
  // エディタウィンドウにフォーカスがあり、文字入力中でないときだけ受け付ける
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }
  const ImGuiIO &io = ImGui::GetIO();
  if (io.WantTextInput) {
    return;
  }
  const bool ctrl = io.KeyCtrl;
  const bool shift = io.KeyShift;
  auto pressed = [](ImGuiKey key, bool repeat = false) {
    return ImGui::IsKeyPressed(key, repeat);
  };

  // 履歴
  if (ctrl && pressed(ImGuiKey_Z)) {
    shift ? session_.Redo() : session_.Undo();
    return;
  }
  if (ctrl && pressed(ImGuiKey_Y)) {
    session_.Redo();
    return;
  }

  // コピー / ペースト
  if (ctrl && pressed(ImGuiKey_C)) {
    session_.CopyPose();
    return;
  }
  if (ctrl && pressed(ImGuiKey_V)) {
    session_.PastePose(shift);
    return;
  }
  if (ctrl) {
    return;
  }

  // 再生・時刻移動
  if (pressed(ImGuiKey_Space)) {
    session_.TogglePlay();
  }
  if (pressed(ImGuiKey_LeftArrow, true)) {
    shift ? session_.JumpToPrevKey() : session_.StepFrames(-1);
  }
  if (pressed(ImGuiKey_RightArrow, true)) {
    shift ? session_.JumpToNextKey() : session_.StepFrames(1);
  }
  if (pressed(ImGuiKey_Home)) {
    session_.Seek(0.0f);
  }
  if (pressed(ImGuiKey_End)) {
    session_.Seek(session_.Playback().Duration());
  }

  // キー編集
  if (pressed(ImGuiKey_I) && session_.CurrentMotion()) {
    if (shift || session_.SelectedBone().empty()) {
      session_.InsertKeyForAllBones();
    } else {
      // 現在見えている姿勢をそのままキーにする
      session_.PullPoseFromJoint();
      session_.InsertKeyForSelectedBone();
    }
  }
  // タイムラインで選んだキーは DopeSheet 側が Delete を処理するので、
  // ここではキー未選択時（選択ボーンの現在時刻のキー）だけを扱う
  if (pressed(ImGuiKey_Delete) && !session_.SelectedKey()) {
    session_.DeleteSelectedKey();
  }

  // 表示・選択
  if (pressed(ImGuiKey_B)) {
    ui_.drawBones = !ui_.drawBones;
  }
  if (pressed(ImGuiKey_Escape)) {
    if (session_.SelectedKey()) {
      session_.ClearKeySelection();
    } else {
      session_.ClearBoneSelection();
    }
  }
#endif
}

// ============================================================
// 3D ビューでのボーン選択
// ============================================================
void MotionEditor::HandleBonePicking() {
#ifdef USE_IMGUI
  Skeleton *skeleton = session_.TargetSkeleton();
  if (!ui_.drawBones || !skeleton || !camera_) {
    return;
  }
  // ゲームビューの絵の上でのクリックだけを受ける
  // (手前のパネル・ギズモの上は除く)。 以前の IsWindowHovered(AnyWindow)
  // はゲームビュー自身もウィンドウなので 常に true
  // になり、ボーンをクリックで選べなかった。
  if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
      !Editor::GetInstance()->CanPickInGameView()) {
    return;
  }

  gizmoCtrl_.ClearPickables();
  pickables_.clear();
  for (const Joint &joint : skeleton->GetJoints()) {
    auto pickable = std::make_unique<BoneGizmable>(this, joint.GetName());
    pickable->UpdateFromJoint();
    gizmoCtrl_.RegisterPickable(pickable.get());
    pickables_.push_back(std::move(pickable));
  }

  const ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
  const ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();
  if (IGizmable *picked = gizmoCtrl_.TryPickObject(
          ImGui::GetMousePos(), viewPos, viewSize, camera_)) {
    session_.SelectBone(static_cast<BoneGizmable *>(picked)->GetBoneName());
  }
#endif
}

// ============================================================
// ボーン位置の球を描画
// ============================================================
void MotionEditor::Draw() {
#ifdef USE_IMGUI
  if (!isEnabled_ || !ui_.drawBones || !camera_ || !boneObj_) {
    return;
  }
  Skeleton *skeleton = session_.TargetSkeleton();
  if (!skeleton) {
    return;
  }

  auto &joints = skeleton->GetJoints();
  if (boneWorldTransforms_.size() != joints.size()) {
    boneWorldTransforms_.resize(joints.size());
    for (auto &wt : boneWorldTransforms_) {
      wt.Initialize();
    }
  }

  const Matrix4x4 targetMat = session_.GetTargetWorldMatrix();
  for (size_t i = 0; i < joints.size(); ++i) {
    const Matrix4x4 worldMat =
        joints[i].GetWorldTransform().matWorld_ * targetMat;
    float t[3], rDeg[3], s[3];
    ImGuizmo::DecomposeMatrixToComponents(&worldMat.m[0][0], t, rDeg, s);

    auto &wt = boneWorldTransforms_[i];
    wt.translate_ = {t[0], t[1], t[2]};
    wt.rotate_ = {rDeg[0] * (kPi / 180.f), rDeg[1] * (kPi / 180.f),
                  rDeg[2] * (kPi / 180.f)};
    wt.scale_ = {0.1f, 0.1f, 0.1f};
    wt.UpdateMatrix();

    // 選択ボーンは黄色で強調
    const bool selected = session_.SelectedBone() == joints[i].GetName();
    boneObj_->SetMaterialColor(selected ? Vector4{1.0f, 0.85f, 0.2f, 1.0f}
                                        : Vector4{0.5f, 0.5f, 0.5f, 1.0f});
    boneObj_->Draw(camera_, wt);
  }
#endif
}

// ============================================================
// 選択ボーンのギズモ
// ============================================================
void MotionEditor::DrawGizmo() {
#ifdef USE_IMGUI
  if (!isEnabled_ || !ui_.drawBones || !camera_ || !session_.TargetSkeleton()) {
    return;
  }
  const std::string &bone = session_.SelectedBone();
  if (bone.empty()) {
    selectedGizmo_.reset();
    return;
  }

  if (!selectedGizmo_ || selectedGizmo_->GetBoneName() != bone) {
    selectedGizmo_ = std::make_unique<BoneGizmable>(this, bone);
  }
  // 操作中でなければ最新のボーン位置に合わせる
  if (!gizmoCtrl_.IsUsing()) {
    selectedGizmo_->UpdateFromJoint();
  }

  std::vector<IGizmable *> targets = {selectedGizmo_.get()};
  const ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
  const ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();
  gizmoCtrl_.Draw(camera_, targets, viewPos, viewSize);
#endif
}

// ============================================================
// ボーンの線を描画
// ============================================================
void MotionEditor::DrawBone() {
  if (!isEnabled_ || !ui_.drawBones || !camera_ || !lineDrawer_) {
    return;
  }
  YoRigine::Object3d *target = session_.TargetObject();
  if (!target) {
    return;
  }
  lineDrawer_->SetCamera(camera_);
  lineDrawer_->Reset();
  target->DrawBone(*lineDrawer_, session_.GetTargetWorldMatrix());
}

// ============================================================
// ギズモ連携
// ============================================================
Matrix4x4 MotionEditor::GetJointWorldMatrix(const std::string &boneName) const {
  return session_.GetJointWorldMatrix(boneName);
}

void MotionEditor::ApplyBoneGizmoTransform(const std::string &boneName,
                                           const Matrix4x4 &newWorldMat) {
#ifdef USE_IMGUI
  // 親のワールド行列の逆行列を掛けてローカル空間に戻し、TRS に分解する
  const Matrix4x4 parentWorld = session_.GetJointParentWorldMatrix(boneName);
  const Matrix4x4 newLocalMat = newWorldMat * Inverse(parentWorld);

  float t[3], rDeg[3], s[3];
  ImGuizmo::DecomposeMatrixToComponents(&newLocalMat.m[0][0], t, rDeg, s);
  session_.SetLiveBoneTransform(boneName, {t[0], t[1], t[2]},
                                {rDeg[0], rDeg[1], rDeg[2]},
                                {s[0], s[1], s[2]});
#else
  (void)boneName;
  (void)newWorldMat;
#endif
}

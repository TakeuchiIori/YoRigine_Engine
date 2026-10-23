#ifdef USE_IMGUI

#include "SceneMenuBar.h"
#include "Localization/Localization.h"

// Engine
#include "../Edit/SceneGizmoLayer.h"
#include "../Edit/SceneLoadController.h"
#include "../Edit/ScenePlacementService.h"
#include "../ObjectSelector.h"
#include <Motion/Editor/MotionEditor.h>

namespace YoRigine {

//=============================================================================
// ファイル
//=============================================================================
void SceneMenuBar::DrawFileMenu(const ScenePanelContext &context) {
  if (!ImGui::BeginMenu(TR("ファイル"))) {
    return;
  }

  SceneLoadController *loader = context.loader;
  const bool hasScene = loader && !loader->GetCurrentScenePath().empty();

  if (!hasScene) {
    ImGui::BeginDisabled();
  }
  if (ImGui::MenuItem(TR("保存"), "Ctrl+S")) {
    loader->Save();
  }
  if (ImGui::MenuItem(TR("読み込み直す"))) {
    loader->Reload();
  }
  if (!hasScene) {
    ImGui::EndDisabled();
  }

  if (hasScene) {
    ImGui::Separator();
    ImGui::TextDisabled("%s", loader->GetCurrentScenePath().c_str());
  }

  ImGui::EndMenu();
}

//=============================================================================
// ウィンドウ
//=============================================================================
void SceneMenuBar::DrawWindowMenu(const ScenePanelContext &context,
                                  const WindowFlags &flags) {
  if (!ImGui::BeginMenu(TR("ウィンドウ"))) {
    return;
  }

  if (flags.outliner) {
    ImGui::MenuItem(TR("アウトライナ"), nullptr, flags.outliner);
  }
  if (flags.inspector) {
    ImGui::MenuItem(TR("インスペクタ"), nullptr, flags.inspector);
  }
  if (flags.modelBrowser) {
    ImGui::MenuItem(TR("モデルブラウザ"), nullptr, flags.modelBrowser);
  }
  ImGui::Separator();
  if (flags.duplicate) {
    ImGui::MenuItem(TR("複製ツール"), nullptr, flags.duplicate);
  }
  if (flags.prefab) {
    ImGui::MenuItem(TR("プレファブ"), nullptr, flags.prefab);
  }
  if (flags.colliderOverview) {
    ImGui::MenuItem(TR("コライダー一覧"), nullptr, flags.colliderOverview);
  }

  // モーションエディタは表示フラグを MotionEditor 自身が持つ。
  // OFF にすると選択への追従とボーン描画ごと止まる。
  if (MotionEditor *motionEditor = context.scene->motionEditor) {
    const bool enabled = motionEditor->IsEnabled();
    if (ImGui::MenuItem(TR("モーションエディタ"), nullptr, enabled)) {
      motionEditor->SetEnabled(!enabled);
    }
  }

  ImGui::EndMenu();
}

//=============================================================================
// 編集
//=============================================================================
void SceneMenuBar::DrawEditMenu(const ScenePanelContext &context) {
  if (!ImGui::BeginMenu(TR("編集"))) {
    return;
  }

  const bool hasSelection =
      context.scene->selector && context.scene->selector->HasSelection();

  if (!hasSelection) {
    ImGui::BeginDisabled();
  }

  if (ImGui::MenuItem(TR("地面に吸着"), "Ctrl+G") && context.placement) {
    context.placement->SnapSelectionToSurface();
  }
  if (ImGui::MenuItem(TR("グリッドに整列")) && context.placement) {
    context.placement->SnapSelectionToGrid(gridSnapSize_);
  }
  ImGui::SetNextItemWidth(120.0f);
  ImGui::DragFloat(TR("  グリッド幅"), &gridSnapSize_, 0.1f, 0.01f, 100.0f, "%.2f");

  if (ImGui::MenuItem(TR("回転を整列")) && context.placement) {
    context.placement->SnapSelectionRotation(rotationSnapDegrees_);
  }
  ImGui::SetNextItemWidth(120.0f);
  ImGui::DragFloat(TR("  回転刻み (度)"), &rotationSnapDegrees_, 0.5f, 1.0f, 90.0f,
                   "%.1f");

  ImGui::Separator();

  // スタンプ配置: 選択オブジェクトをクリック連打で連続配置する
  const bool stamping = context.isStamping && context.isStamping();
  if (stamping) {
    if (ImGui::MenuItem(TR("スタンプ配置を終了"), TR("Esc / 右クリック")) &&
        context.exitStamp) {
      context.exitStamp();
    }
  } else {
    if (ImGui::MenuItem(TR("スタンプ配置を開始"), "B") && context.startStamp) {
      context.startStamp();
    }
  }

  if (!hasSelection) {
    ImGui::EndDisabled();
  }

  // ── ギズモのスナップ設定 ──
  if (context.gizmoLayer) {
    ImGui::Separator();
    GizmoController &gizmo = context.gizmoLayer->GetController();

    bool useSnap = gizmo.IsUsingSnap();
    if (ImGui::MenuItem(TR("ギズモのグリッドスナップ"), nullptr, useSnap)) {
      gizmo.SetUseSnap(!useSnap);
    }

    Vector3 snapValues = gizmo.GetSnapValues();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::DragFloat3(TR("  移動スナップ"), &snapValues.x, 0.1f, 0.1f, 100.0f,
                          "%.2f")) {
      gizmo.SetSnapValues(snapValues);
    }

    float rotationSnap = gizmo.GetRotationSnap();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::DragFloat(TR("  回転スナップ (度)"), &rotationSnap, 0.5f, 0.5f, 90.0f,
                         "%.1f")) {
      gizmo.SetRotationSnap(rotationSnap);
    }
  }

  ImGui::EndMenu();
}

//=============================================================================
// 表示
//=============================================================================
void SceneMenuBar::DrawViewMenu(const ScenePanelContext &context) {
  if (!ImGui::BeginMenu(TR("表示"))) {
    return;
  }

  SceneViewSettings &settings = *context.scene->viewSettings;

  ImGui::MenuItem(TR("選択中を枠で囲む"), nullptr, &settings.showSelectionOutline);

  ImGui::Separator();
  ImGui::MenuItem(TR("コライダーを表示"), nullptr, &settings.showCollider);
  if (settings.showCollider) {
    ImGui::MenuItem(TR("  選択中のみ"), nullptr, &settings.showColliderSelectedOnly);
    ImGui::MenuItem(TR("  BroadPhase グリッド"), nullptr,
                    &settings.showBroadPhaseGrid);
    if (settings.showBroadPhaseGrid) {
      ImGui::SetNextItemWidth(120.0f);
      ImGui::DragFloat(TR("  描画半径"), &settings.broadPhaseGridDrawRadius, 1.0f,
                       5.0f, 500.0f, "%.1f");
    }
  }

  ImGui::Separator();
  ImGui::MenuItem(TR("描画 Frustum カリング"), nullptr,
                  &settings.enableDrawFrustumCulling);
  ImGui::Text(TR("  カリング済み %d / %d"),
              context.scene->objectManager->GetLastFrameCulledCount(),
              context.scene->objectManager->GetLastFrameTotalCount());

  ImGui::EndMenu();
}

//=============================================================================
// メニューバー本体
//=============================================================================
void SceneMenuBar::Draw(const ScenePanelContext &context,
                        const WindowFlags &flags) {
  if (!context.IsValid()) {
    return;
  }
  if (!ImGui::BeginMenu(TR("シーンオブジェクト"))) {
    return;
  }

  DrawFileMenu(context);
  DrawWindowMenu(context, flags);
  DrawEditMenu(context);
  DrawViewMenu(context);

  ImGui::EndMenu();
}

} // namespace YoRigine

#endif // USE_IMGUI

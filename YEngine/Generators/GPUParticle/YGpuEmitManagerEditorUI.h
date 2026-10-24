#pragma once
#ifdef USE_IMGUI

#include "YGpuEmitManager.h"
#include <json.hpp>
#include <string>

namespace YoRigine {

// YGpuEmitManager のエディタ UI（USE_IMGUI 限定。Release には含まれない）。
//
//   ┌ ツールバー: 保存 / Undo / Redo / 再生・停止 / ギズモ / ファイル ──────┐
//   ├ グループ＋エミッターのツリー ┬ スタック（項目一覧） ┬ インスペクタ ────┤
//
// ・タブを行き来せず、ツリーで選んだものをその場で編集する
// ・編集はグループ単位の JSON スナップショットで Undo / Redo できる
//   （GPU バッファは作り直さず、値だけを書き戻す）
// ・Delete はツリーにフォーカスがあるときだけ効き、Undo で戻せる
//
// 編集状態（選択項目・履歴など）は cpp 側に持つ。エミッタ形状のギズモ描画は
// Draw() の描画パイプラインと密結合しているため YGpuEmitManager
// 側に残している。
class YGpuEmitManagerEditorUI {
public:
  // Editor の GameUI パネルから毎フレーム呼ばれる
  static void DrawImGui(YGpuEmitManager &manager);

  // 選択中のグループ / エミッター / フィールドをギズモで動かす。
  // Editor のギズモ描画コールバック（ゲーム画面の上）から毎フレーム呼ばれる
  static void DrawGizmo(YGpuEmitManager& manager);

private:
  using EmitterData = YGpuEmitManager::EmitterData;
  using EmitterGroup = YGpuEmitManager::EmitterGroup;

  //=================================================================
  // 画面の部品
  //=================================================================
  static void DrawToolbar(YGpuEmitManager &manager);
  static void DrawTree(YGpuEmitManager &manager);
  static void DrawGroupInspector(YGpuEmitManager &manager, EmitterGroup &group);
  static void DrawEmitterStack(YGpuEmitManager &manager, EmitterData &emitter);
  static void DrawEmitterInspector(YGpuEmitManager &manager,
                                   EmitterData &emitter);
  static void DrawTextureSection(YGpuEmitManager &manager,
                                 EmitterData &emitter);
  static void DrawStats(EmitterData &emitter);
  static void DrawPopups(YGpuEmitManager &manager);
  static void HandleShortcuts(YGpuEmitManager &manager);

  //=================================================================
  // 構造変更（描画後にまとめて実行し、Undo 履歴に積む）
  //=================================================================
  static void RequestDeleteEmitter(YGpuEmitManager &manager,
                                   const std::string &group,
                                   const std::string &emitter);
  static void RequestDuplicateEmitter(YGpuEmitManager &manager,
                                      const std::string &group,
                                      const std::string &emitter);

  //=================================================================
  // Undo（グループ単位の JSON スナップショット）と保存
  //=================================================================
  // 実行中に変わる値（再生中フラグ・経過時間）は含めない。エミッタは名前をキーにする
  static nlohmann::json MakeGroupSnapshot(YGpuEmitManager &manager,
                                          EmitterGroup &group);
  // エミッタを作り直さずに値を書き戻す（増減したエミッタだけ作成・削除する）
  static void ApplyGroupSnapshot(YGpuEmitManager &manager,
                                 const std::string &groupName,
                                 const nlohmann::json &snapshot);
  static void TrackChanges(YGpuEmitManager &manager);
  static bool SaveGroup(YGpuEmitManager &manager, const std::string &groupName);
  static bool IsGroupDirty(const std::string &groupName);

  //=================================================================
  // 形状別のパラメータ編集（変更があれば true）
  //=================================================================
  static bool DrawShapeEditor(YGpuEmitManager &manager,
                              EmitterData *emitterData);
  static bool DrawSphereEditor(YGpuEmitManager &manager,
                               EmitterData *emitterData);
  static bool DrawBoxEditor(YGpuEmitManager &manager, EmitterData *emitterData);
  static bool DrawTriangleEditor(YGpuEmitManager &manager,
                                 EmitterData *emitterData);
  static bool DrawConeEditor(YGpuEmitManager &manager,
                             EmitterData *emitterData);
  static bool DrawMeshEditor(YGpuEmitManager &manager,
                             EmitterData *emitterData);
  static bool DrawRingEditor(YGpuEmitManager &manager,
                             EmitterData *emitterData);
  static bool DrawLineEditor(YGpuEmitManager &manager,
                             EmitterData *emitterData);
};

} // namespace YoRigine

#endif

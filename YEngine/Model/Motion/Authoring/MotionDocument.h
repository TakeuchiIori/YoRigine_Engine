#pragma once

#include "../Core/Motion.h"
#include <Editor/Command/CommandHistory.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

// ============================================================
// 編集中モーション 1 本分のドキュメント
//
// ・編集対象の Motion（キャッシュ要素）と、その Motion 専用の Undo 履歴を持つ
//   → モーションを切り替えても、別モーションの履歴が混ざらない
// ・変更のたびに Revision が進む。表示側は Revision を見て再構築・再適用する
// ・変更はすべて Commit* 経由で行い、直接 Motion を書き換えない
// ============================================================
class MotionDocument {
public:
  using NodeAnimationMap = std::map<std::string, Motion::NodeAnimation>;

  MotionDocument(std::string key, Motion *motion);

  // キャッシュキー（Model::animationCache_ のキー）
  const std::string &Key() const { return key_; }

  // 編集対象。読み取りは自由だが、変更は Commit* を使うこと
  Motion *Get() const { return motion_; }

  // 変更のたびに増える番号（Undo/Redo でも増える）
  uint64_t Revision() const { return revision_; }

  // 保存後に変更されたか
  bool IsDirty() const { return revision_ != savedRevision_; }
  void MarkSaved() { savedRevision_ = revision_; }

  // ------------------------------------------------------------
  // 変更の確定（すべて Undo 可能）
  // ------------------------------------------------------------

  // 1ボーン分のトラックを before → after に置き換える（nullopt はトラック無し）
  void CommitBone(const std::string &label, const std::string &bone,
                  std::optional<Motion::NodeAnimation> before,
                  std::optional<Motion::NodeAnimation> after);

  // 全ボーンのトラックを before → after に置き換える
  void CommitAnimations(const std::string &label, NodeAnimationMap before,
                        NodeAnimationMap after);

  // 長さ・SpeedCurve を含む Motion 全体を before → after に置き換える
  void CommitMotion(const std::string &label, Motion before, Motion after);

  // ------------------------------------------------------------
  // 履歴
  // ------------------------------------------------------------
  void Undo() { history_.Undo(); }
  void Redo() { history_.Redo(); }
  bool CanUndo() const { return history_.CanUndo(); }
  bool CanRedo() const { return history_.CanRedo(); }
  void ClearHistory() { history_.Clear(); }
  CommandHistory &History() { return history_; }

private:
  void Touch() { ++revision_; }

  std::string key_;
  Motion *motion_ = nullptr;
  CommandHistory history_;
  uint64_t revision_ = 1;
  uint64_t savedRevision_ = 1;
};

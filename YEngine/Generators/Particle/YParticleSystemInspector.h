#pragma once

#ifdef USE_IMGUI
#include "Core/Editor/Command/CommandHistory.h"
#include <array>
#include <functional>
#include <json.hpp>
#include <string>

class YParticleSystem;

/// <summary>
/// 1 つの YParticleSystem
/// を「スタック（モジュール一覧）＋インスペクタ（選んだ項目の詳細）」で編集する
/// UI。
///
/// ・スタックは 1 モジュール 1 行（有効チェック / カテゴリ / 名前 /
/// 要約）。並びは実行順そのもの。
///   ドラッグで並べ替え、右クリックで 複製 / 上下移動 / 既定値に戻す / 削除。
/// ・詳細は右（または下）のインスペクタに選んだ 1
/// 件だけ出す。縦に長く伸びない。 ・値の変更は「操作が終わった時点」の JSON
/// スナップショットで自動的に Undo 履歴へ積む。
///   モジュール側に Undo 対応のコードを書かなくてよい。
///
/// YParticleEditor（横並び）と YEmitterGroupEditor（縦積み）で共用する。
/// </summary>
class YParticleSystemInspector {
public:
  enum class Layout {
    SideBySide, // スタック｜インスペクタ を左右に並べる（広いパネル用）
    Stacked,    // スタックの下にインスペクタ（狭い場所に埋め込む用）
  };

  /// 編集 UI を描く。対象が前回と違えば履歴をリセットして結び直す
  void Draw(YParticleSystem &system, Layout layout);

  CommandHistory &GetHistory() { return history_; }

  /// 最後に保存した状態から変わっているか（一覧の * 表示用）
  bool IsDirty() const {
    return !savedSnapshot_.is_null() && lastSnapshot_ != savedSnapshot_;
  }

  /// 保存した直後に呼ぶ（いまの状態を「保存済み」とする）
  void MarkSaved() { savedSnapshot_ = lastSnapshot_; }

private:
  enum class SelectionKind { Settings, Spawn, Update };

  struct Selection {
    SelectionKind kind = SelectionKind::Settings;
    int index = -1;
  };

  // 描画中にリストを壊さないよう、構造変更（追加・削除・並べ替え）は描画後にまとめて行う
  struct PendingOp {
    std::function<void(YParticleSystem &)> apply;
    std::string label; // Undo 履歴に出す名前
  };

  void Bind(YParticleSystem &system);
  void DrawStack(YParticleSystem &system);
  void DrawModuleSection(YParticleSystem &system, SelectionKind kind);
  void DrawInspector(YParticleSystem &system);
  void DrawStats(const YParticleSystem &system);
  void HandleShortcuts(YParticleSystem &system);
  void TrackChanges(YParticleSystem &system);

  // 選択中モジュールへの操作（描画後に実行される）
  void RequestMove(SelectionKind kind, int from, int to);
  void RequestDuplicate(YParticleSystem &system, SelectionKind kind, int index);
  void RequestReset(YParticleSystem &system, SelectionKind kind, int index);
  void RequestDelete(YParticleSystem &system, SelectionKind kind, int index);

  const YParticleSystem *boundSystem_ = nullptr;
  std::string boundName_;

  nlohmann::json lastSnapshot_;  // 直近に確定した状態
  nlohmann::json savedSnapshot_; // 最後に保存した状態
  CommandHistory history_;

  Selection selection_;
  PendingOp pendingOp_;
  float stackWidth_ = 320.0f;
  Layout layout_ = Layout::SideBySide;

  // 粒子数の推移（ImPlot 表示用のリングバッファ）
  static constexpr int kStatsSamples = 240;
  std::array<float, kStatsSamples> aliveHistory_{};
  int aliveHead_ = 0;
};

#endif // USE_IMGUI

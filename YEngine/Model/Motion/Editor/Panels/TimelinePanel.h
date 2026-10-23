#pragma once
#include "IMotionEditorPanel.h"

#include "Debugger/DopeSheet/DopeSheetEditor.h"

#include <cstdint>
#include <string>
#include <vector>

class Motion;

// ============================================================
// タイムライン（ドープシート）パネル
//
// ・モーションのキーを DopeSheet のトラックに変換して表示する
// ・キーの移動/削除/選択/シークは Session の関数に変換して渡す
// ・DopeKey::value にはキーの元の時刻（秒）を入れておき、
//   動かしていないキーの時刻をフレームに丸めないようにする
// ============================================================
class TimelinePanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;

protected:
  void OnInitialize() override;

private:
  // トラック 1 行が何を表すか
  struct TrackInfo {
    std::string bone;
    int channel = -1; // 0=T 1=R 2=S、-1=グループ見出し、-2=概要
  };

  void RebuildTracks(const Motion &motion);
  // ドラッグで動いたトラックの時刻を確定する
  void CommitMovedTrack(int trackIndex);

  DopeSheet::DopeSheetEditor dopeSheet_;
  std::vector<DopeSheet::DopeTrack> tracks_;
  std::vector<TrackInfo> trackInfos_;

  // 再構築の判定
  const Motion *builtMotion_ = nullptr;
  uint64_t builtRevision_ = 0;
  std::string builtForBone_;
  bool builtSelectedOnly_ = true;

  // ドラッグ終了で動いたトラック（Draw 後に確定する）
  int movedTrack_ = -1;

  // 選択ボーンのトラックだけを表示する
  bool showSelectedOnly_ = true;
};

#include "TimelinePanel.h"

#include "../../Authoring/MotionEditorSession.h"
#include "../../Core/Motion.h"
#include "../MotionEditorUIState.h"

#ifdef USE_IMGUI
#include <IconsFontAwesome5.h>
#include <imgui.h>
#endif

#include <algorithm>
#include <set>

namespace {
constexpr int kFps = MotionEditorSession::kFps;

const DopeSheet::Color kColorT = {1.0f, 0.59f, 0.2f, 1.0f};
const DopeSheet::Color kColorR = {0.35f, 0.78f, 0.35f, 1.0f};
const DopeSheet::Color kColorS = {0.31f, 0.59f, 1.0f, 1.0f};
const DopeSheet::Color kColorSummary = {1.0f, 0.82f, 0.24f, 1.0f};

MotionOps::Channel ToChannel(int channel) {
  return static_cast<MotionOps::Channel>(channel);
}

// 1 チャンネル分のキーをトラックにする（value に元の時刻を入れる）
template <typename Keyframes>
DopeSheet::DopeTrack MakeChannelTrack(const char *label,
                                      const DopeSheet::Color &color,
                                      const Keyframes &keyframes, int subType) {
  DopeSheet::DopeTrack track;
  track.label = label;
  track.color = color;
  track.groupDepth = 1;
  for (const auto &kf : keyframes) {
    track.keys.emplace_back(MotionOps::TimeToFrame(kf.time, kFps), kf.time,
                            subType);
  }
  return track;
}
} // namespace

void TimelinePanel::OnInitialize() {
  // ルーラーのクリック/ドラッグで時刻を移動
  dopeSheet_.SetSeekCallback([this](int frame) {
    session_->Seek(frame / static_cast<float>(kFps));
  });

  // キー選択: そのキーのボーン・チャンネル・時刻を Session に伝える
  dopeSheet_.SetSelectKeyCallback([this](int trackIdx, int keyIdx, bool selected) {
    if (!selected || trackIdx < 0 || trackIdx >= static_cast<int>(trackInfos_.size())) {
      return;
    }
    const TrackInfo &info = trackInfos_[trackIdx];
    if (info.channel < 0) {
      return;
    }
    KeySelection key;
    key.bone = info.bone;
    key.channel = ToChannel(info.channel);
    key.time = tracks_[trackIdx].keys[keyIdx].value;
    session_->SelectKey(key);
  });

  // キー削除: クリックされたトラックのボーン・チャンネルだけを消す
  dopeSheet_.SetDeleteKeyCallback([this](int trackIdx, int keyIdx) {
    if (trackIdx < 0 || trackIdx >= static_cast<int>(trackInfos_.size())) {
      return;
    }
    const TrackInfo &info = trackInfos_[trackIdx];
    if (info.channel < 0 ||
        keyIdx >= static_cast<int>(tracks_[trackIdx].keys.size())) {
      return;
    }
    session_->DeleteKeysAt(info.bone, tracks_[trackIdx].keys[keyIdx].value,
                           MotionOps::ToMask(ToChannel(info.channel)));
    session_->ClearKeySelection();
  });

  // キー移動: ドラッグ終了時に呼ばれる。確定は Draw の後で行う
  dopeSheet_.SetMoveKeyCallback(
      [this](int trackIdx, int /*keyIdx*/, int /*newFrame*/) { movedTrack_ = trackIdx; });

  // 概要トラック: 同じフレームの全キーを一括移動
  dopeSheet_.SetSummaryKeyMovedCallback([this](int fromFrame, int delta) {
    session_->ShiftKeysAtFrame(fromFrame, delta);
  });
}

void TimelinePanel::RebuildTracks(const Motion &motion) {
  tracks_.clear();
  trackInfos_.clear();

  const auto &animations = motion.animation_.nodeAnimations_;
  const std::string &selectedBone = session_->SelectedBone();
  const bool filterBySelection = showSelectedOnly_ && !selectedBone.empty();

  // 概要トラック（全キーの時刻を重複なしで1行に集約）
  std::set<int> allFrames;
  for (const auto &[name, na] : animations) {
    for (const auto &kf : na.translate.keyframes) allFrames.insert(MotionOps::TimeToFrame(kf.time, kFps));
    for (const auto &kf : na.rotate.keyframes) allFrames.insert(MotionOps::TimeToFrame(kf.time, kFps));
    for (const auto &kf : na.scale.keyframes) allFrames.insert(MotionOps::TimeToFrame(kf.time, kFps));
  }
  DopeSheet::DopeTrack summary;
  summary.label = "概要（全ボーン）";
  summary.isSummary = true;
  summary.color = kColorSummary;
  for (int frame : allFrames) {
    summary.keys.emplace_back(frame, 0.0f, -1);
  }
  tracks_.push_back(std::move(summary));
  trackInfos_.push_back({"", -2});

  // ボーンごとに 見出し + T/R/S の 4 行（std::map なので名前順）
  for (const auto &[bone, na] : animations) {
    if (filterBySelection && bone != selectedBone) {
      continue;
    }

    DopeSheet::DopeTrack header;
    header.label = bone;
    header.isGroupHeader = true;
    tracks_.push_back(std::move(header));
    trackInfos_.push_back({bone, -1});

    tracks_.push_back(MakeChannelTrack("  T", kColorT, na.translate.keyframes, 0));
    trackInfos_.push_back({bone, 0});
    tracks_.push_back(MakeChannelTrack("  R", kColorR, na.rotate.keyframes, 1));
    trackInfos_.push_back({bone, 1});
    tracks_.push_back(MakeChannelTrack("  S", kColorS, na.scale.keyframes, 2));
    trackInfos_.push_back({bone, 2});
  }

  // 選択中のキーをハイライトする
  if (const auto &selectedKey = session_->SelectedKey()) {
    for (size_t i = 0; i < tracks_.size(); ++i) {
      const TrackInfo &info = trackInfos_[i];
      if (info.channel < 0 || info.bone != selectedKey->bone ||
          ToChannel(info.channel) != selectedKey->channel) {
        continue;
      }
      for (auto &key : tracks_[i].keys) {
        key.selected = std::abs(key.value - selectedKey->time) < 1e-4f;
      }
    }
  }
}

void TimelinePanel::CommitMovedTrack(int trackIndex) {
  if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks_.size())) {
    return;
  }
  const TrackInfo &info = trackInfos_[trackIndex];
  if (info.channel < 0) {
    return;
  }

  // 動かしたキー（フレームが元の時刻と一致しない）だけ新しいフレームの時刻にし、
  // 動かしていないキーは元の時刻をそのまま使う
  std::vector<float> times;
  std::set<int> usedFrames;
  for (const auto &key : tracks_[trackIndex].keys) {
    const bool moved = key.frame != MotionOps::TimeToFrame(key.value, kFps);
    times.push_back(moved ? key.frame / static_cast<float>(kFps) : key.value);
    if (!usedFrames.insert(key.frame).second) {
      // 同じフレームにキーが重なる移動は受け付けない（再構築で元に戻る）
      session_->SetStatus("同じフレームに既にキーがあります");
      return;
    }
  }
  session_->RetimeChannel(info.bone, ToChannel(info.channel), std::move(times));
}

void TimelinePanel::DrawImGui() {
#ifdef USE_IMGUI
  const Motion *motion = session_->CurrentMotion();
  if (!motion) {
    ImGui::TextDisabled("アニメーションを選択するとタイムラインが表示されます");
    return;
  }
  if (motion->animation_.nodeAnimations_.empty()) {
    ImGui::TextDisabled("アニメーションデータなし");
    return;
  }

  // ------------------------------------------------------------
  // 時刻バー
  // ------------------------------------------------------------
  const float duration = motion->GetDuration();
  const int totalFrames = std::max(1, static_cast<int>(duration * kFps));
  float time = session_->Playback().Time();

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 420.0f);
  if (ImGui::SliderFloat("##scrub", &time, 0.0f, duration, "%.3f s")) {
    session_->Seek(time);
  }
  ImGui::SameLine(0, 6);
  ImGui::Text("%3d / %d f", MotionOps::TimeToFrame(time, kFps), totalFrames);
  ImGui::SameLine(0, 10);
  if (ImGui::SmallButton("|<")) session_->Seek(0.0f);
  ImGui::SameLine(0, 2);
  if (ImGui::SmallButton("<")) session_->StepFrames(-1);
  ImGui::SameLine(0, 2);
  if (ImGui::SmallButton(">")) session_->StepFrames(1);
  ImGui::SameLine(0, 2);
  if (ImGui::SmallButton(">|")) session_->Seek(duration);
  ImGui::SameLine(0, 12);
  ImGui::Checkbox("選択ボーンのみ", &showSelectedOnly_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("ボーン未選択のときは全ボーンを表示します");
  }

  // ------------------------------------------------------------
  // トラックの再構築（データ・選択・表示設定が変わったとき）
  // ドラッグ中（離した瞬間のフレームも含む）に作り直すと、DopeSheet が
  // ドラッグ終了を処理する前にキーが元の位置へ戻ってしまうので避ける
  // ------------------------------------------------------------
  const uint64_t revision = session_->Document()->Revision();
  const bool needsRebuild = motion != builtMotion_ || revision != builtRevision_ ||
                            session_->SelectedBone() != builtForBone_ ||
                            showSelectedOnly_ != builtSelectedOnly_;
  const bool mouseBusy = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                         ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  if (needsRebuild && !mouseBusy) {
    RebuildTracks(*motion);
    builtMotion_ = motion;
    builtRevision_ = revision;
    builtForBone_ = session_->SelectedBone();
    builtSelectedOnly_ = showSelectedOnly_;
  }

  // 再生ヘッドは常に Session の時刻に合わせる
  dopeSheet_.SetSeekFrame(MotionOps::TimeToFrame(time, kFps));

  movedTrack_ = -1;
  dopeSheet_.Draw("MotionTimeline", tracks_, totalFrames, kFps);
  if (movedTrack_ >= 0) {
    CommitMovedTrack(movedTrack_);
    movedTrack_ = -1;
    // 受け付けなかった移動も含め、表示をデータに合わせ直す
    builtMotion_ = nullptr;
  }

#endif
}

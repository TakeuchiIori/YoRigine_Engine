#pragma once

#include "MotionDocument.h"
#include "MotionOps.h"
#include "MotionPlayback.h"

#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Vector3.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace YoRigine {
class Object3d;
class Model;
} // namespace YoRigine
class Skeleton;
class MotionSystem;
class Joint;

// ============================================================
// UI が直接バインドする姿勢の編集バッファ（回転は度数法のオイラー角）
// ============================================================
struct PoseBuffer {
  float translate[3] = {0.0f, 0.0f, 0.0f};
  float rotateDeg[3] = {0.0f, 0.0f, 0.0f};
  float scale[3] = {1.0f, 1.0f, 1.0f};

  QuaternionTransform ToTransform() const;
  void FromTransform(const QuaternionTransform &transform);
};

// ============================================================
// 姿勢のクリップボード
// ============================================================
struct PoseClipboard {
  bool hasData = false;
  QuaternionTransform pose{};
  std::string sourceBone;
  float sourceTime = 0.0f;
};

// ============================================================
// タイムライン上で選択中のキー
// ============================================================
struct KeySelection {
  std::string bone;
  MotionOps::Channel channel = MotionOps::Channel::Translate;
  float time = 0.0f;
};

// ============================================================
// モーションエディタのセッション（編集処理の窓口）
//
// ・UI（パネル）はこのクラスの関数だけを呼ぶ。Model / Skeleton / Motion を
//   パネルから直接書き換えない
// ・ImGui に依存しない。Release でもコンパイルできる
// ・編集は MotionDocument を通して行い、すべて Undo できる
// ============================================================
class MotionEditorSession {
public:
  // タイムライン・フレーム指定で使う基準フレームレート
  static constexpr int kFps = 60;

  // ============================================================
  // 対象オブジェクト
  // ============================================================

  // 編集対象を切り替える。MotionSystem を持たないオブジェクトは -1 扱い
  void SetTargetObjectId(int id);
  int TargetObjectId() const { return targetObjectId_; }

  YoRigine::Object3d *TargetObject() const;
  YoRigine::Model *TargetModel() const;
  Skeleton *TargetSkeleton() const;
  MotionSystem *TargetMotionSystem() const;
  bool HasTarget() const { return TargetModel() != nullptr; }

  // ============================================================
  // 編集中のモーション
  // ============================================================
  MotionDocument *Document() const { return activeDocument_; }
  Motion *CurrentMotion() const;
  const std::string &CurrentKey() const;
  // 表示用の名前（キャッシュキーからアニメ名部分を取り出したもの）
  std::string CurrentDisplayName() const;

  // ============================================================
  // 毎フレーム処理
  //   ・再生時刻の同期
  //   ・一時停止中は「時刻」か「データ」が変わったときだけポーズを再適用する
  // ============================================================
  void Update();

  // ============================================================
  // 再生
  // ============================================================
  MotionPlayback &Playback() { return playback_; }
  const MotionPlayback &Playback() const { return playback_; }
  void TogglePlay();
  void Play();
  void Pause();
  void StopPlayback();
  void Seek(float time);
  void StepFrames(int frames);
  void JumpToPrevKey();
  void JumpToNextKey();

  // ============================================================
  // 選択
  // ============================================================
  const std::string &SelectedBone() const { return selectedBone_; }
  void SelectBone(const std::string &bone);
  void ClearBoneSelection();

  const std::optional<KeySelection> &SelectedKey() const {
    return selectedKey_;
  }
  void SelectKey(const KeySelection &key);
  void ClearKeySelection() { selectedKey_.reset(); }

  // ============================================================
  // ポーズ編集（キーを打つまでは一時的な上書き）
  // ============================================================
  PoseBuffer &Pose() { return pose_; }
  const PoseBuffer &Pose() const { return pose_; }
  // Mixamo アセットは cm 単位なので、表示を 1/100 にする倍率
  float TranslateDisplayScale() const { return translateDisplayScale_; }

  // 選択ボーンの現在姿勢をバッファへ読み込む
  void PullPoseFromJoint();
  // バッファの内容を選択ボーンへ一時的に適用する
  void PushPoseToJoint();
  // ギズモなどから、任意ボーンへローカル姿勢を一時適用する（回転は度数法）
  void SetLiveBoneTransform(const std::string &bone, const Vector3 &translate,
                            const Vector3 &rotateDeg, const Vector3 &scale);
  // 一時適用を元に戻す
  bool CanRevertLiveEdit() const;
  void RevertLiveEdit();

  // ボーンのワールド行列（ターゲットのワールド行列込み）
  Matrix4x4 GetJointWorldMatrix(const std::string &bone) const;
  // ボーンの親のワールド行列（ターゲットのワールド行列込み）
  Matrix4x4 GetJointParentWorldMatrix(const std::string &bone) const;
  // ターゲットオブジェクトのワールド行列
  Matrix4x4 GetTargetWorldMatrix() const;

  // ============================================================
  // キー編集
  // ============================================================
  // 選択ボーンに、バッファの姿勢でキーを打つ
  void InsertKeyForSelectedBone();
  // 全ボーンの現在姿勢でキーを打つ
  void InsertKeyForAllBones();
  // 選択キー（無ければ選択ボーンの現在時刻のキー）を削除する
  void DeleteSelectedKey();
  // 指定ボーン・時刻・チャンネルのキーを削除する
  void DeleteKeysAt(const std::string &bone, float time,
                    unsigned mask = MotionOps::kChannelAll);

  // タイムラインでのキー移動を確定する
  void RetimeChannel(const std::string &bone, MotionOps::Channel channel,
                     std::vector<float> newTimes);
  // 概要トラックでの一括移動を確定する
  void ShiftKeysAtFrame(int fromFrame, int delta);

  // ============================================================
  // コピー＆ペースト
  // ============================================================
  void CopyPose();
  // クリップボードの姿勢を選択ボーンへ適用する（insertKey ならキーも打つ）
  void PastePose(bool insertKey);
  const PoseClipboard &Clipboard() const { return clipboard_; }

  // ============================================================
  // モーション加工（新しいモーションを作って編集対象を切り替える）
  // ============================================================
  void MirrorMotion();
  void TrimMotion(float startTime, float endTime);
  void MakePingPongMotion();
  // 選択ボーンの Translate に区間移動を焼き込む（offset は表示単位）
  void BakeTranslateOffset(int startFrame, int endFrame,
                           const Vector3 &offsetDisplay);

  // ============================================================
  // タイムスケールカーブ（points は (正規化時間, 速度倍率)）
  // ============================================================
  void ApplySpeedCurve(const std::vector<std::pair<float, float>> &points);
  void ResetSpeedCurve();
  void BakeSpeedCurve(const std::vector<std::pair<float, float>> &points);

  // ============================================================
  // ファイル
  // ============================================================
  bool LoadAnimFile(const std::string &path);
  bool SaveAnimFile(const std::string &path, bool timeReversed);
  // glTF から読み込み、対象オブジェクトのモーションとして適用する
  bool LoadSourceAnimation(const std::string &objectPath,
                           const std::string &animationName);
  // glTF の全アニメーションをキャッシュへ読み込み、restoreName を適用する
  bool LoadAllSourceAnimations(const std::string &objectPath,
                               const std::vector<std::string> &names,
                               const std::string &restoreName);

  // ============================================================
  // 履歴（編集中モーション単位）
  // ============================================================
  void Undo();
  void Redo();
  bool CanUndo() const;
  bool CanRedo() const;
  void ClearHistory();

  // ============================================================
  // ステータス表示
  // ============================================================
  const std::string &Status() const { return status_; }
  void SetStatus(std::string message) { status_ = std::move(message); }

private:
  // ライブ編集（キーを打つ前の一時上書き）の記録
  struct LiveEdit {
    std::string bone;
    QuaternionTransform original{}; // 編集前の姿勢（元に戻す用）
  };

  // key の Motion を編集対象として開く。freshContent なら履歴を作り直す
  void OpenMotion(const std::string &key, Motion *motion, bool freshContent,
                  bool restartFromHead);
  // 生成したモーションをキャッシュへ登録して開く
  void AdoptDerivedMotion(const std::string &key, Motion &&motion,
                          bool restartFromHead);
  // ターゲットが現在再生しているモーションを開く
  void OpenTargetMotion();

  Joint *FindJoint(const std::string &bone) const;
  // スケルトンの行列とスキニング用パレットを更新する
  void RefreshSkeleton() const;
  // 全キー時刻（昇順・重複なし）
  std::vector<float> CollectKeyTimes() const;

  int targetObjectId_ = -1;

  std::unordered_map<std::string, std::unique_ptr<MotionDocument>> documents_;
  MotionDocument *activeDocument_ = nullptr;

  MotionPlayback playback_;

  std::string selectedBone_;
  std::optional<KeySelection> selectedKey_;

  PoseBuffer pose_;
  float translateDisplayScale_ = 1.0f;
  std::optional<LiveEdit> liveEdit_;

  PoseClipboard clipboard_;

  // 一時停止中のポーズ再適用の判定用
  float lastAppliedTime_ = -1.0f;
  uint64_t lastAppliedRevision_ = 0;
  const Motion *lastAppliedMotion_ = nullptr;

  std::string status_ = "Ready";
};

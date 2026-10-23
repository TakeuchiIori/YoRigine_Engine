#include "MotionEditorSession.h"

#include "MotionFiles.h"

#include "../Core/MotionSystem.h"
#include "MathFunc.h"
#include "Model.h"
#include "Object3D/Object3d.h"
#include "Object3D/ObjectManager.h"
#include "Skeleton/Joint.h"
#include "Skeleton/Skeleton.h"
#include "Skeleton/SkinCluster.h"

#include <algorithm>
#include <cmath>
#include <exception>

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

// ランタイムが再生中のモーションがキャッシュに無いときのキー
const std::string kRuntimeMotionKey = "<runtime>";
const std::string kEmptyKey;

} // namespace

// ============================================================
// PoseBuffer
// ============================================================
QuaternionTransform PoseBuffer::ToTransform() const {
  QuaternionTransform tr;
  tr.translate = {translate[0], translate[1], translate[2]};
  tr.rotate = EulerToQuaternion({rotateDeg[0] * kDegToRad,
                                 rotateDeg[1] * kDegToRad,
                                 rotateDeg[2] * kDegToRad});
  tr.scale = {scale[0], scale[1], scale[2]};
  return tr;
}

void PoseBuffer::FromTransform(const QuaternionTransform &transform) {
  translate[0] = transform.translate.x;
  translate[1] = transform.translate.y;
  translate[2] = transform.translate.z;
  const Vector3 euler = QuaternionToEuler(transform.rotate);
  rotateDeg[0] = euler.x * kRadToDeg;
  rotateDeg[1] = euler.y * kRadToDeg;
  rotateDeg[2] = euler.z * kRadToDeg;
  scale[0] = transform.scale.x;
  scale[1] = transform.scale.y;
  scale[2] = transform.scale.z;
}

// ============================================================
// 対象オブジェクト
// ============================================================
YoRigine::Object3d *MotionEditorSession::TargetObject() const {
  if (targetObjectId_ == -1) {
    return nullptr;
  }
  return ObjectManager::GetInstance()->GetObject3dById(targetObjectId_);
}

YoRigine::Model *MotionEditorSession::TargetModel() const {
  YoRigine::Object3d *object = TargetObject();
  return object ? object->GetModel() : nullptr;
}

Skeleton *MotionEditorSession::TargetSkeleton() const {
  YoRigine::Model *model = TargetModel();
  return model ? model->GetSkeleton() : nullptr;
}

MotionSystem *MotionEditorSession::TargetMotionSystem() const {
  YoRigine::Model *model = TargetModel();
  return model ? model->GetMotionSystem() : nullptr;
}

void MotionEditorSession::SetTargetObjectId(int id) {
  // MotionSystem を持たないオブジェクトは編集対象にしない
  if (id != -1) {
    YoRigine::Object3d *object =
        ObjectManager::GetInstance()->GetObject3dById(id);
    if (!object || !object->GetModel() ||
        !object->GetModel()->GetMotionSystem()) {
      id = -1;
    }
  }

  if (targetObjectId_ == id) {
    return;
  }

  // 前の対象に残っている一時編集を戻してから切り替える
  RevertLiveEdit();

  targetObjectId_ = id;
  selectedBone_.clear();
  selectedKey_.reset();
  activeDocument_ = nullptr;
  lastAppliedMotion_ = nullptr;
  lastAppliedTime_ = -1.0f;

  MotionSystem *ms = TargetMotionSystem();
  playback_.Bind(ms);
  if (!ms) {
    status_ = "Ready";
    return;
  }

  OpenTargetMotion();
  playback_.ResetForNewTarget(ms->GetAnimationTime());
  status_ = "対象オブジェクトを同期: " + TargetModel()->GetName();
}

void MotionEditorSession::OpenTargetMotion() {
  YoRigine::Model *model = TargetModel();
  MotionSystem *ms = TargetMotionSystem();
  if (!model || !ms) {
    return;
  }

  // ブレンド後の MotionSystem は内部コピーを指しているため、
  // 編集はキャッシュ本体に対して行い、MotionSystem もそちらを再生させる
  const std::string &baseKey = model->GetBaseMotionCacheKey();
  auto &cache = YoRigine::Model::animationCache_;
  if (auto it = cache.find(baseKey); it != cache.end()) {
    OpenMotion(baseKey, &it->second, false, false);
    return;
  }

  if (Motion *runtime = ms->GetAnimation()) {
    OpenMotion(kRuntimeMotionKey, runtime, false, false);
  }
}

// ============================================================
// 編集中のモーション
// ============================================================
Motion *MotionEditorSession::CurrentMotion() const {
  return activeDocument_ ? activeDocument_->Get() : nullptr;
}

const std::string &MotionEditorSession::CurrentKey() const {
  return activeDocument_ ? activeDocument_->Key() : kEmptyKey;
}

std::string MotionEditorSession::CurrentDisplayName() const {
  return MotionFiles::DisplayNameFromKey(CurrentKey());
}

void MotionEditorSession::OpenMotion(const std::string &key, Motion *motion,
                                     bool freshContent, bool restartFromHead) {
  if (!motion) {
    return;
  }

  // 同じキーでも中身が入れ替わった（再読み込み・再生成）なら履歴は無効なので作り直す
  auto &document = documents_[key];
  if (!document || document->Get() != motion || freshContent) {
    document = std::make_unique<MotionDocument>(key, motion);
  }
  activeDocument_ = document.get();
  selectedKey_.reset();

  if (MotionSystem *ms = TargetMotionSystem()) {
    if (ms->GetAnimation() != motion) {
      ms->SetAnimation(motion);
    }
    if (restartFromHead) {
      ms->SetAnimationTime(0.0f);
    }
  }
  // 次の Update でポーズを必ず再適用させる
  lastAppliedMotion_ = nullptr;
}

void MotionEditorSession::AdoptDerivedMotion(const std::string &key,
                                             Motion &&motion,
                                             bool restartFromHead) {
  auto &cache = YoRigine::Model::animationCache_;
  cache[key] = std::move(motion);
  OpenMotion(key, &cache[key], true, restartFromHead);
}

// ============================================================
// 毎フレーム処理
// ============================================================
void MotionEditorSession::Update() {
  MotionSystem *ms = TargetMotionSystem();
  if (!ms) {
    return;
  }

  playback_.Bind(ms);
  playback_.Sync();

  Skeleton *skeleton = TargetSkeleton();
  if (!skeleton) {
    return;
  }

  Motion *motion = CurrentMotion();
  if (!playback_.IsPlaying() && motion) {
    // 一時停止中は「時刻」「データ」「対象モーション」のどれかが変わったときだけ
    // ポーズを作り直す。毎フレーム作り直すと手動のライブ編集が消えてしまう
    const float time = playback_.Time();
    const uint64_t revision = activeDocument_->Revision();
    const bool changed = std::abs(time - lastAppliedTime_) > 1e-5f ||
                         revision != lastAppliedRevision_ ||
                         motion != lastAppliedMotion_;
    if (changed) {
      motion->ApplyAnimation(skeleton->GetJoints(), time);
      lastAppliedTime_ = time;
      lastAppliedRevision_ = revision;
      lastAppliedMotion_ = motion;

      // ポーズを作り直したので一時編集は消えている
      liveEdit_.reset();
      if (!selectedBone_.empty()) {
        PullPoseFromJoint();
      }
    }
  } else if (playback_.IsPlaying() && liveEdit_) {
    // 再生中もライブ編集中のボーンはバッファの姿勢で上書きして見せる
    if (Joint *joint = FindJoint(liveEdit_->bone)) {
      joint->SetTransform(pose_.ToTransform());
    }
  }

  RefreshSkeleton();
}

// ============================================================
// 再生
// ============================================================
void MotionEditorSession::TogglePlay() {
  if (playback_.IsPlaying()) {
    Pause();
  } else {
    Play();
  }
}

void MotionEditorSession::Play() {
  playback_.Play(CurrentMotion());
  status_ = "再生";
}

void MotionEditorSession::Pause() {
  playback_.Pause();
  // 一時停止した瞬間の時刻でポーズを作り直す
  lastAppliedTime_ = -1.0f;
  status_ = "一時停止";
}

void MotionEditorSession::StopPlayback() {
  playback_.Stop();
  status_ = "停止 (先頭に戻る)";
}

void MotionEditorSession::Seek(float time) { playback_.Seek(time); }

void MotionEditorSession::StepFrames(int frames) {
  playback_.Step(frames, kFps);
}

std::vector<float> MotionEditorSession::CollectKeyTimes() const {
  std::vector<float> times;
  const Motion *motion = CurrentMotion();
  if (!motion) {
    return times;
  }

  auto collect = [&times](const Motion::NodeAnimation &na) {
    for (const auto &kf : na.translate.keyframes) times.push_back(kf.time);
    for (const auto &kf : na.rotate.keyframes) times.push_back(kf.time);
    for (const auto &kf : na.scale.keyframes) times.push_back(kf.time);
  };

  // ボーン選択中はそのボーンのキーだけを対象にする
  const auto &animations = motion->animation_.nodeAnimations_;
  if (!selectedBone_.empty()) {
    if (auto it = animations.find(selectedBone_); it != animations.end()) {
      collect(it->second);
    }
  } else {
    for (const auto &[name, na] : animations) {
      collect(na);
    }
  }

  std::sort(times.begin(), times.end());
  times.erase(std::unique(times.begin(), times.end(),
                          [](float a, float b) { return std::abs(a - b) < 1e-4f; }),
              times.end());
  return times;
}

void MotionEditorSession::JumpToPrevKey() {
  const std::vector<float> times = CollectKeyTimes();
  const float now = playback_.Time();
  for (auto it = times.rbegin(); it != times.rend(); ++it) {
    if (*it < now - 1e-4f) {
      Seek(*it);
      return;
    }
  }
}

void MotionEditorSession::JumpToNextKey() {
  const std::vector<float> times = CollectKeyTimes();
  const float now = playback_.Time();
  for (float t : times) {
    if (t > now + 1e-4f) {
      Seek(t);
      return;
    }
  }
}

// ============================================================
// 選択
// ============================================================
void MotionEditorSession::SelectBone(const std::string &bone) {
  if (selectedBone_ != bone) {
    // 別ボーンへ移ったら、前のボーンの一時編集の追跡はやめる（姿勢は残る）
    liveEdit_.reset();
  }
  selectedBone_ = bone;
  PullPoseFromJoint();
  status_ = "ボーン選択: " + bone;
}

void MotionEditorSession::ClearBoneSelection() {
  selectedBone_.clear();
  liveEdit_.reset();
  status_ = "ボーン選択を解除しました";
}

void MotionEditorSession::SelectKey(const KeySelection &key) {
  selectedKey_ = key;
  if (selectedBone_ != key.bone) {
    SelectBone(key.bone);
  }
}

// ============================================================
// ポーズ編集
// ============================================================
Joint *MotionEditorSession::FindJoint(const std::string &bone) const {
  Skeleton *skeleton = TargetSkeleton();
  return skeleton ? skeleton->GetJointByName(bone) : nullptr;
}

void MotionEditorSession::RefreshSkeleton() const {
  YoRigine::Model *model = TargetModel();
  if (!model || !model->GetSkeleton()) {
    return;
  }
  model->GetSkeleton()->Update();
  if (model->GetSkinCluster()) {
    model->GetSkinCluster()->UpdateMatrixPalette(
        model->GetSkeleton()->GetJoints());
  }
}

void MotionEditorSession::PullPoseFromJoint() {
  Joint *joint = FindJoint(selectedBone_);
  if (!joint) {
    return;
  }
  YoRigine::Model *model = TargetModel();
  translateDisplayScale_ = (model && model->IsMixamoAsset()) ? 100.0f : 1.0f;
  pose_.FromTransform(joint->GetTransform());
}

void MotionEditorSession::PushPoseToJoint() {
  Joint *joint = FindJoint(selectedBone_);
  if (!joint) {
    return;
  }
  // 最初の一時編集のときだけ、元の姿勢を覚えておく
  if (!liveEdit_ || liveEdit_->bone != selectedBone_) {
    liveEdit_ = LiveEdit{selectedBone_, joint->GetTransform()};
  }
  joint->SetTransform(pose_.ToTransform());
  RefreshSkeleton();
}

void MotionEditorSession::SetLiveBoneTransform(const std::string &bone,
                                               const Vector3 &translate,
                                               const Vector3 &rotateDeg,
                                               const Vector3 &scale) {
  Joint *joint = FindJoint(bone);
  if (!joint) {
    return;
  }

  PoseBuffer buffer;
  buffer.translate[0] = translate.x;
  buffer.translate[1] = translate.y;
  buffer.translate[2] = translate.z;
  buffer.rotateDeg[0] = rotateDeg.x;
  buffer.rotateDeg[1] = rotateDeg.y;
  buffer.rotateDeg[2] = rotateDeg.z;
  buffer.scale[0] = scale.x;
  buffer.scale[1] = scale.y;
  buffer.scale[2] = scale.z;

  if (bone == selectedBone_) {
    pose_ = buffer;
  }
  if (!liveEdit_ || liveEdit_->bone != bone) {
    liveEdit_ = LiveEdit{bone, joint->GetTransform()};
  }
  joint->SetTransform(buffer.ToTransform());
  RefreshSkeleton();
}

bool MotionEditorSession::CanRevertLiveEdit() const {
  return liveEdit_.has_value() && liveEdit_->bone == selectedBone_;
}

void MotionEditorSession::RevertLiveEdit() {
  if (!liveEdit_) {
    return;
  }
  const LiveEdit edit = *liveEdit_;
  liveEdit_.reset();

  Joint *joint = FindJoint(edit.bone);
  if (!joint) {
    return;
  }
  joint->SetTransform(edit.original);
  if (selectedBone_ == edit.bone) {
    PullPoseFromJoint();
  }
  RefreshSkeleton();
  status_ = "一時編集を元に戻しました: " + edit.bone;
}

Matrix4x4 MotionEditorSession::GetTargetWorldMatrix() const {
  if (targetObjectId_ != -1) {
    const auto *placed =
        ObjectManager::GetInstance()->GetObjectById(targetObjectId_);
    if (placed && placed->worldTransform) {
      return placed->worldTransform->matWorld_;
    }
  }
  return MakeIdentity4x4();
}

Matrix4x4 MotionEditorSession::GetJointWorldMatrix(
    const std::string &bone) const {
  Joint *joint = FindJoint(bone);
  if (!joint) {
    return MakeIdentity4x4();
  }
  return joint->GetWorldTransform().matWorld_ * GetTargetWorldMatrix();
}

Matrix4x4 MotionEditorSession::GetJointParentWorldMatrix(
    const std::string &bone) const {
  Joint *joint = FindJoint(bone);
  if (joint && joint->GetWorldTransform().parent_) {
    return joint->GetWorldTransform().parent_->matWorld_ *
           GetTargetWorldMatrix();
  }
  return GetTargetWorldMatrix();
}

// ============================================================
// キー編集
// ============================================================
void MotionEditorSession::InsertKeyForSelectedBone() {
  if (!activeDocument_ || selectedBone_.empty()) {
    return;
  }
  const std::string bone = selectedBone_;
  const float time = playback_.Time();
  const auto &animations = CurrentMotion()->animation_.nodeAnimations_;

  // 対象ボーン 1 本分だけを作業用マップに写して編集する
  MotionOps::NodeAnimationMap work;
  std::optional<Motion::NodeAnimation> before;
  if (auto it = animations.find(bone); it != animations.end()) {
    before = it->second;
    work[bone] = it->second;
  }
  MotionOps::InsertPoseKey(work, bone, time, pose_.ToTransform());

  activeDocument_->CommitBone("KF挿入: " + bone, bone, before, work[bone]);
  liveEdit_.reset();
  status_ = "KF挿入: " + bone + " @ " + std::to_string(time) + "s";
}

void MotionEditorSession::InsertKeyForAllBones() {
  Skeleton *skeleton = TargetSkeleton();
  if (!activeDocument_ || !skeleton) {
    return;
  }
  const float time = playback_.Time();
  const auto before = CurrentMotion()->animation_.nodeAnimations_;
  auto after = before;
  for (const Joint &joint : skeleton->GetJoints()) {
    MotionOps::InsertPoseKey(after, joint.GetName(), time,
                             joint.GetTransform());
  }
  activeDocument_->CommitAnimations(
      "全ボーンKF @ " + std::to_string(time) + "s", before, std::move(after));
  liveEdit_.reset();
  status_ = "全ボーンのポーズをキーにしました @ " + std::to_string(time) + "s";
}

void MotionEditorSession::DeleteSelectedKey() {
  if (selectedKey_) {
    const KeySelection key = *selectedKey_;
    DeleteKeysAt(key.bone, key.time, MotionOps::ToMask(key.channel));
    selectedKey_.reset();
    return;
  }
  if (!selectedBone_.empty()) {
    DeleteKeysAt(selectedBone_, playback_.Time(), MotionOps::kChannelAll);
  }
}

void MotionEditorSession::DeleteKeysAt(const std::string &bone, float time,
                                       unsigned mask) {
  if (!activeDocument_) {
    return;
  }
  const auto &animations = CurrentMotion()->animation_.nodeAnimations_;
  auto it = animations.find(bone);
  if (it == animations.end()) {
    status_ = "削除するキーがありません: " + bone;
    return;
  }

  MotionOps::NodeAnimationMap work{{bone, it->second}};
  switch (MotionOps::RemoveKeysAt(work, bone, time, mask)) {
  case MotionOps::RemoveResult::Removed:
    activeDocument_->CommitBone("KF削除: " + bone, bone, it->second,
                                work[bone]);
    status_ = "KF削除: " + bone + " @ " + std::to_string(time) + "s";
    break;
  case MotionOps::RemoveResult::NotFound:
    status_ = "この時刻にキーがありません: " + bone;
    break;
  case MotionOps::RemoveResult::WouldEmptyChannel:
    status_ = "最後のキーは削除できません（トラックが空になるため）: " + bone;
    break;
  }
}

void MotionEditorSession::RetimeChannel(const std::string &bone,
                                        MotionOps::Channel channel,
                                        std::vector<float> newTimes) {
  if (!activeDocument_) {
    return;
  }
  const auto &animations = CurrentMotion()->animation_.nodeAnimations_;
  auto it = animations.find(bone);
  if (it == animations.end()) {
    return;
  }
  MotionOps::NodeAnimationMap work{{bone, it->second}};
  if (!MotionOps::RetimeChannel(work, bone, channel, std::move(newTimes))) {
    return;
  }
  activeDocument_->CommitBone("KF移動: " + bone, bone, it->second, work[bone]);
  status_ = "KF移動: " + bone;
}

void MotionEditorSession::ShiftKeysAtFrame(int fromFrame, int delta) {
  if (!activeDocument_ || delta == 0) {
    return;
  }
  const auto before = CurrentMotion()->animation_.nodeAnimations_;
  auto after = before;
  MotionOps::ShiftKeysAtFrame(after, fromFrame, delta, kFps);
  activeDocument_->CommitAnimations("概要KF移動", before, std::move(after));
  status_ = "概要: frame " + std::to_string(fromFrame) +
            (delta > 0 ? " +" : " ") + std::to_string(delta) + " 移動";
}

// ============================================================
// コピー＆ペースト
// ============================================================
void MotionEditorSession::CopyPose() {
  if (selectedBone_.empty()) {
    return;
  }
  clipboard_.hasData = true;
  clipboard_.pose = pose_.ToTransform();
  clipboard_.sourceBone = selectedBone_;
  clipboard_.sourceTime = playback_.Time();
  status_ = "コピー: " + selectedBone_ + " @ " +
            std::to_string(clipboard_.sourceTime) + "s";
}

void MotionEditorSession::PastePose(bool insertKey) {
  if (selectedBone_.empty() || !clipboard_.hasData) {
    return;
  }
  pose_.FromTransform(clipboard_.pose);
  PushPoseToJoint();
  if (insertKey) {
    InsertKeyForSelectedBone();
    status_ = "ペースト + KF挿入: " + clipboard_.sourceBone + " -> " +
              selectedBone_;
  } else {
    status_ = "ペースト: " + clipboard_.sourceBone + " -> " + selectedBone_;
  }
}

// ============================================================
// モーション加工
// ============================================================
void MotionEditorSession::MirrorMotion() {
  Motion *motion = CurrentMotion();
  if (!motion) {
    return;
  }
  AdoptDerivedMotion("Mirror:" + CurrentKey(), MotionOps::CreateMirrored(*motion),
                     false);
  status_ = "ミラーモーション生成完了";
}

void MotionEditorSession::TrimMotion(float startTime, float endTime) {
  Motion *motion = CurrentMotion();
  if (!motion) {
    return;
  }
  std::optional<Motion> trimmed =
      MotionOps::CreateTrimmed(*motion, startTime, endTime);
  if (!trimmed) {
    status_ = "トリム失敗: 終了が開始より前です";
    return;
  }
  const float duration = trimmed->GetDuration();
  const std::string key = "Trim:" + CurrentKey() + "[" +
                          std::to_string(startTime) + "-" +
                          std::to_string(endTime) + "]";
  AdoptDerivedMotion(key, std::move(*trimmed), true);
  status_ = "トリムモーション生成: " + std::to_string(duration) + "s";
}

void MotionEditorSession::MakePingPongMotion() {
  Motion *motion = CurrentMotion();
  if (!motion) {
    return;
  }
  Motion pingpong = MotionOps::CreatePingPong(*motion);
  const float duration = pingpong.GetDuration();
  AdoptDerivedMotion("PingPong:" + CurrentKey(), std::move(pingpong), true);
  status_ = "ピンポンモーション生成完了 (" + std::to_string(duration) + "s)";
}

void MotionEditorSession::BakeTranslateOffset(int startFrame, int endFrame,
                                              const Vector3 &offsetDisplay) {
  Motion *motion = CurrentMotion();
  if (!activeDocument_ || !motion || selectedBone_.empty()) {
    return;
  }

  MotionOps::TranslateOffsetBakeParams params;
  params.boneName = selectedBone_;
  params.fallback.translate = {0.0f, 0.0f, 0.0f};
  params.fallback.rotate = {0.0f, 0.0f, 0.0f, 1.0f};
  params.fallback.scale = {1.0f, 1.0f, 1.0f};
  if (Joint *joint = FindJoint(selectedBone_)) {
    params.fallback = joint->GetTransform();
  }
  params.startFrame = startFrame;
  params.endFrame = endFrame;
  params.fps = kFps;
  // UI 入力値（表示単位）をモーションデータの単位へ換算する
  const float unitScale = std::max(0.0001f, translateDisplayScale_);
  params.offset = {offsetDisplay.x * unitScale, offsetDisplay.y * unitScale,
                   offsetDisplay.z * unitScale};

  auto result = MotionOps::BakeTranslateOffsetRange(*motion, params);
  if (!result) {
    status_ = "区間移動なし: 移動量が0です";
    return;
  }

  activeDocument_->CommitAnimations("区間移動焼き込み: " + selectedBone_,
                                    motion->animation_.nodeAnimations_,
                                    std::move(result->animations));
  status_ = "区間移動焼き込み: " + selectedBone_ + " [" +
            std::to_string(result->startFrame) + "f-" +
            std::to_string(result->endFrame) + "f]";
}

// ============================================================
// タイムスケールカーブ
// ============================================================
namespace {
// (正規化時間, 倍率) の列を、時間順に並んだ SpeedCurve のキーにする
void AssignSpeedCurve(Motion &motion,
                      const std::vector<std::pair<float, float>> &points) {
  auto &keys = motion.GetSpeedCurve().curve.keyframes;
  keys.clear();
  for (const auto &[t, speed] : points) {
    keys.push_back({t, speed});
  }
  std::sort(keys.begin(), keys.end(),
            [](const auto &a, const auto &b) { return a.time < b.time; });
}
} // namespace

void MotionEditorSession::ApplySpeedCurve(
    const std::vector<std::pair<float, float>> &points) {
  Motion *motion = CurrentMotion();
  if (!activeDocument_ || !motion) {
    return;
  }
  Motion after = *motion;
  AssignSpeedCurve(after, points);
  activeDocument_->CommitMotion("スピードカーブ適用", *motion, std::move(after));
  status_ = "スピードカーブを適用しました";
}

void MotionEditorSession::ResetSpeedCurve() {
  Motion *motion = CurrentMotion();
  if (!activeDocument_ || !motion) {
    return;
  }
  Motion after = *motion;
  after.ClearSpeedCurve();
  activeDocument_->CommitMotion("スピードカーブリセット", *motion,
                                std::move(after));
  status_ = "スピードカーブをリセットしました";
}

void MotionEditorSession::BakeSpeedCurve(
    const std::vector<std::pair<float, float>> &points) {
  Motion *motion = CurrentMotion();
  if (!activeDocument_ || !motion) {
    return;
  }
  Motion withCurve = *motion;
  AssignSpeedCurve(withCurve, points);
  activeDocument_->CommitMotion("スピードカーブ焼き込み", *motion,
                                MotionOps::BakeSpeedCurve(withCurve));
  status_ = "スピードカーブを焼き込みました（Ctrl+Z で戻せます）";
}

// ============================================================
// ファイル
// ============================================================
bool MotionEditorSession::LoadAnimFile(const std::string &path) {
  try {
    Motion loaded = MotionFiles::LoadAnim(path);
    const std::string key = "Binary:" + path;
    auto &cache = YoRigine::Model::animationCache_;
    cache[key] = std::move(loaded);
    OpenMotion(key, &cache[key], true, true);

    if (MotionSystem *ms = TargetMotionSystem()) {
      ms->SetPlayMode(MotionPlayMode::Loop);
    }
    status_ = "読み込み成功: " + path;
    return true;
  } catch (const std::exception &e) {
    status_ = std::string("読み込み失敗: ") + e.what();
    return false;
  }
}

bool MotionEditorSession::SaveAnimFile(const std::string &path,
                                       bool timeReversed) {
  Motion *motion = CurrentMotion();
  if (!motion) {
    status_ = "保存失敗: モーションが選択されていません";
    return false;
  }
  try {
    const std::string name = CurrentDisplayName();
    if (timeReversed) {
      MotionFiles::SaveAnim(MotionOps::CreateTimeReversed(*motion), name, path);
      status_ = "保存成功 (時間反転): " + path;
    } else {
      MotionFiles::SaveAnim(*motion, name, path);
      activeDocument_->MarkSaved();
      status_ = "保存成功: " + path;
    }
    return true;
  } catch (const std::exception &e) {
    status_ = std::string("保存失敗: ") + e.what();
    return false;
  }
}

bool MotionEditorSession::LoadSourceAnimation(const std::string &objectPath,
                                              const std::string &animationName) {
  YoRigine::Object3d *object = TargetObject();
  if (!object || !object->GetModel()) {
    status_ = "読み込み失敗: 対象オブジェクトがありません";
    return false;
  }
  try {
    object->SetChangeMotion(objectPath, MotionPlayMode::Loop, animationName);
    OpenTargetMotion();
    if (!CurrentMotion()) {
      status_ = "読み込み失敗: MotionSystemに同期できません";
      return false;
    }
    lastAppliedTime_ = -1.0f;
    playback_.ResetForNewTarget(0.0f);
    status_ = "読み込み成功: " + objectPath + " / " + animationName;
    return true;
  } catch (const std::exception &e) {
    status_ = std::string("読み込み失敗: ") + e.what();
    return false;
  }
}

bool MotionEditorSession::LoadAllSourceAnimations(
    const std::string &objectPath, const std::vector<std::string> &names,
    const std::string &restoreName) {
  YoRigine::Object3d *object = TargetObject();
  if (!object || !object->GetModel()) {
    status_ = "一括読み込み失敗: 対象オブジェクトがありません";
    return false;
  }
  if (names.empty()) {
    status_ = "一括読み込み失敗: アニメーション候補がありません";
    return false;
  }
  try {
    int loaded = 0;
    for (const auto &name : names) {
      if (name.empty()) {
        continue;
      }
      object->SetChangeMotion(objectPath, MotionPlayMode::Loop, name);
      ++loaded;
    }
    const std::string &applyName = restoreName.empty() ? names.front() : restoreName;
    if (!LoadSourceAnimation(objectPath, applyName)) {
      return false;
    }
    status_ = "一括読み込み成功: " + std::to_string(loaded) + "件";
    return true;
  } catch (const std::exception &e) {
    status_ = std::string("一括読み込み失敗: ") + e.what();
    return false;
  }
}

// ============================================================
// 履歴
// ============================================================
void MotionEditorSession::Undo() {
  if (activeDocument_ && activeDocument_->CanUndo()) {
    activeDocument_->Undo();
    status_ = "元に戻す";
  }
}

void MotionEditorSession::Redo() {
  if (activeDocument_ && activeDocument_->CanRedo()) {
    activeDocument_->Redo();
    status_ = "やり直す";
  }
}

bool MotionEditorSession::CanUndo() const {
  return activeDocument_ && activeDocument_->CanUndo();
}

bool MotionEditorSession::CanRedo() const {
  return activeDocument_ && activeDocument_->CanRedo();
}

void MotionEditorSession::ClearHistory() {
  if (activeDocument_) {
    activeDocument_->ClearHistory();
    status_ = "履歴をクリアしました";
  }
}

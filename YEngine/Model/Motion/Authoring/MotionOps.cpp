#include "MotionOps.h"

#include <algorithm>
#include <cmath>

namespace {

// 文字列内の from を全て to に置換する
void ReplaceAll(std::string &s, const std::string &from,
                const std::string &to) {
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.length(), to);
    pos += to.length();
  }
}

// 左右ペアの記法を入れ替える。a→b / b→a
// を同時に行うため、一時プレースホルダーを経由する。
void SwapPair(std::string &s, const std::string &a, const std::string &b) {
  const std::string placeholder = "__TEMP_R__";
  ReplaceAll(s, a, placeholder);
  ReplaceAll(s, b, a);
  ReplaceAll(s, placeholder, b);
}

// ボーン名の左右を入れ替える（対象記法が無ければそのまま）
std::string MirrorBoneName(const std::string &name) {
  std::string result = name;
  if (name.find("Left") != std::string::npos ||
      name.find("Right") != std::string::npos) {
    SwapPair(result, "Left", "Right");
  } else if (name.find(".L") != std::string::npos ||
             name.find(".R") != std::string::npos) {
    SwapPair(result, ".L", ".R");
  } else if (name.find("_L_") != std::string::npos ||
             name.find("_R_") != std::string::npos) {
    SwapPair(result, "_L_", "_R_");
  }
  return result;
}

// 同時刻のキーがあれば値を上書き、無ければ挿入して時間順に並べる
template <typename Keyframes, typename Value>
void InsertOrReplaceKey(Keyframes &keyframes, float time, const Value &value) {
  auto it =
      std::find_if(keyframes.begin(), keyframes.end(), [time](const auto &kf) {
        return std::abs(kf.time - time) < 1e-4f;
      });
  if (it != keyframes.end()) {
    it->value = value;
  } else {
    keyframes.push_back({time, value});
  }
  std::sort(keyframes.begin(), keyframes.end(),
            [](const auto &a, const auto &b) { return a.time < b.time; });
}

} // namespace

namespace MotionOps {

// ============================================================
// ミラー
// ============================================================
Motion CreateMirrored(const Motion &source) {
  Motion mirrored = source;

  // 値の反転（X軸ミラー）
  for (auto &[name, na] : mirrored.animation_.nodeAnimations_) {
    for (auto &kf : na.translate.keyframes) {
      kf.value.x = -kf.value.x;
    }
    for (auto &kf : na.rotate.keyframes) {
      kf.value.y = -kf.value.y;
      kf.value.z = -kf.value.z;
    }
  }

  // ボーン名の左右入れ替え（トラックごと別名のキーへ移す）
  NodeAnimationMap swapped;
  for (auto &[name, na] : mirrored.animation_.nodeAnimations_) {
    swapped[MirrorBoneName(name)] = std::move(na);
  }
  mirrored.animation_.nodeAnimations_ = std::move(swapped);

  return mirrored;
}

// ============================================================
// トリム
// ============================================================
std::optional<Motion> CreateTrimmed(const Motion &source, float startTime,
                                    float endTime) {
  if (endTime <= startTime) {
    return std::nullopt;
  }

  Motion trimmed;
  const float newDuration = endTime - startTime;
  trimmed.SetDuration(newDuration);

  // 1チャンネル分を切り出す。両端は補間値で補い、途中のキーは開始時刻分ずらす
  auto trimChannel = [&](const auto &srcKeyframes, auto &dstKeyframes,
                         Motion::InterpolationType interp) {
    if (srcKeyframes.empty())
      return;

    dstKeyframes.push_back(
        {0.0f, source.CalculateValue(srcKeyframes, startTime, interp)});

    for (const auto &kf : srcKeyframes) {
      if (kf.time > startTime && kf.time < endTime) {
        dstKeyframes.push_back({kf.time - startTime, kf.value});
      }
    }

    dstKeyframes.push_back(
        {newDuration, source.CalculateValue(srcKeyframes, endTime, interp)});
  };

  for (const auto &[name, na] : source.animation_.nodeAnimations_) {
    Motion::NodeAnimation newNA;
    newNA.interpolationType = na.interpolationType;

    trimChannel(na.translate.keyframes, newNA.translate.keyframes,
                na.interpolationType);
    trimChannel(na.rotate.keyframes, newNA.rotate.keyframes,
                na.interpolationType);
    trimChannel(na.scale.keyframes, newNA.scale.keyframes,
                na.interpolationType);

    trimmed.animation_.nodeAnimations_[name] = std::move(newNA);
  }

  return trimmed;
}

// ============================================================
// ピンポン結合
// ============================================================
Motion CreatePingPong(const Motion &source) {
  Motion pingpong;
  const float originalDuration = source.GetDuration();
  pingpong.SetDuration(originalDuration * 2.0f);

  // 正方向のキーに続けて、逆順のキーを時間反転して並べる
  auto appendPingPong = [&](const auto &src, auto &dst) {
    for (const auto &kf : src) {
      dst.push_back(kf);
    }
    for (auto it = src.rbegin(); it != src.rend(); ++it) {
      auto reversedKey = *it;
      reversedKey.time = originalDuration + (originalDuration - it->time);
      // 折り返し地点（ちょうど originalDuration のキー）が重複するのでスキップ
      if (!dst.empty() && std::abs(reversedKey.time - dst.back().time) < 1e-4f)
        continue;
      dst.push_back(reversedKey);
    }
  };

  for (const auto &[name, na] : source.animation_.nodeAnimations_) {
    Motion::NodeAnimation newNA;
    newNA.interpolationType = na.interpolationType;

    appendPingPong(na.translate.keyframes, newNA.translate.keyframes);
    appendPingPong(na.rotate.keyframes, newNA.rotate.keyframes);
    appendPingPong(na.scale.keyframes, newNA.scale.keyframes);

    pingpong.animation_.nodeAnimations_[name] = std::move(newNA);
  }

  return pingpong;
}

// ============================================================
// 区間移動の焼き込み
// ============================================================
std::optional<TranslateOffsetBakeResult>
BakeTranslateOffsetRange(const Motion &motion,
                         const TranslateOffsetBakeParams &params) {
  const Vector3 &offset = params.offset;
  if (std::abs(offset.x) < 1e-6f && std::abs(offset.y) < 1e-6f &&
      std::abs(offset.z) < 1e-6f) {
    return std::nullopt;
  }

  const float fps = static_cast<float>(params.fps);

  // 元になるノードアニメーション（無ければ空のLinearから開始）
  Motion::NodeAnimation sourceNode{};
  auto nodeIt = motion.animation_.nodeAnimations_.find(params.boneName);
  if (nodeIt != motion.animation_.nodeAnimations_.end()) {
    sourceNode = nodeIt->second;
  } else {
    sourceNode.interpolationType = Motion::InterpolationType::Linear;
  }

  // 空のチャンネルには初期姿勢のキーを1つ置いて、補間を成立させる
  if (sourceNode.translate.keyframes.empty()) {
    sourceNode.translate.keyframes.push_back({0.0f, params.fallback.translate});
  }
  if (sourceNode.rotate.keyframes.empty()) {
    sourceNode.rotate.keyframes.push_back({0.0f, params.fallback.rotate});
  }
  if (sourceNode.scale.keyframes.empty()) {
    sourceNode.scale.keyframes.push_back({0.0f, params.fallback.scale});
  }

  // 区間をモーション長の範囲に収める
  const int totalFrames =
      std::max(0, static_cast<int>(motion.GetDuration() * params.fps));
  int startFrame = std::clamp(params.startFrame, 0, totalFrames);
  int endFrame = std::clamp(params.endFrame, 0, totalFrames);
  if (endFrame < startFrame)
    std::swap(startFrame, endFrame);

  TranslateOffsetBakeResult result;
  result.animations = motion.animation_.nodeAnimations_;
  result.startFrame = startFrame;
  result.endFrame = endFrame;

  auto &targetNode = result.animations[params.boneName];
  targetNode = sourceNode;

  const float startTime = startFrame / fps;
  const float endTime = endFrame / fps;
  const Motion::InterpolationType interp = targetNode.interpolationType;
  auto &translateKeys = targetNode.translate.keyframes;

  // 区間の外へ影響が漏れないよう、開始直前の元の値をキーとして固定する
  if (startFrame > 0) {
    const float beforeTime = (startFrame - 1) / fps;
    InsertOrReplaceKey(translateKeys, beforeTime,
                       motion.CalculateValue(sourceNode.translate.keyframes,
                                             beforeTime, interp));
  }

  // 開始・終了の値は、キー追加前の元データから評価する
  const Vector3 startValue =
      motion.CalculateValue(sourceNode.translate.keyframes, startTime, interp);
  const Vector3 endValue =
      motion.CalculateValue(sourceNode.translate.keyframes, endTime, interp);
  InsertOrReplaceKey(translateKeys, startTime, startValue + offset);

  // 区間内の既存キーにも移動量を足す
  for (const auto &kf : sourceNode.translate.keyframes) {
    if (kf.time > startTime + 1e-4f && kf.time < endTime - 1e-4f) {
      InsertOrReplaceKey(translateKeys, kf.time, kf.value + offset);
    }
  }

  InsertOrReplaceKey(translateKeys, endTime, endValue + offset);

  // 区間の外へ影響が漏れないよう、終了直後の元の値をキーとして固定する
  if (endFrame < totalFrames) {
    const float afterTime = (endFrame + 1) / fps;
    InsertOrReplaceKey(translateKeys, afterTime,
                       motion.CalculateValue(sourceNode.translate.keyframes,
                                             afterTime, interp));
  }

  return result;
}

// ============================================================
// 時間反転
// ============================================================
Motion CreateTimeReversed(const Motion &source) {
  Motion reversed = source;
  const float duration = reversed.GetDuration();

  auto reverseTrack = [duration](auto &keyframes) {
    for (auto &kf : keyframes) {
      kf.time = duration - kf.time;
    }
    std::sort(keyframes.begin(), keyframes.end(),
              [](const auto &a, const auto &b) { return a.time < b.time; });
  };

  for (auto &[name, na] : reversed.animation_.nodeAnimations_) {
    reverseTrack(na.translate.keyframes);
    reverseTrack(na.rotate.keyframes);
    reverseTrack(na.scale.keyframes);
  }

  // SpeedCurve は正規化時間 [0,1] なので 1-t で反転する
  auto &speedKeys = reversed.GetSpeedCurve().curve.keyframes;
  for (auto &kf : speedKeys) {
    kf.time = 1.0f - kf.time;
  }
  std::sort(speedKeys.begin(), speedKeys.end(),
            [](const auto &a, const auto &b) { return a.time < b.time; });

  return reversed;
}

// ============================================================
// SpeedCurve の焼き込み
// ============================================================
Motion BakeSpeedCurve(const Motion &source) {
  Motion baked = source;
  if (!baked.HasSpeedCurve()) {
    return baked;
  }

  const float duration = baked.GetDuration();
  if (duration <= 0.0f) {
    return baked;
  }

  constexpr int kSamples = 512;
  // 速度 0 付近で逆数が発散しないよう下限を設ける
  constexpr float kMinSpeed = 0.05f;

  // 正規化時間 t までにかかる「再生後の正規化時間」= ∫ 1/speed dt
  // を台形積分で表にする
  std::vector<float> invLut(kSamples + 1);
  invLut[0] = 0.0f;
  for (int i = 1; i <= kSamples; ++i) {
    const float t0 = static_cast<float>(i - 1) / kSamples;
    const float t1 = static_cast<float>(i) / kSamples;
    const float s0 = std::max(baked.EvaluateSpeedCurve(t0), kMinSpeed);
    const float s1 = std::max(baked.EvaluateSpeedCurve(t1), kMinSpeed);
    invLut[i] = invLut[i - 1] + (1.0f / s0 + 1.0f / s1) * 0.5f / kSamples;
  }

  const float newDurationNorm = invLut[kSamples];
  if (newDurationNorm < 1e-6f) {
    return baked;
  }

  // 元の時刻 T → 焼き込み後の時刻（表を線形補間して引く）
  auto remapTime = [&](float T) -> float {
    const float normT = std::clamp(T / duration, 0.0f, 1.0f);
    const float fIdx = normT * static_cast<float>(kSamples);
    const int lo = static_cast<int>(fIdx);
    const int hi = std::min(lo + 1, kSamples);
    const float frac = fIdx - static_cast<float>(lo);
    const float newNormT = invLut[lo] + frac * (invLut[hi] - invLut[lo]);
    return newNormT * duration;
  };

  for (auto &[boneName, nodeAnim] : baked.animation_.nodeAnimations_) {
    auto remap = [&](auto &track) {
      for (auto &kf : track.keyframes) {
        kf.time = remapTime(kf.time);
      }
    };
    remap(nodeAnim.translate);
    remap(nodeAnim.rotate);
    remap(nodeAnim.scale);
  }

  baked.SetDuration(newDurationNorm * duration);
  baked.ClearSpeedCurve();
  return baked;
}

// ============================================================
// キー挿入
// ============================================================
void InsertPoseKey(NodeAnimationMap &animations, const std::string &bone,
                   float time, const QuaternionTransform &pose) {
  auto &na = animations[bone];
  if (na.translate.keyframes.empty() && na.rotate.keyframes.empty() &&
      na.scale.keyframes.empty()) {
    na.interpolationType = Motion::InterpolationType::Linear;
  }
  InsertOrReplaceKey(na.translate.keyframes, time, pose.translate);
  InsertOrReplaceKey(na.rotate.keyframes, time, pose.rotate);
  InsertOrReplaceKey(na.scale.keyframes, time, pose.scale);
}

// ============================================================
// キー削除
// ============================================================
RemoveResult RemoveKeysAt(NodeAnimationMap &animations, const std::string &bone,
                          float time, unsigned mask) {
  auto it = animations.find(bone);
  if (it == animations.end()) {
    return RemoveResult::NotFound;
  }
  auto &na = it->second;

  auto isAtTime = [time](const auto &kf) {
    return std::abs(kf.time - time) < 1e-4f;
  };

  // 先に「消したら空になるチャンネルが無いか」を全チャンネルで確認する
  bool anyMatch = false;
  bool wouldEmpty = false;
  auto inspect = [&](const auto &keyframes, unsigned bit) {
    if (!(mask & bit)) {
      return;
    }
    const auto matches =
        std::count_if(keyframes.begin(), keyframes.end(), isAtTime);
    if (matches == 0) {
      return;
    }
    anyMatch = true;
    if (static_cast<size_t>(matches) >= keyframes.size()) {
      wouldEmpty = true;
    }
  };
  inspect(na.translate.keyframes, kChannelTranslate);
  inspect(na.rotate.keyframes, kChannelRotate);
  inspect(na.scale.keyframes, kChannelScale);

  if (!anyMatch) {
    return RemoveResult::NotFound;
  }
  if (wouldEmpty) {
    return RemoveResult::WouldEmptyChannel;
  }

  auto erase = [&](auto &keyframes, unsigned bit) {
    if (!(mask & bit)) {
      return;
    }
    keyframes.erase(
        std::remove_if(keyframes.begin(), keyframes.end(), isAtTime),
        keyframes.end());
  };
  erase(na.translate.keyframes, kChannelTranslate);
  erase(na.rotate.keyframes, kChannelRotate);
  erase(na.scale.keyframes, kChannelScale);
  return RemoveResult::Removed;
}

// ============================================================
// 同一フレームのキーを一括移動
// ============================================================
void ShiftKeysAtFrame(NodeAnimationMap &animations, int fromFrame, int delta,
                      int fps) {
  if (delta == 0 || fps <= 0) {
    return;
  }
  const float newTime =
      std::max(0.0f, (fromFrame + delta) / static_cast<float>(fps));

  auto shift = [&](auto &keyframes) {
    for (auto &kf : keyframes) {
      if (TimeToFrame(kf.time, fps) == fromFrame) {
        kf.time = newTime;
      }
    }
    std::sort(keyframes.begin(), keyframes.end(),
              [](const auto &a, const auto &b) { return a.time < b.time; });
  };

  for (auto &[boneName, na] : animations) {
    shift(na.translate.keyframes);
    shift(na.rotate.keyframes);
    shift(na.scale.keyframes);
  }
}

// ============================================================
// チャンネルのキー時刻置き換え
// ============================================================
bool RetimeChannel(NodeAnimationMap &animations, const std::string &bone,
                   Channel channel, std::vector<float> newTimes) {
  auto it = animations.find(bone);
  if (it == animations.end()) {
    return false;
  }

  auto retime = [&](auto &keyframes) -> bool {
    if (keyframes.size() != newTimes.size()) {
      return false;
    }
    // 新旧とも昇順に並べ、k 番目同士を対応付けて時刻だけ差し替える
    std::sort(newTimes.begin(), newTimes.end());
    std::sort(keyframes.begin(), keyframes.end(),
              [](const auto &a, const auto &b) { return a.time < b.time; });
    for (size_t k = 0; k < keyframes.size(); ++k) {
      keyframes[k].time = newTimes[k];
    }
    return true;
  };

  switch (channel) {
  case Channel::Translate:
    return retime(it->second.translate.keyframes);
  case Channel::Rotate:
    return retime(it->second.rotate.keyframes);
  case Channel::Scale:
    return retime(it->second.scale.keyframes);
  }
  return false;
}

} // namespace MotionOps

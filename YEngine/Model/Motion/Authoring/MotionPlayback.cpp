#include "MotionPlayback.h"

#include "../Core/MotionSystem.h"

#include <algorithm>

namespace {
// 端に到達したとみなす許容幅（秒）
constexpr float kEdgeEpsilon = 0.01f;
} // namespace

float MotionPlayback::Duration() const {
  return motionSystem_ ? motionSystem_->GetDuration() : 0.0f;
}

void MotionPlayback::Sync() {
  if (!motionSystem_) {
    return;
  }
  time_ = motionSystem_->GetAnimationTime();

  // ピンポン: 端に着いたら向きを反転する
  if (isPingPong_ && isPlaying_) {
    const float duration = motionSystem_->GetDuration();
    if (pingPongForward_ && time_ >= duration - kEdgeEpsilon) {
      pingPongForward_ = false;
      ApplySpeed();
    } else if (!pingPongForward_ && time_ <= kEdgeEpsilon) {
      pingPongForward_ = true;
      ApplySpeed();
    }
  }
}

void MotionPlayback::Play(Motion *motion) {
  if (motionSystem_) {
    if (motion && motionSystem_->GetAnimation() != motion) {
      motionSystem_->SetAnimation(motion);
    }
    // 終端で止まっていたら頭から再生し直す
    float resumeTime = motionSystem_->GetAnimationTime();
    if (resumeTime >= motionSystem_->GetDuration() ||
        motionSystem_->IsFinished()) {
      resumeTime = 0.0f;
    }
    if (isLoop_) {
      motionSystem_->PlayLoop();
    } else {
      motionSystem_->PlayOnce();
    }
    motionSystem_->SetAnimationTime(resumeTime);
    ApplySpeed();
  }
  isPlaying_ = true;
}

void MotionPlayback::Pause() {
  if (motionSystem_) {
    motionSystem_->Stop();
  }
  isPlaying_ = false;
}

void MotionPlayback::TogglePlay(Motion *motion) {
  if (isPlaying_) {
    Pause();
  } else {
    Play(motion);
  }
}

void MotionPlayback::Stop() {
  if (motionSystem_) {
    motionSystem_->Stop();
    motionSystem_->SetAnimationTime(0.0f);
  }
  isPlaying_ = false;
  time_ = 0.0f;
}

void MotionPlayback::Seek(float time) {
  Pause();
  if (motionSystem_) {
    motionSystem_->SetAnimationTime(time);
    // SetAnimationTime は長さでクランプするので、実際の値を読み戻す
    time_ = motionSystem_->GetAnimationTime();
  } else {
    time_ = time;
  }
}

void MotionPlayback::Step(int frames, int fps) {
  if (fps <= 0) {
    return;
  }
  Seek(time_ + static_cast<float>(frames) / static_cast<float>(fps));
}

void MotionPlayback::SetSpeed(float speed) {
  speed_ = std::max(0.0f, speed);
  ApplySpeed();
}

void MotionPlayback::SetReverse(bool reverse) {
  if (isReverse_ == reverse) {
    return;
  }
  isReverse_ = reverse;
  ApplySpeed();

  // 端にいると即終了扱いになるので、進む向きの反対側の端へ移す
  if (motionSystem_) {
    const float duration = motionSystem_->GetDuration();
    const float t = motionSystem_->GetAnimationTime();
    if (isReverse_ && t <= kEdgeEpsilon) {
      motionSystem_->SetAnimationTime(duration);
    } else if (!isReverse_ && t >= duration - kEdgeEpsilon) {
      motionSystem_->SetAnimationTime(0.0f);
    }
    time_ = motionSystem_->GetAnimationTime();
  }
}

void MotionPlayback::SetPingPong(bool pingPong) {
  isPingPong_ = pingPong;
  pingPongForward_ = true;
  ApplySpeed();
}

void MotionPlayback::ResetForNewTarget(float time) {
  time_ = time;
  isPlaying_ = true;
  pingPongForward_ = true;
  ApplySpeed();
}

void MotionPlayback::ApplySpeed() {
  if (!motionSystem_) {
    return;
  }
  // 逆再生とピンポンの戻り区間は、それぞれ向きを反転させる
  bool backward = isReverse_;
  if (isPingPong_ && !pingPongForward_) {
    backward = !backward;
  }
  motionSystem_->SetMotionSpeed(backward ? -speed_ : speed_);
}

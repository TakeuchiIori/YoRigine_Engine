#include "MotionDocument.h"

#include <utility>

MotionDocument::MotionDocument(std::string key, Motion *motion)
    : key_(std::move(key)), motion_(motion) {}

void MotionDocument::CommitBone(const std::string &label,
                                const std::string &bone,
                                std::optional<Motion::NodeAnimation> before,
                                std::optional<Motion::NodeAnimation> after) {
  // nullopt はトラックを持たない状態として扱う
  auto assign = [this,
                 bone](const std::optional<Motion::NodeAnimation> &value) {
    auto &animations = motion_->animation_.nodeAnimations_;
    if (value) {
      animations[bone] = *value;
    } else {
      animations.erase(bone);
    }
    Touch();
  };

  history_.Execute(MakeLambdaCommand(
      label, [assign, after]() { assign(after); },
      [assign, before]() { assign(before); }));
}

void MotionDocument::CommitAnimations(const std::string &label,
                                      NodeAnimationMap before,
                                      NodeAnimationMap after) {
  history_.Execute(MakeLambdaCommand(
      label,
      [this, after = std::move(after)]() {
        motion_->animation_.nodeAnimations_ = after;
        Touch();
      },
      [this, before = std::move(before)]() {
        motion_->animation_.nodeAnimations_ = before;
        Touch();
      }));
}

void MotionDocument::CommitMotion(const std::string &label, Motion before,
                                  Motion after) {
  history_.Execute(MakeLambdaCommand(
      label,
      [this, after = std::move(after)]() {
        *motion_ = after;
        Touch();
      },
      [this, before = std::move(before)]() {
        *motion_ = before;
        Touch();
      }));
}

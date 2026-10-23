#include "ShatterTransition.h"

// C++
#include <algorithm>
#include <cmath>

// Engine
#include "OffScreen/PostEffectManager.h"
#include "Systems/GameTime/GameTime.h"
#include "WinApp/WinApp.h"

namespace {
constexpr float kPi = 3.1415926f;

// ブラーの強さ（遷移の中央で最大になる）
constexpr float kMaxBlurWidth = 0.02f;
constexpr int kBaseBlurSamples = 8;
constexpr int kExtraBlurSamples = 16;
} // namespace

/// <summary>
/// 初期化処理（エフェクトを無効状態で追加しておく）
/// </summary>
void ShatterTransition::Initialize() {
  auto *postEffectManager = PostEffectManager::GetInstance();

  shatterEffectIndex_ = postEffectManager->AddEffect(
      OffScreen::OffScreenEffectType::ShatterTransition, "SceneTransition");
  radialBlurEffectIndex_ = postEffectManager->AddEffect(
      OffScreen::OffScreenEffectType::RadialBlur, "SceneTransitionBlur");

  SetEffectsEnabled(false);
  phase_ = Phase::None;
}

/// <summary>
/// 更新処理
/// </summary>
void ShatterTransition::Update() {
  if (phase_ == Phase::None) {
    return;
  }

  // 段階の終わりで止め、次の段階は呼び出し側の StartReveal() で進める
  time_ =
      std::min(time_ + YoRigine::GameTime::GetDeltaTime(), GetPhaseEndTime());
  ApplyEffects(time_ / duration_);

  // 明け終わったらエフェクトを外して待機に戻る
  if (phase_ == Phase::Reveal && time_ >= duration_) {
    SetEffectsEnabled(false);
    phase_ = Phase::None;
  }
}

/// <summary>
/// 現在の段階が終わったか
/// </summary>
bool ShatterTransition::IsFinished() const {
  return phase_ == Phase::None || time_ >= GetPhaseEndTime();
}

/// <summary>
/// 覆い始める（砕け始める）
/// </summary>
void ShatterTransition::StartCover() {
  phase_ = Phase::Cover;
  time_ = 0.0f;
  SetEffectsEnabled(true);
  ApplyEffects(0.0f);
}

/// <summary>
/// 明け始める（切り替え時点の続きから最後まで進める）
/// </summary>
void ShatterTransition::StartReveal() {
  phase_ = Phase::Reveal;
  time_ = std::max(time_, duration_ * switchPoint_);
}

/// <summary>
/// エフェクトのパラメータ反映
/// </summary>
void ShatterTransition::ApplyEffects(float progress) {
  auto *postEffectManager = PostEffectManager::GetInstance();

  //------------------------------------------------------------
  // 破片エフェクト
  //------------------------------------------------------------
  if (auto *shatterData =
          postEffectManager->GetEffectData(shatterEffectIndex_)) {
    auto &shatter = shatterData->params.shatter;
    shatter.progress = progress;
    shatter.resolution = {static_cast<float>(WinApp::kClientWidth),
                          static_cast<float>(WinApp::kClientHeight)};
    shatter.time = time_;
  }

  //------------------------------------------------------------
  // ブラー（遷移の中央で最も強くなる山なりのカーブ）
  //------------------------------------------------------------
  if (auto *blurData =
          postEffectManager->GetEffectData(radialBlurEffectIndex_)) {
    const float strength = std::sin(progress * kPi);
    auto &blur = blurData->params.radialBlur;
    blur.center = {0.5f, 0.5f};
    blur.width = kMaxBlurWidth * strength;
    blur.sampleCount =
        kBaseBlurSamples + static_cast<int>(kExtraBlurSamples * strength);
    blur.isRadial = true;
  }
}

/// <summary>
/// エフェクトの有効 / 無効
/// </summary>
void ShatterTransition::SetEffectsEnabled(bool enabled) {
  auto *postEffectManager = PostEffectManager::GetInstance();
  postEffectManager->SetEffectEnabled(shatterEffectIndex_, enabled);
  postEffectManager->SetEffectEnabled(radialBlurEffectIndex_, enabled);
}

/// <summary>
/// 現在の段階が終わる時刻
/// </summary>
float ShatterTransition::GetPhaseEndTime() const {
  return phase_ == Phase::Cover ? duration_ * switchPoint_ : duration_;
}

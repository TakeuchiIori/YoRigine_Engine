#pragma once

// Engine
#include "../Base/ISceneTransition.h"

/// <summary>
/// 画面が砕けるシーン遷移（ShatterTransition + RadialBlur のポストエフェクト）
/// 描画はポストエフェクトチェーンが行うので、Draw() は何もしない。
/// ※ PostEffectManager::Reset() でエフェクトが消えるため、Reset を挟む
///   メインシーン遷移ではなく、シーン内の切り替え（サブシーン）で使う。
/// </summary>
class ShatterTransition : public ISceneTransition {
public:
  ///************************* 基本関数 *************************///
  void Initialize() override;
  void Update() override;
  void Draw() override {}
  bool IsFinished() const override;
  void StartCover() override;
  void StartReveal() override;

  ///************************* 設定 *************************///
  // 遷移全体（覆う＋明ける）の秒数
  void SetDuration(float duration) { duration_ = duration; }
  // 全体のうち何割の時点でシーンを切り替えるか（0〜1）
  void SetSwitchPoint(float switchPoint) { switchPoint_ = switchPoint; }

private:
  ///************************* 内部処理 *************************///
  // 進行度 0〜1 をエフェクトのパラメータへ反映
  void ApplyEffects(float progress);
  void SetEffectsEnabled(bool enabled);

  // 現在の段階が終わる時刻（秒）
  float GetPhaseEndTime() const;

private:
  ///************************* メンバ変数 *************************///
  enum class Phase { None, Cover, Reveal };
  Phase phase_ = Phase::None;

  // 遷移開始からの経過秒。覆う → 明けるで通しのタイムラインとして進める
  float time_ = 0.0f;
  float duration_ = 1.0f;
  float switchPoint_ = 0.6f;

  // PostEffectManager に追加したエフェクトの番号
  int shatterEffectIndex_ = -1;
  int radialBlurEffectIndex_ = -1;
};

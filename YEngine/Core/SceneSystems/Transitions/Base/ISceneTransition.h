#pragma once
/// <summary>
/// シーン遷移のインターフェース
/// 遷移は「覆う（旧シーンを隠す）→ シーン切り替え →
/// 明ける（新シーンを見せる）」の2段階。 呼び出し側は StartCover → IsFinished
/// を待って切り替え → StartReveal → IsFinished の順に使う。
/// </summary>
class ISceneTransition {
public:
  virtual ~ISceneTransition() = default;
  virtual void Initialize() = 0;
  virtual void Update() = 0;
  virtual void Draw() = 0;
  // 現在の段階が終わったか
  virtual bool IsFinished() const = 0;
  // 画面を覆い始める（フェードなら暗転していく）
  virtual void StartCover() = 0;
  // 画面を明け始める（フェードなら明るくなっていく）
  virtual void StartReveal() = 0;
};

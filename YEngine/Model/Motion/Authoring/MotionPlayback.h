#pragma once

class Motion;
class MotionSystem;

// ============================================================
// エディタのプレビュー再生制御
//
// ・再生/停止/シーク/速度/逆再生/ピンポンの状態を一元管理する
// ・MotionSystem に渡す速度は常にここで計算する（符号を手で合わせない）
// ・対象の MotionSystem は毎フレーム Bind
// し直す（選択オブジェクトが変わるため）
// ============================================================
class MotionPlayback {
public:
  // 操作対象の MotionSystem を設定する（nullptr 可）
  void Bind(MotionSystem *motionSystem) { motionSystem_ = motionSystem; }

  // 毎フレーム呼ぶ。MotionSystem の時刻を取り込み、ピンポンの折り返しを行う
  void Sync();

  // ------------------------------------------------------------
  // 再生制御
  // ------------------------------------------------------------

  // 再生開始。motion が現在の再生対象と違えば差し替える
  void Play(Motion *motion);
  void Pause();
  void TogglePlay(Motion *motion);
  // 停止して先頭へ戻す
  void Stop();
  // 指定時刻へ移動する（再生中なら一時停止する）
  void Seek(float time);
  // 1フレーム送り / 戻し（一時停止する）
  void Step(int frames, int fps);

  // ------------------------------------------------------------
  // 再生設定
  // ------------------------------------------------------------
  void SetLoop(bool loop) { isLoop_ = loop; }
  void SetSpeed(float speed);
  void SetReverse(bool reverse);
  void SetPingPong(bool pingPong);

  // ------------------------------------------------------------
  // 状態
  // ------------------------------------------------------------
  bool IsPlaying() const { return isPlaying_; }
  bool IsLoop() const { return isLoop_; }
  bool IsReverse() const { return isReverse_; }
  bool IsPingPong() const { return isPingPong_; }
  float Speed() const { return speed_; }
  float Time() const { return time_; }
  float Duration() const;

  // 対象切り替え時などに、表示上の状態だけ初期化する
  void ResetForNewTarget(float time);

private:
  // 逆再生・ピンポンの向きを反映した実効速度を MotionSystem に設定する
  void ApplySpeed();

  MotionSystem *motionSystem_ = nullptr;

  bool isPlaying_ = true;
  bool isLoop_ = true;
  bool isReverse_ = false;
  bool isPingPong_ = false;
  bool pingPongForward_ = true; // ピンポン中に順方向へ進んでいるか
  float speed_ = 1.0f;
  float time_ = 0.0f;
};

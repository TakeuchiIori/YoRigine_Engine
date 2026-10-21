#pragma once
#include "IMotionEditorPanel.h"

#include <cstdint>
#include <utility>
#include <vector>

class Motion;

// ============================================================
// タイムスケールカーブ（再生速度の時間変化）の編集パネル
//
// ・X = 正規化時間 [0,1]、Y = 速度倍率
// ・編集中の制御点はパネルが持ち、「適用」「焼き込み」で Session へ渡す
// ============================================================
class SpeedCurvePanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;

private:
  struct CurvePoint {
    float x = 0.0f;
    float y = 1.0f;
  };

  // モーションの SpeedCurve を制御点として読み込む
  void PullFromMotion(const Motion *motion);
  std::vector<std::pair<float, float>> ToPairs() const;

  // カーブエディタ本体を描く（変更があれば true）
  bool DrawCurveEditor(float width, float height);

  // 制御点の操作
  bool AddPoint(CurvePoint point);
  void RemovePoint(int index);
  float Evaluate(float x) const;

  std::vector<CurvePoint> points_;
  float maxSpeed_ = 4.0f;

  // ドラッグ・右クリック
  int dragIndex_ = -1;
  int contextIndex_ = -1;

  // 読み込み元の判定（モーション切り替えや Undo を検出して読み直す）
  const Motion *pulledMotion_ = nullptr;
  uint64_t pulledRevision_ = 0;
  bool isDirty_ = false;
};

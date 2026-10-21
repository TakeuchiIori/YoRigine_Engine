#include "SpeedCurvePanel.h"

#include "../../Authoring/MotionEditorSession.h"
#include "../../Core/Motion.h"

#ifdef USE_IMGUI
#include <IconsFontAwesome5.h>
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kSpeedStep = 0.1f;   // 速度倍率のスナップ幅
constexpr float kMinGapX = 0.001f;   // 隣接点どうしの最小間隔
constexpr float kDuplicateX = 0.03f; // 追加時に「同じ位置」とみなす幅
constexpr float kHitRadius = 8.0f;   // 制御点のヒット判定半径(px)

float SnapSpeed(float value) { return std::round(value / kSpeedStep) * kSpeedStep; }
} // namespace

// ============================================================
// 制御点の操作
// ============================================================
void SpeedCurvePanel::PullFromMotion(const Motion *motion) {
  points_.clear();
  if (motion) {
    for (const auto &kf : motion->GetSpeedCurve().curve.keyframes) {
      points_.push_back({kf.time, kf.value});
    }
  }
  // 端点（x=0, x=1）は常に存在させる
  if (points_.empty()) {
    points_ = {{0.0f, 1.0f}, {1.0f, 1.0f}};
  }
  std::sort(points_.begin(), points_.end(),
            [](const CurvePoint &a, const CurvePoint &b) { return a.x < b.x; });
  points_.front().x = 0.0f;
  points_.back().x = 1.0f;
  isDirty_ = false;
}

std::vector<std::pair<float, float>> SpeedCurvePanel::ToPairs() const {
  std::vector<std::pair<float, float>> pairs;
  pairs.reserve(points_.size());
  for (const auto &p : points_) {
    pairs.emplace_back(p.x, p.y);
  }
  return pairs;
}

bool SpeedCurvePanel::AddPoint(CurvePoint point) {
  point.x = std::clamp(point.x, 0.0f, 1.0f);
  point.y = std::clamp(point.y, 0.0f, maxSpeed_);
  for (const auto &p : points_) {
    if (std::abs(p.x - point.x) < kDuplicateX) {
      return false;
    }
  }
  points_.push_back(point);
  std::sort(points_.begin(), points_.end(),
            [](const CurvePoint &a, const CurvePoint &b) { return a.x < b.x; });
  return true;
}

void SpeedCurvePanel::RemovePoint(int index) {
  // 端点は消さない
  if (index <= 0 || index >= static_cast<int>(points_.size()) - 1) {
    return;
  }
  points_.erase(points_.begin() + index);
}

float SpeedCurvePanel::Evaluate(float x) const {
  if (points_.size() < 2) return 1.0f;
  if (x <= points_.front().x) return points_.front().y;
  if (x >= points_.back().x) return points_.back().y;
  for (size_t i = 0; i + 1 < points_.size(); ++i) {
    if (x <= points_[i + 1].x) {
      const float range = points_[i + 1].x - points_[i].x;
      const float t = range > 1e-6f ? (x - points_[i].x) / range : 0.0f;
      return points_[i].y + t * (points_[i + 1].y - points_[i].y);
    }
  }
  return 1.0f;
}

// ============================================================
// カーブエディタ
// ============================================================
bool SpeedCurvePanel::DrawCurveEditor(float width, float height) {
#ifdef USE_IMGUI
  bool changed = false;

  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 rMin = ImGui::GetCursorScreenPos();
  const ImVec2 rMax = {rMin.x + width, rMin.y + height};
  const float yRange = maxSpeed_;

  // カーブ空間 ⇔ スクリーン座標
  auto toScreen = [&](CurvePoint p) {
    return ImVec2{rMin.x + std::clamp(p.x, 0.0f, 1.0f) * width,
                  rMax.y - std::clamp(p.y, 0.0f, maxSpeed_) / yRange * height};
  };
  auto toLocal = [&](ImVec2 s) {
    return CurvePoint{(s.x - rMin.x) / width,
                      maxSpeed_ - (s.y - rMin.y) / height * yRange};
  };
  auto hitTest = [&](ImVec2 s) {
    int best = -1;
    float bestD2 = kHitRadius * kHitRadius;
    for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
      const ImVec2 p = toScreen(points_[i]);
      const float d2 = (p.x - s.x) * (p.x - s.x) + (p.y - s.y) * (p.y - s.y);
      if (d2 < bestD2) {
        bestD2 = d2;
        best = i;
      }
    }
    return best;
  };

  ImGui::InvisibleButton("##curveArea", {width, height},
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight);
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 mouse = ImGui::GetMousePos();

  // 背景とグリッド
  dl->AddRectFilled(rMin, rMax, IM_COL32(28, 28, 32, 255), 4.0f);
  dl->AddRect(rMin, rMax, IM_COL32(80, 80, 90, 200), 4.0f);
  for (float xs : {0.25f, 0.5f, 0.75f}) {
    const float px = rMin.x + xs * width;
    dl->AddLine({px, rMin.y}, {px, rMax.y}, IM_COL32(60, 60, 70, 140), 0.8f);
  }
  for (float y = 0.0f; y <= maxSpeed_ + 1e-4f; y += 0.5f) {
    const float py = rMax.y - y / yRange * height;
    const bool isOne = std::abs(y - 1.0f) < 0.01f; // 等速の線を強調
    dl->AddLine({rMin.x, py}, {rMax.x, py},
                isOne ? IM_COL32(120, 220, 120, 160) : IM_COL32(60, 60, 70, 130),
                isOne ? 1.2f : 0.8f);
    char label[8];
    std::snprintf(label, sizeof(label), "%.1f", y);
    dl->AddText({rMin.x + 3.0f, py - 10.0f}, IM_COL32(120, 120, 140, 160), label);
  }

  // 現在の再生位置
  if (const Motion *motion = session_->CurrentMotion();
      motion && motion->GetDuration() > 0.0f) {
    const float nt =
        std::clamp(session_->Playback().Time() / motion->GetDuration(), 0.0f, 1.0f);
    const float sx = rMin.x + nt * width;
    dl->AddLine({sx, rMin.y}, {sx, rMax.y}, IM_COL32(80, 200, 255, 100), 1.0f);
  }

  // カーブ（1px ごとに評価して折れ線で描く）
  const int segments = std::max(1, static_cast<int>(width));
  ImVec2 prev = toScreen({0.0f, Evaluate(0.0f)});
  for (int i = 1; i <= segments; ++i) {
    const float x = static_cast<float>(i) / segments;
    const ImVec2 cur = toScreen({x, Evaluate(x)});
    dl->AddLine(prev, cur, IM_COL32(68, 210, 255, 230), 2.0f);
    prev = cur;
  }

  // 左ドラッグ: 点の移動（端点は X 固定、隣の点は追い越さない）
  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    dragIndex_ = hitTest(mouse);
  }
  if (dragIndex_ >= 0) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      CurvePoint local = toLocal(mouse);
      const int last = static_cast<int>(points_.size()) - 1;
      if (dragIndex_ == 0) {
        local.x = 0.0f;
      } else if (dragIndex_ == last) {
        local.x = 1.0f;
      } else {
        local.x = std::clamp(local.x, points_[dragIndex_ - 1].x + kMinGapX,
                             points_[dragIndex_ + 1].x - kMinGapX);
      }
      local.y = std::clamp(SnapSpeed(local.y), 0.0f, maxSpeed_);
      if (local.x != points_[dragIndex_].x || local.y != points_[dragIndex_].y) {
        points_[dragIndex_] = local;
        changed = true;
      }
    } else {
      dragIndex_ = -1;
    }
  }

  // 右クリック: 空白なら点を追加、点の上ならメニュー
  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    contextIndex_ = hitTest(mouse);
    if (contextIndex_ >= 0) {
      ImGui::OpenPopup("##curveContext");
    } else {
      CurvePoint local = toLocal(mouse);
      local.y = SnapSpeed(local.y);
      changed |= AddPoint(local);
    }
  }
  if (ImGui::BeginPopup("##curveContext")) {
    const int last = static_cast<int>(points_.size()) - 1;
    if (contextIndex_ >= 0 && contextIndex_ <= last) {
      if (contextIndex_ == 0 || contextIndex_ == last) {
        ImGui::TextDisabled("端点は削除できません");
      } else if (ImGui::MenuItem(ICON_FA_TIMES "  この点を削除")) {
        RemovePoint(contextIndex_);
        contextIndex_ = -1;
        changed = true;
      }
    }
    if (contextIndex_ >= 0 && contextIndex_ <= static_cast<int>(points_.size()) - 1) {
      ImGui::Separator();
      ImGui::Text("t = %.3f", points_[contextIndex_].x);
      float y = points_[contextIndex_].y;
      ImGui::SetNextItemWidth(100.0f);
      if (ImGui::DragFloat("倍率##ctxY", &y, kSpeedStep, 0.0f, maxSpeed_, "%.1fx")) {
        points_[contextIndex_].y = SnapSpeed(y);
        changed = true;
      }
    }
    ImGui::EndPopup();
  }

  // 制御点
  for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
    const ImVec2 sp = toScreen(points_[i]);
    const bool isEnd = (i == 0 || i == static_cast<int>(points_.size()) - 1);
    const bool isDrag = (i == dragIndex_);
    dl->AddCircleFilled(sp, 7.5f, IM_COL32(0, 0, 0, 120));
    dl->AddCircleFilled(sp, 6.0f,
                        isDrag  ? IM_COL32(255, 230, 80, 255)
                        : isEnd ? IM_COL32(80, 200, 255, 255)
                                : IM_COL32(240, 240, 240, 255));
    if (hovered) {
      const float dx = mouse.x - sp.x;
      const float dy = mouse.y - sp.y;
      if (dx * dx + dy * dy < kHitRadius * kHitRadius) {
        ImGui::SetTooltip("t=%.3f  speed=%.1fx", points_[i].x, points_[i].y);
      }
    }
  }

  return changed;
#else
  (void)width;
  (void)height;
  return false;
#endif
}

// ============================================================
// パネル
// ============================================================
void SpeedCurvePanel::DrawImGui() {
#ifdef USE_IMGUI
  const Motion *motion = session_->CurrentMotion();
  const uint64_t revision =
      session_->Document() ? session_->Document()->Revision() : 0;

  // モーションが切り替わった / Undo などで中身が変わった（未編集時）なら読み直す
  if (motion != pulledMotion_ || (!isDirty_ && revision != pulledRevision_)) {
    PullFromMotion(motion);
    pulledMotion_ = motion;
    pulledRevision_ = revision;
  }

  ImGui::PushID("SpeedCurvePanel");
  const char *header = isDirty_ ? ICON_FA_BOLT " タイムスケールカーブ *###speedCurve"
                                : ICON_FA_BOLT " タイムスケールカーブ###speedCurve";
  if (!ImGui::CollapsingHeader(header)) {
    ImGui::PopID();
    return;
  }
  if (!motion) {
    ImGui::TextDisabled("モーションが選択されていません");
    ImGui::PopID();
    return;
  }

  ImGui::SetNextItemWidth(160.0f);
  ImGui::SliderFloat("速度上限##maxSpeed", &maxSpeed_, 1.0f, 8.0f, "%.1fx");
  ImGui::SameLine();
  ImGui::TextDisabled("右クリック(空白): 点を追加 / 右クリック(点): メニュー / 左ドラッグ: 移動");

  if (DrawCurveEditor(ImGui::GetContentRegionAvail().x, 160.0f)) {
    isDirty_ = true;
  }

  ImGui::BeginDisabled(!isDirty_);
  if (ImGui::Button(ICON_FA_BOLT " 適用", ImVec2(80, 0))) {
    session_->ApplySpeedCurve(ToPairs());
    isDirty_ = false;
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("カーブを再生速度として反映します（キーは変更しません）");
  }

  ImGui::SameLine(0, 6);
  if (ImGui::Button(ICON_FA_SAVE " 焼き込み", ImVec2(90, 0))) {
    session_->BakeSpeedCurve(ToPairs());
    isDirty_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("カーブをキー時刻に積分して焼き込み、等速に戻します\n"
                      ".anim は SpeedCurve を保存しないので、保存前に焼き込んでください\n"
                      "（Ctrl+Z で戻せます）");
  }

  ImGui::SameLine(0, 6);
  if (ImGui::Button(ICON_FA_UNDO " リセット", ImVec2(90, 0))) {
    session_->ResetSpeedCurve();
    isDirty_ = false;
  }

  if (motion->GetDuration() > 0.0f) {
    const float nt = std::clamp(session_->Playback().Time() / motion->GetDuration(),
                                0.0f, 1.0f);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "現在: %.2fx (t=%.2f)",
                       Evaluate(nt), nt);
  }

  ImGui::PopID();
#endif
}

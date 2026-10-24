#include "UpdateColorOverLifetime.h"

#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget_Gradient.h"
#endif

UpdateColorOverLifetime::UpdateColorOverLifetime() {
  // デフォルト: 炎グラデーション
  gradient_.space = ColorGradient::Space::Oklab;
  gradient_.stops = {
      {0.0f, Vector4{1.0f, 1.0f, 0.8f, 1.0f}}, // 白黄
      {0.3f, Vector4{1.0f, 0.5f, 0.0f, 1.0f}}, // オレンジ
      {0.7f, Vector4{0.8f, 0.1f, 0.0f, 0.5f}}, // 赤（半透明）
      {1.0f, Vector4{0.2f, 0.0f, 0.0f, 0.0f}}, // 暗赤（透明）
  };
  gradient_.MarkDirty();
}

void UpdateColorOverLifetime::OnUpdate(ParticleAttribute *attrs, uint32_t index,
                                       float) {
  attrs[index].color = gradient_.Sample(attrs[index].GetNormalizedAge());
}

void UpdateColorOverLifetime::DrawEditor() {
#ifdef USE_IMGUI
  YEditorWidget::Gradient("寿命でのカラー & アルファ", gradient_);
#endif
}

void UpdateColorOverLifetime::SaveToJson(nlohmann::json &json) const {
  gradient_.SaveToJson(json);
}

void UpdateColorOverLifetime::LoadFromJson(const nlohmann::json &json) {
  // 旧フォーマット {"stops":[{"t","color"}]} もそのまま読める（space 無し →
  // RGB補間）
  gradient_.LoadFromJson(json);
}

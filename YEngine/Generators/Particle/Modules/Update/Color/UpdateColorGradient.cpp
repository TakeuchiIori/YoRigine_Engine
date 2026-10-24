#include "UpdateColorGradient.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget_Gradient.h"
#endif

UpdateColorGradient::UpdateColorGradient() {
  // デフォルトで炎のグラデーション
  gradient_.space = ColorGradient::Space::Oklab;
  gradient_.stops = {
      {0.0f, {1.0f, 1.0f, 0.0f, 1.0f}}, // 黄色
      {0.5f, {1.0f, 0.5f, 0.0f, 1.0f}}, // オレンジ
      {1.0f, {1.0f, 0.0f, 0.0f, 0.0f}}, // 赤（透明）
  };
  gradient_.MarkDirty();
}

void UpdateColorGradient::OnUpdate(ParticleAttribute *attrs, uint32_t index,
                                   float dt) {
  (void)dt;
  attrs[index].color = gradient_.Sample(attrs[index].GetNormalizedAge());
}

void UpdateColorGradient::DrawEditor() {
#ifdef USE_IMGUI
  YEditorWidget::Gradient("寿命に応じた多色グラデーション", gradient_);
#endif
}

void UpdateColorGradient::SaveToJson(nlohmann::json &json) const {
  gradient_.SaveToJson(json);
}

void UpdateColorGradient::LoadFromJson(const nlohmann::json &json) {
  if (json.contains("stops")) {
    gradient_.LoadFromJson(json);
    return;
  }

  // 旧フォーマット {"colorCount", "colors":[{x,y,z,w}]}:
  // 等間隔の直線補間だったので見た目を変えずに変換する
  if (json.contains("colors") && json["colors"].is_array()) {
    std::vector<Vector4> colors;
    for (const auto &c : json["colors"]) {
      colors.push_back({c["x"].get<float>(), c["y"].get<float>(),
                        c["z"].get<float>(), c["w"].get<float>()});
    }
    const int count =
        std::min(json.value("colorCount", static_cast<int>(colors.size())),
                 static_cast<int>(colors.size()));
    gradient_ = ColorGradient::FromEvenColors(colors.data(), count,
                                              ColorGradient::Space::LinearRGB);
    gradient_.MarkDirty();
  }
}

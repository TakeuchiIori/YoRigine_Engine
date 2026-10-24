#include "UpdateColorGradient.h"
#include <algorithm>
#include <vector>

void UpdateColorGradient::LoadFromJson(const nlohmann::json &json) {
  if (json.contains("stops")) {
    UpdateColorOverLifetime::LoadFromJson(json);
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
        (std::min)(json.value("colorCount", static_cast<int>(colors.size())),
                   static_cast<int>(colors.size()));
    gradient_ = ColorGradient::FromEvenColors(colors.data(), count,
                                              ColorGradient::Space::LinearRGB);
    gradient_.MarkDirty();
  }
  multiplyInitialColor_ = json.value("multiplyInitialColor", false);
}

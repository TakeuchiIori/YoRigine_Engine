#include "ColorGradient.h"
#include <algorithm>
#include <cmath>

namespace {

//=================================================================
// 色空間変換
// imgui_gradient（エディタ表示側）と同じ式を使い、プレビューと実行結果を一致させる。
// sRGB 式: https://entropymine.com/imageworsener/srgbformula/
// Oklab : https://bottosson.github.io/posts/oklab/
//=================================================================

struct Vec3 {
  float x, y, z;
};

// 負だけを 0 に丸める。1 を超える値（HDR。Bloom を乗せる発光色）はそのまま通す
// （imgui_gradient は 0〜1 に丸めて表示するが、実行時は HDR を保ちたいため上側は丸めない）
float NonNegative(float v) { return (std::max)(v, 0.0f); }

float LinearFromSrgb(float c) {
  c = NonNegative(c);
  return c < 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float SrgbFromLinear(float c) {
  c = NonNegative(c);
  return c < 0.0031308f ? c * 12.92f
                        : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

Vec3 OklabFromLinear(const Vec3 &c) {
  const float l = std::cbrt(0.4122214708f * c.x + 0.5363325363f * c.y +
                            0.0514459929f * c.z);
  const float m = std::cbrt(0.2119034982f * c.x + 0.6806995451f * c.y +
                            0.1073969566f * c.z);
  const float s = std::cbrt(0.0883024619f * c.x + 0.2817188376f * c.y +
                            0.6299787005f * c.z);
  return {
      0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s,
      1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
      0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s,
  };
}

Vec3 LinearFromOklab(const Vec3 &c) {
  const float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
  const float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
  const float s_ = c.x - 0.0894841775f * c.y - 1.2914855480f * c.z;
  const float l = l_ * l_ * l_;
  const float m = m_ * m_ * m_;
  const float s = s_ * s_ * s_;
  return {
      +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
      -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
      -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
  };
}

/// sRGB ストレート → Oklab 乗算済みアルファ
Vector4 ToOklabPremultiplied(const Vector4 &c) {
  const Vec3 lab = OklabFromLinear(
      {LinearFromSrgb(c.x), LinearFromSrgb(c.y), LinearFromSrgb(c.z)});
  return {lab.x * c.w, lab.y * c.w, lab.z * c.w, c.w};
}

/// Oklab 乗算済みアルファ → sRGB ストレート
Vector4 FromOklabPremultiplied(const Vector4 &c) {
  // α=0 だと色が復元できないので黒扱い（どのみち見えない）
  if (c.w <= 1e-6f)
    return {0.0f, 0.0f, 0.0f, 0.0f};
  const Vec3 rgb = LinearFromOklab({c.x / c.w, c.y / c.w, c.z / c.w});
  return {SrgbFromLinear(rgb.x), SrgbFromLinear(rgb.y), SrgbFromLinear(rgb.z),
          c.w};
}

Vector4 Lerp4(const Vector4 &a, const Vector4 &b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
          a.w + (b.w - a.w) * t};
}

} // namespace

//=================================================================
// 評価
//=================================================================

Vector4 ColorGradient::Evaluate(float t) const {
  if (stops.empty())
    return {1.0f, 1.0f, 1.0f, 1.0f};
  if (t <= stops.front().t)
    return stops.front().color;
  if (t >= stops.back().t)
    return stops.back().color;

  for (size_t i = 0; i + 1 < stops.size(); ++i) {
    const Stop &a = stops[i];
    const Stop &b = stops[i + 1];
    if (t < a.t || t > b.t)
      continue;

    const float local = (b.t > a.t) ? (t - a.t) / (b.t - a.t) : 0.0f;
    if (space == Space::Oklab) {
      return FromOklabPremultiplied(Lerp4(
          ToOklabPremultiplied(a.color), ToOklabPremultiplied(b.color), local));
    }
    return Lerp4(a.color, b.color, local);
  }
  return stops.back().color;
}

Vector4 ColorGradient::Sample(float t) const {
  if (lutDirty_)
    BakeLut();

  // LUT の隣接 2 点を直線補間（128 分割なので見た目の差は出ない）
  const float f = std::clamp(t, 0.0f, 1.0f) * (kLutSize - 1);
  const int i = std::min(static_cast<int>(f), kLutSize - 2);
  return Lerp4(lut_[i], lut_[i + 1], f - static_cast<float>(i));
}

void ColorGradient::BakeLut() const {
  for (int i = 0; i < kLutSize; ++i) {
    lut_[i] = Evaluate(static_cast<float>(i) / (kLutSize - 1));
  }
  lutDirty_ = false;
}

//=================================================================
// 編集ヘルパー
//=================================================================

void ColorGradient::SortStops() {
  std::stable_sort(stops.begin(), stops.end(),
                   [](const Stop &a, const Stop &b) { return a.t < b.t; });
  MarkDirty();
}

ColorGradient ColorGradient::FromEvenColors(const Vector4 *colors, int count,
                                            Space space) {
  ColorGradient g;
  g.space = space;
  g.stops.clear();
  for (int i = 0; i < count; ++i) {
    const float t = (count > 1) ? static_cast<float>(i) / (count - 1) : 0.0f;
    g.stops.push_back({t, colors[i]});
  }
  return g;
}

bool operator==(const ColorGradient &a, const ColorGradient &b) {
  if (a.space != b.space || a.stops.size() != b.stops.size())
    return false;
  for (size_t i = 0; i < a.stops.size(); ++i) {
    const auto &sa = a.stops[i];
    const auto &sb = b.stops[i];
    if (sa.t != sb.t || sa.color.x != sb.color.x || sa.color.y != sb.color.y ||
        sa.color.z != sb.color.z || sa.color.w != sb.color.w) {
      return false;
    }
  }
  return true;
}

//=================================================================
// シリアライズ
//=================================================================

void ColorGradient::SaveToJson(nlohmann::json &json) const {
  json["space"] = static_cast<int>(space);
  nlohmann::json arr = nlohmann::json::array();
  for (const Stop &s : stops) {
    arr.push_back(
        {{"t", s.t}, {"color", {s.color.x, s.color.y, s.color.z, s.color.w}}});
  }
  json["stops"] = arr;
}

void ColorGradient::LoadFromJson(const nlohmann::json &json) {
  // space
  // が無いのは旧フォーマット（直線補間だった）なので、見た目を変えないよう
  // LinearRGB で読む
  space = static_cast<Space>(
      json.value("space", static_cast<int>(Space::LinearRGB)));

  if (json.contains("stops") && json["stops"].is_array()) {
    stops.clear();
    for (const auto &s : json["stops"]) {
      const auto &c = s["color"];
      stops.push_back({s.value("t", 0.0f),
                       {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(),
                        c[3].get<float>()}});
    }
    SortStops();
  }
  MarkDirty();
}

#include "YColorGradientModule.h"

#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget_Gradient.h"
#include <imgui.h>
#endif

namespace YColorGradientModule {

void WriteTo(YGpuParticle::ParticleExtParameters &dst,
             const ColorGradientParams &src) {
  dst.colorGradientEnable = src.isEnable ? 1u : 0u;
  if (!src.isEnable)
    return;

  // ColorGradient の LUT（128 段）から GPU 用の表（32 段）を等間隔に取り出す
  constexpr int kSize = YGpuParticle::kGpuColorGradientLutSize;
  for (int i = 0; i < kSize; ++i) {
    dst.colorGradientLut[i] =
        src.gradient.Sample(static_cast<float>(i) / (kSize - 1));
  }
}

#ifdef USE_IMGUI
bool DrawImGui(ColorGradientParams &params) {
  const bool changed =
      YEditorWidget::Gradient("寿命でのカラー & アルファ", params.gradient);
  ImGui::TextDisabled(
      "有効な間は「色」の開始色 / 終了色とそのばらつきは使われません");
  return changed;
}
#endif

} // namespace YColorGradientModule

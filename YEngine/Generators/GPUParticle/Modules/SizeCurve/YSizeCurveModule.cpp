#include "YSizeCurveModule.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace YSizeCurveModule {

	void WriteTo(YGpuParticle::ParticleExtParameters& dst, const SizeCurveParams& src)
	{
		dst.sizeCurveEnable = src.isEnable ? 1u : 0u;
		if (!src.isEnable) return;

		// カーブを等間隔にサンプルし、float4 1 個に 4 段ずつ詰める（HLSL 側は [i/4][i%4] で読む）
		constexpr int kSize = YGpuParticle::kGpuSizeCurveLutSize;
		for (int i = 0; i < kSize; ++i) {
			const float v = src.curve.Evaluate(static_cast<float>(i) / (kSize - 1));
			Vector4& packed = dst.sizeCurveLut[i / 4];
			switch (i % 4) {
			case 0: packed.x = v; break;
			case 1: packed.y = v; break;
			case 2: packed.z = v; break;
			default: packed.w = v; break;
			}
		}
	}

#ifdef USE_IMGUI
	bool DrawImGui(SizeCurveParams& params)
	{
		const bool changed = params.curve.DrawEditor("開始スケールに掛ける倍率", 0.0f, 3.0f, ImVec2(0, 120));
		ImGui::TextDisabled("有効な間は「スケール」の終了スケールは使われません（開始スケール × この倍率）");
		return changed;
	}
#endif

} // namespace YSizeCurveModule

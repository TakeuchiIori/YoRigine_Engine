#pragma once
// ===========================================================================
// YColorGradientModule
//
// 拡張Paramモジュール（任意ON/OFF）。粒子の色を寿命に沿ったグラデーションで決める。
//
// なぜ必要か: GPU 粒子の色は「開始色 → 終了色」の 2 色補間しかできず、
// 炎（白 → 黄 → 橙 → 赤 → 透明）のような多段の色変化が作れなかった。
// CPU パーティクルと同じ ColorGradient / 同じグラデーション UI を使うので、
// 作り方も見た目も CPU 側と揃う。
//
// GPU側は VS が ParticleExtParams(b1) の colorGradientLut を寿命で引き、
// start/end の 2 色補間を置き換える。表は ColorGradient（Oklab 補間）を
// kGpuColorGradientLutSize 段に等間隔サンプルしたもの。
// ※ 有効時は「開始色 / 終了色」とそのばらつきは使われない。
// ===========================================================================
#include "Particle/ColorGradient.h"
#include <GPUParticle/YGpuParticle.h>

struct ColorGradientParams {
  bool isEnable = false;
  ColorGradient gradient;
};

namespace YColorGradientModule {

void WriteTo(YGpuParticle::ParticleExtParameters &dst,
             const ColorGradientParams &src);

#ifdef USE_IMGUI
bool DrawImGui(ColorGradientParams &params);
#endif

} // namespace YColorGradientModule

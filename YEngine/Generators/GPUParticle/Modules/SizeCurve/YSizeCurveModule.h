#pragma once
// ===========================================================================
// YSizeCurveModule
//
// 拡張Paramモジュール（任意ON/OFF）。粒子のサイズを寿命に沿ったカーブで決める。
//
// なぜ必要か: GPU 粒子のサイズは「開始スケール →
// 終了スケール」の直線補間しかできず、
// 「一瞬で膨らんでからゆっくり縮む」「途中で一度ふくらむ」といった変化が作れなかった。
// CPU パーティクルの「寿命でサイズ変化」と同じ ParticleCurve / 同じカーブ UI
// を使う。
//
// GPU側は VS が ParticleExtParams(b1) の sizeCurveLut を寿命で引き、
// 「開始スケール × カーブの倍率」で scale を決める（終了スケールは使わない）。
// 表は kGpuSizeCurveLutSize 段を float4 に 4 つずつ詰めて送る。
// ===========================================================================
#include "Particle/ParticleCurve.h"
#include <GPUParticle/YGpuParticle.h>

struct SizeCurveParams {
  bool isEnable = false;
  ParticleCurve curve; // 寿命 [0,1] → 開始スケールに掛ける倍率
};

namespace YSizeCurveModule {

void WriteTo(YGpuParticle::ParticleExtParameters &dst,
             const SizeCurveParams &src);

#ifdef USE_IMGUI
bool DrawImGui(SizeCurveParams &params);
#endif

} // namespace YSizeCurveModule

#pragma once
#include "../../../ColorGradient.h"
#include "../../../YParticleModuleFactory.h"
#include "../../IParticleModule.h"

/// <summary>
/// 寿命に応じたカラー & アルファ変化（ストップ数無制限のグラデーション）
/// 用途: 炎（黄→オレンジ→赤→透明）、魔法（白→青→消滅）
///
/// 既定はグラデーションの色で上書きする。「生成時の色を掛ける」を有効にすると
/// 生成時の色（カラー範囲・ランダムカラー）× グラデーション
/// になり、色のばらつきを活かせる。
/// </summary>
class UpdateColorOverLifetime : public IUpdateModule {
public:
  UpdateColorOverLifetime();
  void OnUpdate(ParticleAttribute *attrs, uint32_t index, float dt) override;
  void DrawEditor() override;
  std::string GetName() const override { return "寿命でカラー変化"; }
  std::string GetTypeName() const override { return "UpdateColorOverLifetime"; }
  const ColorGradient *GetPreviewGradient() const override {
    return &gradient_;
  }
  ColorWrite GetColorWrite() const override {
    return multiplyInitialColor_ ? ColorWrite::Multiply : ColorWrite::Overwrite;
  }
  void SaveToJson(nlohmann::json &json) const override;
  void LoadFromJson(const nlohmann::json &json) override;

protected:
  ColorGradient gradient_;
  bool multiplyInitialColor_ = false; // true: 生成時の色 × グラデーション
};

REGISTER_UPDATE_MODULE(UpdateColorOverLifetime)

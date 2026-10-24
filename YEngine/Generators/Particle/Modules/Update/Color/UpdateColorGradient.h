#pragma once
#include "../../../ColorGradient.h"
#include "../../../YParticleModuleFactory.h"
#include "../../IParticleModule.h"

/// <summary>
/// 複数色を使ったグラデーション
/// 用途: レインボーエフェクト、複雑な色変化など
/// ※ 中身は UpdateColorOverLifetime と同じ ColorGradient。旧データ（等間隔
/// 最大5色）互換のため型名を残している
/// </summary>
class UpdateColorGradient : public IUpdateModule {
public:
  UpdateColorGradient();
  void OnUpdate(ParticleAttribute *attrs, uint32_t index, float dt) override;
  void DrawEditor() override;
  std::string GetName() const override { return "グラデーション"; }
  std::string GetTypeName() const override { return "UpdateColorGradient"; }
  const ColorGradient* GetPreviewGradient() const override { return &gradient_; }

  void SaveToJson(nlohmann::json &json) const override;
  void LoadFromJson(const nlohmann::json &json) override;

private:
  ColorGradient gradient_;
};

REGISTER_UPDATE_MODULE(UpdateColorGradient)

#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include "../../ModuleSummary.h"

/// <summary>
/// 空気抵抗・摩擦を追加
/// 用途: 減速エフェクト、煙、霧など
/// </summary>
class UpdateDrag : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "ドラッグ"; }
    std::string GetTypeName() const override { return "UpdateDrag"; }
    std::string GetSummary()  const override { return FormatSummary("係数 %.2f", dragCoefficient_); }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    float dragCoefficient_ = 0.5f; // 抵抗係数
};

REGISTER_UPDATE_MODULE(UpdateDrag)

#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include "../../ModuleSummary.h"


class UpdateVelocity : public IUpdateModule
{
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "速度で移動"; }
    std::string  GetTypeName() const override { return "UpdateVelocity"; }
    std::string GetSummary()  const override { return isRandom_ ? std::string("生成時の速度") : FormatSummary("固定 (%.1f, %.1f, %.1f)", velocity_.x, velocity_.y, velocity_.z); }
    bool OverwritesVelocity() const override { return !isRandom_; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
	Vector3 velocity_{ 0.0f, 1.0f, 0.0f };
	bool isRandom_ = true; // 新規追加時は生成時の速度を使う（固定速度だと他の速度系が効かないため）
};

// ファクトリーに登録
REGISTER_UPDATE_MODULE(UpdateVelocity)
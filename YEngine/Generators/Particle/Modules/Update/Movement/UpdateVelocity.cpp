#include "UpdateVelocity.h"

void UpdateVelocity::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{        
	// 速度に基づいて座標を動かすだけの
	if (!isRandom_) {
		attrs[index].velocity = velocity_;
	}
	attrs[index].position += attrs[index].velocity * dt;
}

void UpdateVelocity::DrawEditor()
{
#ifdef USE_IMGUI
	// isRandom_ は「生成時の速度（と重力などで変わった速度）をそのまま使う」の意味。
	// OFF だと毎フレーム下の固定速度で上書きするため、他の速度系モジュールが効かなくなる。
	ImGui::Checkbox("生成時の速度を使う", &isRandom_);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::SetTooltip("ON : 生成時の速度・重力・乱流などで決まった速度で動く（通常はこちら）\n"
		                  "OFF: 毎フレーム下の固定速度で上書きして動く（他の速度系モジュールは効かない）");
	}
	ImGui::BeginDisabled(isRandom_);
	ImGui::DragFloat3("固定速度", &velocity_.x, 0.1f);
	ImGui::EndDisabled();
#endif
}

void UpdateVelocity::SaveToJson(nlohmann::json& json) const
{
	json = {
		{"isRandom", isRandom_},
		{"velocity", {
			{"x", velocity_.x},
			{"y", velocity_.y},
			{"z", velocity_.z}
		}}
	};
}

void UpdateVelocity::LoadFromJson(const nlohmann::json& json)
{
	if (json.contains("isRandom")) {
		isRandom_ = json["isRandom"];
	}
	if (json.contains("velocity")) {
		velocity_.x = json["velocity"]["x"];
		velocity_.y = json["velocity"]["y"];
		velocity_.z = json["velocity"]["z"];
	}
}

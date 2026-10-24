#include "YParticleModuleFactory.h"

//=================================================================
// 全パーティクルモジュールの登録を保証する翻訳単位
//
// REGISTER_*_MODULE は各モジュール .cpp 内の static 変数で登録する仕組みだが、
// YEngine は静的ライブラリなので「どこからも参照されない .obj」はリンカーに捨てられ、
// 登録ごと消える（JSON に書いたモジュールが読み込み時に黙って欠落していた）。
// ここで全ヘッダを include して各モジュール型への参照を作り、確実にリンクさせる。
//
// ★ 新しいモジュールを作ったら、ここに include を1行追加すること。
//=================================================================

// Spawn
#include "Modules/Spawn/Color/SpawnColorRange.h"
#include "Modules/Spawn/Color/SpawnRandomColor.h"
#include "Modules/Spawn/Life/SpawnBurst.h"
#include "Modules/Spawn/Life/SpawnLifeTime.h"
#include "Modules/Spawn/Movement/SpawnAngularVelocity.h"
#include "Modules/Spawn/Movement/SpawnConeVelocity.h"
#include "Modules/Spawn/Movement/SpawnDirection.h"
#include "Modules/Spawn/Movement/SpawnRadialVelocity.h"
#include "Modules/Spawn/Movement/SpawnRing.h"
#include "Modules/Spawn/Movement/SpawnTangentVelocity.h"
#include "Modules/Spawn/Movement/SpawnVelocity.h"
#include "Modules/Spawn/Transform/SpawnPhase.h"
#include "Modules/Spawn/Transform/SpawnRandomRotate.h"
#include "Modules/Spawn/Transform/SpawnRandomScale.h"
#include "Modules/Spawn/Transform/SpawnSize.h"
#include "Modules/Spawn/UV/SpawnSubTexture.h"
#include "Modules/Spawn/UV/SpawnUV.h"

// Update
#include "Modules/Update/Animation/UpdateFlipbook.h"
#include "Modules/Update/Animation/UpdatePulse.h"
#include "Modules/Update/Color/UpdateColor.h"
#include "Modules/Update/Color/UpdateColorBySpeed.h"
#include "Modules/Update/Color/UpdateColorGradient.h"
#include "Modules/Update/Color/UpdateColorOverLifetime.h"
#include "Modules/Update/Color/UpdateFadeOut.h"
#include "Modules/Update/Emit/UpdateSubEmitter.h"
#include "Modules/Update/Movement/UpdateFollowTarget.h"
#include "Modules/Update/Movement/UpdateOrbital.h"
#include "Modules/Update/Movement/UpdateSinMovement.h"
#include "Modules/Update/Movement/UpdateSpiralConverge.h"
#include "Modules/Update/Movement/UpdateVelocity.h"
#include "Modules/Update/Physics/UpdateAttractor.h"
#include "Modules/Update/Physics/UpdateBounce.h"
#include "Modules/Update/Physics/UpdateCollision.h"
#include "Modules/Update/Physics/UpdateDrag.h"
#include "Modules/Update/Physics/UpdateForceField.h"
#include "Modules/Update/Physics/UpdateGravity.h"
#include "Modules/Update/Physics/UpdateSpeedClamp.h"
#include "Modules/Update/Physics/UpdateTurbulence.h"
#include "Modules/Update/Physics/UpdateVortex.h"
#include "Modules/Update/Physics/UpdateWind.h"
#include "Modules/Update/Random/UpdateNoise.h"
#include "Modules/Update/Transform/UpdateAngularVelocity.h"
#include "Modules/Update/Transform/UpdateLimitByDistance.h"
#include "Modules/Update/Transform/UpdateRotation.h"
#include "Modules/Update/Transform/UpdateScale.h"
#include "Modules/Update/Transform/UpdateSizeOverLifetime.h"
#include "Modules/Update/Transform/UpdateStretchByVelocity.h"
#include "Modules/Update/UV/UpdateUVFlicker.h"
#include "Modules/Update/UV/UpdateUVScroll.h"

// UV 系はヘッダではなく .cpp 側で登録しているため、ここでも登録して型への参照を作る
REGISTER_UPDATE_MODULE(UpdateUVFlicker)
REGISTER_UPDATE_MODULE(UpdateUVScroll)

void YParticleModuleFactory::EnsureAllModulesLinked() {
	// 中身は不要。この関数を呼ぶことでこの .obj 自体がリンク対象になり、上の登録が実行される
}

//=================================================================
// エディタ用カテゴリ表（★ 新モジュールはここにも 1 行足す。無ければ「その他」扱い）
//=================================================================

#include <unordered_map>

ParticleModuleCategory YParticleModuleFactory::GetCategory(const std::string& typeName) {
	using C = ParticleModuleCategory;
	static const std::unordered_map<std::string, C> table = {
		// 発生
		{ "SpawnLifeTime", C::Emission }, { "SpawnBurst", C::Emission },
		// 色
		{ "SpawnColorRange", C::Color }, { "SpawnRandomColor", C::Color },
		{ "UpdateColor", C::Color }, { "UpdateColorBySpeed", C::Color }, { "UpdateColorGradient", C::Color },
		{ "UpdateColorOverLifetime", C::Color }, { "UpdateFadeOut", C::Color },
		// サイズ・回転
		{ "SpawnRandomScale", C::Transform }, { "SpawnSize", C::Transform }, { "SpawnRandomRotate", C::Transform },
		{ "SpawnAngularVelocity", C::Transform }, { "UpdateAngularVelocity", C::Transform },
		{ "UpdateRotation", C::Transform }, { "UpdateScale", C::Transform }, { "UpdateSizeOverLifetime", C::Transform },
		{ "UpdateStretchByVelocity", C::Transform }, { "UpdatePulse", C::Transform },
		// 動き
		{ "SpawnVelocity", C::Motion }, { "SpawnConeVelocity", C::Motion }, { "SpawnDirection", C::Motion },
		{ "SpawnRadialVelocity", C::Motion }, { "SpawnRing", C::Motion }, { "SpawnTangentVelocity", C::Motion },
		{ "UpdateVelocity", C::Motion }, { "UpdateFollowTarget", C::Motion }, { "UpdateOrbital", C::Motion },
		{ "UpdateSinMovement", C::Motion }, { "UpdateSpiralConverge", C::Motion }, { "UpdateNoise", C::Motion },
		{ "UpdateLimitByDistance", C::Motion },
		// 力場・物理
		{ "UpdateGravity", C::Force }, { "UpdateDrag", C::Force }, { "UpdateWind", C::Force },
		{ "UpdateTurbulence", C::Force }, { "UpdateVortex", C::Force }, { "UpdateAttractor", C::Force },
		{ "UpdateForceField", C::Force }, { "UpdateBounce", C::Force }, { "UpdateCollision", C::Force },
		{ "UpdateSpeedClamp", C::Force },
		// 見た目
		{ "SpawnUV", C::Look }, { "SpawnSubTexture", C::Look }, { "UpdateUVScroll", C::Look },
		{ "UpdateUVFlicker", C::Look }, { "UpdateFlipbook", C::Look },
		// 子エフェクト
		{ "UpdateSubEmitter", C::Child },
	};
	auto it = table.find(typeName);
	return (it != table.end()) ? it->second : C::Other;
}

const char* YParticleModuleFactory::GetCategoryName(ParticleModuleCategory category) {
	switch (category) {
	case ParticleModuleCategory::Emission:  return "発生";
	case ParticleModuleCategory::Color:     return "色";
	case ParticleModuleCategory::Transform: return "サイズ・回転";
	case ParticleModuleCategory::Motion:    return "動き";
	case ParticleModuleCategory::Force:     return "力場・物理";
	case ParticleModuleCategory::Look:      return "見た目";
	case ParticleModuleCategory::Child:     return "子エフェクト";
	default:                                return "その他";
	}
}

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
// エディタ用のモジュール情報表（カテゴリ / 一行説明 / 旧モジュールの置き換え先）
// ★ 新モジュールはここにも 1 行足す。無いと「その他」・説明なしで追加メニューに出る。
//=================================================================

#include <unordered_map>

namespace {

	struct ModuleInfo {
		ParticleModuleCategory category;
		const char* description;         // 追加メニューとインスペクタに出す一行説明
		const char* replacement = nullptr; // 旧モジュールなら推奨の置き換え先（追加メニューには出さない）
	};

	const std::unordered_map<std::string, ModuleInfo>& GetInfoTable() {
		using C = ParticleModuleCategory;
		static const std::unordered_map<std::string, ModuleInfo> table = {
			// ── 発生 ──
			{ "SpawnLifeTime",  { C::Emission, "生存時間を最小〜最大でランダムに決める（バラバラに消える）" } },
			{ "SpawnBurst",     { C::Emission, "寿命をバースト向けに短くする（0.1〜1 秒程度。爆発・火花）" } },

			// ── 色 ──
			{ "SpawnColorRange",   { C::Color, "2 色の間でランダムな色にする（色の系統を保ったままばらつく）" } },
			{ "SpawnRandomColor",  { C::Color, "RGBA を各チャンネル独立にランダムにする（カラフルな火花など）" } },
			{ "UpdateColorOverLifetime", { C::Color, "寿命に沿ってグラデーションで色とアルファを変える（炎・魔法の基本）" } },
			{ "UpdateColorGradient", { C::Color, "旧「グラデーション」。中身は「寿命でカラー変化」と同じ", "UpdateColorOverLifetime" } },
			{ "UpdateColor",       { C::Color, "R/G/B/A を個別のカーブで変える（2 色だけなら「寿命でカラー変化」が簡単）" } },
			{ "UpdateColorBySpeed", { C::Color, "速さで色を変える（速い = 熱い色 / 遅い = 冷めた色）" } },
			{ "UpdateFadeOut",     { C::Color, "寿命の後半でアルファだけを 1 → 0 にする（自然に消える）" } },

			// ── サイズ・回転 ──
			{ "SpawnRandomScale",     { C::Transform, "生成時のスケールをランダムにする（XYZ 個別）" } },
			{ "SpawnSize",            { C::Transform, "生成時のサイズをランダムにする（XYZ 同じ値）" } },
			{ "SpawnRandomRotate",    { C::Transform, "生成時の向きをランダムにする" } },
			{ "SpawnAngularVelocity", { C::Transform, "生成時にランダムな回転速度を与える（破片・葉・コイン）" } },
			{ "UpdateAngularVelocity",{ C::Transform, "回転速度を毎フレーム向きに積算する（スピン）" } },
			{ "UpdateRotation",       { C::Transform, "一定の速さで回転させる（竜巻・渦）" } },
			{ "UpdateScale",          { C::Transform, "時間でスケールを変える（成長 / 収縮）" } },
			{ "UpdateSizeOverLifetime", { C::Transform, "寿命に沿ってカーブでサイズを変える（広がって消える、の基本）" } },
			{ "UpdateStretchByVelocity",{ C::Transform, "速度の向きに引き伸ばす（火花・流星・スピード線）" } },
			{ "UpdatePulse",          { C::Transform, "スケールを脈打たせる（ヒット・鼓動）" } },

			// ── 動き ──
			{ "SpawnVelocity",        { C::Motion, "生成時の速度を最小〜最大の範囲でランダムに決める" } },
			{ "SpawnConeVelocity",    { C::Motion, "円錐状に飛ばす（噴射・スパーク）" } },
			{ "SpawnDirection",       { C::Motion, "指定方向＋拡散角で飛ばす（斬撃の飛沫など）" } },
			{ "SpawnRadialVelocity",  { C::Motion, "中心から外へ放射状に飛ばす（爆発・衝撃波）" } },
			{ "SpawnRing",            { C::Motion, "XZ 平面のリング状に配置して外へ飛ばす（着地の衝撃波）" } },
			{ "SpawnTangentVelocity", { C::Motion, "球面の接線方向に飛ばす（渦を巻く爆発）" } },
			{ "UpdateVelocity",       { C::Motion, "速度で位置を動かす（★ これが無いと速度系のモジュールを入れても粒は動かない）" } },
			{ "UpdateFollowTarget",   { C::Motion, "目標位置へ向かって加速する（ホーミング・吸収）" } },
			{ "UpdateOrbital",        { C::Motion, "円軌道で周回させる（オーブ・衛星）" } },
			{ "UpdateSinMovement",    { C::Motion, "サイン波で揺らす（ふわふわ浮遊。「位相」と組み合わせる）" } },
			{ "UpdateSpiralConverge", { C::Motion, "螺旋を描いて中心へ収束させる（チャージ・溜め）" } },
			{ "UpdateNoise",          { C::Motion, "ランダムなノイズで揺らぐ（炎の揺らぎ）" } },
			{ "UpdateLimitByDistance",{ C::Motion, "生成位置から一定距離を超えた粒を消す（飛びすぎ防止）" } },

			// ── 力場・物理 ──
			{ "UpdateGravity",    { C::Force, "重力（一定の加速度）を加える" } },
			{ "UpdateDrag",       { C::Force, "空気抵抗で減速させる（煙・霧・失速する破片）" } },
			{ "UpdateWind",       { C::Force, "一定方向の風を加える（雨・雪・砂塵）" } },
			{ "UpdateTurbulence", { C::Force, "3D の乱流で速度を乱す（煙・霧・魔法の有機的な揺れ）" } },
			{ "UpdateVortex",     { C::Force, "軸の周りの渦に乗せる（竜巻・水の渦・魔法陣）" } },
			{ "UpdateAttractor",  { C::Force, "一点へ引き寄せる（吸収・ブラックホール）" } },
			{ "UpdateForceField", { C::Force, "一点からの引力 / 斥力（磁石・衝撃）" } },
			{ "UpdateBounce",     { C::Force, "水平な床で跳ねる（破片・水滴）" } },
			{ "UpdateCollision",  { C::Force, "平面との衝突で反射する" } },
			{ "UpdateSpeedClamp", { C::Force, "速さの最大 / 最小を制限する（飛びすぎ・止まりすぎ防止）" } },

			// ── 見た目 ──
			{ "SpawnUV",          { C::Look, "生成時の UV（オフセット / スケール）を設定する" } },
			{ "SpawnSubTexture",  { C::Look, "スプライトシートの初期コマをランダムにする（フリップブックと併用）" } },
			{ "UpdateFlipbook",   { C::Look, "スプライトシートをコマ送りする（爆発・炎のアニメ）" } },
			{ "UpdateUVScroll",   { C::Look, "UV を流す（炎・溶岩・エネルギー）" } },
			{ "UpdateUVFlicker",  { C::Look, "UV をちらつかせる（炎・稲妻テクスチャの揺れ）" } },

			// ── 子エフェクト ──
			{ "UpdateSubEmitter", { C::Child, "粒の死亡時などに別のシステムを発生させる（花火の二段咲き）" } },

			// ── その他 ──
			{ "SpawnPhase", { C::Other, "揺れ系モジュールの位相を粒ごとにずらす（ふわふわが揃わないように）" } },
		};
		return table;
	}

	const ModuleInfo* FindInfo(const std::string& typeName) {
		const auto& table = GetInfoTable();
		auto it = table.find(typeName);
		return (it != table.end()) ? &it->second : nullptr;
	}

} // namespace

ParticleModuleCategory YParticleModuleFactory::GetCategory(const std::string& typeName) {
	const ModuleInfo* info = FindInfo(typeName);
	return info ? info->category : ParticleModuleCategory::Other;
}

const char* YParticleModuleFactory::GetDescription(const std::string& typeName) {
	const ModuleInfo* info = FindInfo(typeName);
	return info ? info->description : "";
}

const char* YParticleModuleFactory::GetReplacement(const std::string& typeName) {
	const ModuleInfo* info = FindInfo(typeName);
	return info ? info->replacement : nullptr;
}

bool YParticleModuleFactory::UsesVelocity(const std::string& typeName) {
	// 速度だけを書き換えるモジュール。位置へ反映するのは UpdateVelocity（速度で移動）だけなので、
	// それが無い / 固定速度で上書きしていると、これらは見た目に何も起きない。
	// （周回・螺旋収束・サイン波は位置を直接動かすので含めない）
	static const std::unordered_map<std::string, bool> table = {
		{ "SpawnVelocity", true }, { "SpawnConeVelocity", true }, { "SpawnDirection", true },
		{ "SpawnRadialVelocity", true }, { "SpawnRing", true }, { "SpawnTangentVelocity", true },
		{ "UpdateGravity", true }, { "UpdateDrag", true }, { "UpdateWind", true }, { "UpdateTurbulence", true },
		{ "UpdateVortex", true }, { "UpdateAttractor", true }, { "UpdateForceField", true },
		{ "UpdateFollowTarget", true }, { "UpdateNoise", true }, { "UpdateSpeedClamp", true },
	};
	return table.count(typeName) > 0;
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

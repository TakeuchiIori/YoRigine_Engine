#pragma once
#include <cstdint>
#include <string>
#include "../ParticleMath.h"
#include "ParticleAttribute.h"
#include "Loaders/Json/ISerializable.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

struct ColorGradient;

// 共通の基底
class IParticleModule : public ISerializable {
public:
    virtual ~IParticleModule() = default;
    virtual void Initialize([[maybe_unused]] uint32_t maxParticles) {}
    virtual void DrawEditor() = 0;
    virtual std::string GetName() const = 0;

    /// <summary>
    /// エディタのモジュール一覧（スタック）に出す一行要約。例: "0.5〜2.0 秒"。
    /// 空文字なら何も出さない（既定）。
    /// </summary>
    virtual std::string GetSummary() const { return {}; }

    /// <summary>
    /// 一覧の行に色帯を出したいモジュール（寿命カラー等）はグラデーションを返す
    /// </summary>
    virtual const ColorGradient* GetPreviewGradient() const { return nullptr; }

    /// <summary>
    /// 粒子の色への書き込み方。エディタが「後ろのモジュールに上書きされて効いていない」
    /// 組み合わせ（例: 生成時の色 → 寿命カラーで上書き）を警告するために使う。
    /// </summary>
    enum class ColorWrite {
        None,      // 色に触らない
        Overwrite, // RGBA を丸ごと上書きする（前の色は消える）
        AlphaOnly, // アルファだけを上書きする
        Multiply,  // 今の色（生成時の色）に掛け合わせる（前の色を活かす）
    };
    virtual ColorWrite GetColorWrite() const { return ColorWrite::None; }

    /// <summary>
    /// 毎フレーム速度を固定値で上書きするか（UpdateVelocity の固定速度モード）。
    /// true のとき、他の速度系モジュール（生成時の速度・重力・乱流…）は効かなくなる。
    /// </summary>
    virtual bool OverwritesVelocity() const { return false; }

    /// <summary>
    /// 無効なモジュールはシステムが丸ごと飛ばす（削除せずに効果を比較するため）。
    /// JSON にはモジュール共通の "enabled" として保存される。
    /// </summary>
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    /// <summary>
    /// 表示名。作者が付けた名前（JSON の "name"。例: 「爆発初速」）があればそれ、無ければ GetName()。
    /// 以前は保存のたびに GetName() で上書きされ、付けた名前が消えていた。
    /// </summary>
    std::string GetDisplayName() const { return label_.empty() ? GetName() : label_; }
    // 既定の名前と同じなら空にしておく（既定名が変わったときに追従させるため）
    void SetLabel(const std::string& label) { label_ = (label == GetName()) ? std::string{} : label; }

private:
    bool enabled_ = true;
    std::string label_;
};

// --- 生成タイミング専用 ---
class ISpawnModule : public IParticleModule {
public:
    virtual void OnSpawn(ParticleAttribute* attrs, uint32_t index) = 0;

    /// <summary>
    /// この生成モジュールが粒に設定しうる「最大生存時間(秒)」のヒント。
    /// エフェクトの再生尺（＝最後の粒が消えるまで）の見積りに使う。
    /// 寿命に関与しないモジュールは 0（既定）を返す。
    /// </summary>
    virtual float GetMaxLifetimeHint() const { return 0.0f; }
};

// --- 更新タイミング専用 ---
class IUpdateModule : public IParticleModule {
public:
    /// <summary>
    /// 毎フレーム、粒子ごとの OnUpdate より前に 1 回だけ呼ばれる（デフォルトは何もしない）。
    /// モジュール全体で共有する時間などは OnUpdate ではなくここで進めること
    /// （OnUpdate で進めると粒子の数だけ進んでしまう）。
    /// </summary>
    virtual void OnFrameBegin([[maybe_unused]] float dt) {}

    virtual void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) = 0;

    /// <summary>
    /// 粒が寿命で死ぬ瞬間に1回だけ呼ばれる（デフォルトは何もしない）。
    /// サブエミッタ（死亡時に子エフェクトを出す等）で使用する。
    /// </summary>
    virtual void OnDeath([[maybe_unused]] ParticleAttribute* attrs,
                         [[maybe_unused]] uint32_t index) {}
};

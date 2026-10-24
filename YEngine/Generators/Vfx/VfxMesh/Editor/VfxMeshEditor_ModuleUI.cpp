#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include "Core/Editor/Widgets/YEditorWidget.h"

namespace YoRigine {

    namespace {
        void AddWaypointBeamModule(std::vector<VfxModule>& modules)
        {
            VfxModule beamPulse;
            beamPulse.type = VfxModuleType::BeamPulse;
            beamPulse.target = VfxModuleTarget::LightVolume;
            beamPulse.amplitude = 0.18f;
            beamPulse.frequency = 1.4f;
            modules.push_back(beamPulse);

            VfxModule flicker;
            flicker.type = VfxModuleType::Flicker;
            flicker.target = VfxModuleTarget::LightVolume;
            flicker.amplitude = 0.12f;
            flicker.frequency = 9.0f;
            modules.push_back(flicker);
        }

        // コンボ・一覧で使う種類名（VfxModuleType の並びと一致させる）
        const char* kTypeNames[] = {
            "Lifetime (エフェクト寿命)", "Move (等速移動)", "Rise (上昇)", "ScalePulse (スケール脈動)",
            "ScaleOverLife (スケール変化)", "ColorOverLife (色変化)", "FadeInOut (フェード)",
            "Accelerate (加速/重力)", "Orbit (周回)", "Shake (揺れ)",
            "Visibility (表示期間)", "Flicker (明滅)", "BeamPulse (ビーム脈動)"
        };
        // 追加メニューに出す一行説明（同じ並び）
        const char* kTypeDescriptions[] = {
            "エフェクト全体のワンショット寿命を決める（動きは無し）",
            "決まった速度で基準位置を動かす",
            "上へ昇らせる（煙・オーラ）",
            "sin でふくらんだり縮んだりさせる（鼓動）",
            "寿命に沿ってスケールを変える（破裂して広がる等）",
            "寿命に沿って色を掛け合わせる（HDR で Bloom 強化も）",
            "出始めと終わりをフェードさせる",
            "初速と加速度で動かす（重力・打ち上げ）",
            "軸の周りを周回させる",
            "細かく揺らす（衝撃・不安定な光）",
            "指定の時間だけ表示する（演出の順番付け）",
            "不透明度をランダムに明滅させる（電撃・故障）",
            "光ボリュームのビームを脈打たせる",
        };
        static_assert(IM_ARRAYSIZE(kTypeNames) == IM_ARRAYSIZE(kTypeDescriptions), "モジュール名と説明の数を揃えること");

        const char* kEaseNames[] = {
            "Linear", "EaseIn (ゆっくり開始)", "EaseOut (ゆっくり終了)", "EaseInOut (両端)",
            "EaseOutCubic (強めブレーキ)", "EaseOutExpo (爆発向き)", "EaseOutBack (ポップ)"
        };
        const char* kTargetNames[] = {
            "All (全効果)", "NoiseVolume", "LightningBolt", "ShockwaveRing", "LightVolume"
        };
    }

    int VfxModuleTypeCount() { return IM_ARRAYSIZE(kTypeNames); }

    const char* VfxModuleTypeLabel(VfxModuleType type)
    {
        const int i = static_cast<int>(type);
        return (i >= 0 && i < IM_ARRAYSIZE(kTypeNames)) ? kTypeNames[i] : "?";
    }

    const char* VfxModuleTypeDescription(VfxModuleType type)
    {
        const int i = static_cast<int>(type);
        return (i >= 0 && i < IM_ARRAYSIZE(kTypeDescriptions)) ? kTypeDescriptions[i] : "";
    }

    const char* VfxModuleTargetLabel(VfxModuleTarget target)
    {
        const int i = static_cast<int>(target);
        return (i >= 0 && i < IM_ARRAYSIZE(kTargetNames)) ? kTargetNames[i] : "?";
    }

    // モジュール 1 個分の編集 UI（変更があれば true。履歴への記録は呼び出し側）
    bool VfxMeshEditor::DrawModuleBody(VfxModule& m, bool showTarget)
    {
        bool c = false;

        int typeIdx = static_cast<int>(m.type);
        ImGui::SetNextItemWidth(220);
        if (ImGui::Combo("種類##mt", &typeIdx, kTypeNames, IM_ARRAYSIZE(kTypeNames))) {
            m.type = static_cast<VfxModuleType>(typeIdx);
            c = true;
        }
        ImGui::TextDisabled("%s", VfxModuleTypeDescription(m.type));

        if (showTarget && m.type != VfxModuleType::Lifetime) {
            int tgtIdx = static_cast<int>(m.target);
            ImGui::SetNextItemWidth(220);
            if (ImGui::Combo("対象##mtg", &tgtIdx, kTargetNames, IM_ARRAYSIZE(kTargetNames))) {
                m.target = static_cast<VfxModuleTarget>(tgtIdx);
                c = true;
            }
        }

        if (m.type != VfxModuleType::Lifetime) {
            ImGui::SetNextItemWidth(90);
            c |= ImGui::DragFloat("開始遅延(秒)##mst", &m.startTime, 0.02f, 0.0f, 30.0f, "%.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            c |= ImGui::DragFloat("持続時間(秒)##mwin", &m.window, 0.02f, 0.0f, 30.0f, "%.2f");
            YEditorWidget::ItemTooltip(
                "このモジュールが効く長さ。\n"
                "0 = 無限（Scale/Color はエフェクト寿命いっぱいで 0→1）\n"
                ">0 = この秒数で完了（移動系はここで停止）");
        }

        if (m.type == VfxModuleType::ScaleOverLife || m.type == VfxModuleType::ColorOverLife) {
            int easeIdx = static_cast<int>(m.ease);
            ImGui::SetNextItemWidth(220);
            if (ImGui::Combo("カーブ##mease", &easeIdx, kEaseNames, IM_ARRAYSIZE(kEaseNames))) {
                m.ease = static_cast<VfxEase>(easeIdx);
                c = true;
            }
        }

        switch (m.type) {
        case VfxModuleType::Lifetime:
            c |= YEditorWidget::DragFloat("エフェクト寿命(秒)##md", m.duration, 0.02f, 0.05f, 10.0f, "%.2f");
            ImGui::TextDisabled("  ※各モジュールの「持続時間=0」はこの寿命いっぱいで 0→1 に進む。");
            break;
        case VfxModuleType::Move:
            c |= YEditorWidget::DragVec3("速度(/秒)##mv", m.velocity, 0.02f, -20.0f, 20.0f, "%.2f");
            break;
        case VfxModuleType::Rise:
            c |= YEditorWidget::DragFloat("上昇速度##mr", m.velocity.y, 0.02f, -20.0f, 20.0f, "%.2f");
            c |= YEditorWidget::DragFloat("係数##ma",     m.amplitude, 0.02f, 0.0f, 10.0f, "%.2f");
            break;
        case VfxModuleType::ScalePulse:
            c |= YEditorWidget::DragFloat("振幅##mp",      m.amplitude, 0.01f, 0.0f, 2.0f, "%.2f");
            c |= YEditorWidget::DragFloat("周波数(Hz)##mf", m.frequency, 0.05f, 0.0f, 20.0f, "%.2f");
            break;
        case VfxModuleType::BeamPulse:
            c |= YEditorWidget::DragFloat("ビーム振幅##mbpa",   m.amplitude, 0.01f, 0.0f, 2.0f, "%.2f");
            c |= YEditorWidget::DragFloat("脈動回数(/秒)##mbpf", m.frequency, 0.05f, 0.0f, 20.0f, "%.2f");
            ImGui::TextDisabled("  ※LightVolume の beamRadius / beamGlow を揺らします");
            break;
        case VfxModuleType::ScaleOverLife:
            c |= YEditorWidget::DragFloat("開始スケール##mss", m.scaleStart, 0.01f, 0.0f, 20.0f, "%.2f");
            c |= YEditorWidget::DragFloat("終了スケール##mse", m.scaleEnd,   0.01f, 0.0f, 20.0f, "%.2f");
            ImGui::TextDisabled("  ※元サイズへの倍率。0→1.5 で「破裂して広がる」");
            break;
        case VfxModuleType::ColorOverLife:
            c |= YEditorWidget::ColorHDR("開始色(乗算)##mcs", m.colorStart);
            c |= YEditorWidget::ColorHDR("終了色(乗算)##mce", m.colorEnd);
            ImGui::TextDisabled("  ※元の色に掛ける倍率。白(1,1,1,1)=変化なし / >1 で Bloom 強化");
            break;
        case VfxModuleType::FadeInOut:
            c |= YEditorWidget::DragFloat("フェードイン(秒)##mfi",  m.fadeIn,  0.01f, 0.0f, 10.0f, "%.2f");
            c |= YEditorWidget::DragFloat("フェードアウト(秒)##mfo", m.fadeOut, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::TextDisabled("  ※アウトは効果時間（0ならワンショット寿命）の終端から逆算");
            break;
        case VfxModuleType::Accelerate:
            c |= YEditorWidget::DragVec3("初速(/秒)##mav",   m.velocity,     0.02f, -50.0f, 50.0f, "%.2f");
            c |= YEditorWidget::DragVec3("加速度(/秒²)##maa", m.acceleration, 0.02f, -50.0f, 50.0f, "%.2f");
            ImGui::TextDisabled("  ※重力なら加速度 Y=-9.8。打ち上げは初速+Y & 加速度-Y");
            break;
        case VfxModuleType::Orbit:
            c |= YEditorWidget::DragVec3("回転軸##mox", m.velocity, 0.02f, -1.0f, 1.0f, "%.2f");
            c |= YEditorWidget::DragFloat("半径##mor",        m.amplitude, 0.02f, 0.0f, 20.0f, "%.2f");
            c |= YEditorWidget::DragFloat("回転数(/秒)##mof", m.frequency, 0.05f, -10.0f, 10.0f, "%.2f");
            break;
        case VfxModuleType::Shake:
            c |= YEditorWidget::DragFloat("振れ幅##msa", m.amplitude, 0.01f, 0.0f, 5.0f, "%.2f");
            c |= YEditorWidget::DragFloat("速さ(Hz)##msf", m.frequency, 0.1f,  0.0f, 60.0f, "%.1f");
            break;
        case VfxModuleType::Visibility:
            ImGui::TextDisabled("  ※開始遅延〜開始遅延+効果時間 の間だけ表示（効果時間0=以降ずっと表示）");
            break;
        case VfxModuleType::Flicker:
            c |= YEditorWidget::DragFloat("明滅の深さ##mfa",   m.amplitude, 0.01f, 0.0f, 1.0f, "%.2f");
            c |= YEditorWidget::DragFloat("明滅回数(/秒)##mff", m.frequency, 0.5f,  0.0f, 120.0f, "%.1f");
            break;
        }
        return c;
    }

    // 種類を選んでモジュールを足すメニュー（「追加してから種類を変える」の手間を省く）
    bool VfxMeshEditor::DrawAddModuleMenu(std::vector<VfxModule>& modules, const char* popupId)
    {
        bool added = false;
        if (ImGui::BeginPopup(popupId)) {
            for (int i = 0; i < VfxModuleTypeCount(); ++i) {
                const auto type = static_cast<VfxModuleType>(i);
                ImGui::PushID(i);
                if (ImGui::Selectable(VfxModuleTypeLabel(type))) {
                    VfxModule m;
                    m.type = type;
                    modules.push_back(m);
                    added = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", VfxModuleTypeDescription(type));
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::Selectable("Waypoint Beam セット（BeamPulse + Flicker）")) {
                AddWaypointBeamModule(modules);
                added = true;
            }
            ImGui::EndPopup();
        }
        return added;
    }

    // モジュールリスト編集（エレメント個別の動き。インスペクタ内に並べる）
    void VfxMeshEditor::DrawModuleListUI(std::vector<VfxModule>& modules,
                                         bool showTarget, const char* commitLabel)
    {
        auto* sel = Selected();
        if (!sel) return;

        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(modules.size()); ++i) {
            ImGui::PushID(i);
            auto& m = modules[i];
            const std::string header = std::string(VfxModuleTypeLabel(m.type)) + "###mod";
            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                VfxEffectAsset b = sel->asset;
                if (DrawModuleBody(m, showTarget)) CommitChange(b, commitLabel);
                if (ImGui::SmallButton(ICON_FA_TRASH " 削除##m")) removeIdx = i;
            }
            ImGui::PopID();
        }

        if (removeIdx >= 0) {
            VfxEffectAsset b = sel->asset;
            modules.erase(modules.begin() + removeIdx);
            CommitChange(b, "モジュール削除");
        }

        if (ImGui::Button(ICON_FA_PLUS " 動きを追加")) ImGui::OpenPopup("##addElemModule");
        {
            VfxEffectAsset b = sel->asset;
            if (DrawAddModuleMenu(modules, "##addElemModule")) CommitChange(b, "モジュール追加");
        }
        if (modules.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(動きなし)");
        }
    }

    void VfxMeshEditor::ApplyDefaultElementModules(VfxElement& sub)
    {
        switch (sub.type) {
        case VfxElementType::LightVolume:
            sub.lightVolume.beamStrength = 0.85f;
            sub.lightVolume.beamRadius = 0.32f;
            sub.lightVolume.beamPower = 2.5f;
            sub.lightVolume.beamGlow = 3.0f;
            sub.lightVolume.edgeFade = 0.08f;
            sub.lightVolume.depthFade = 50.0f;
            AddWaypointBeamModule(sub.modules);
            break;

        case VfxElementType::NoiseVolume: {
            sub.smoke.builtInBurstMotion = false;
            sub.smoke.riseSpeed = 0.f;
            sub.smoke.color = { 1.0f, 1.0f, 1.0f, 1.0f };

            VfxModule grow;
            grow.type = VfxModuleType::ScaleOverLife;
            grow.ease = VfxEase::EaseOutExpo;
            grow.window = 2.0f;
            grow.scaleStart = 0.3f;
            grow.scaleEnd = 1.6f;
            sub.modules.push_back(grow);

            VfxModule rise;
            rise.type = VfxModuleType::Rise;
            rise.startTime = 0.2f;
            rise.window = 1.8f;
            rise.velocity = { 0.f, 1.0f, 0.f };
            rise.amplitude = 1.0f;
            sub.modules.push_back(rise);

            VfxModule fade;
            fade.type = VfxModuleType::FadeInOut;
            fade.window = 2.0f;
            fade.fadeIn = 0.05f;
            fade.fadeOut = 0.7f;
            sub.modules.push_back(fade);

            VfxModule color;
            color.type = VfxModuleType::ColorOverLife;
            color.ease = VfxEase::EaseOutCubic;
            color.window = 0.7f;
            color.colorStart = { 1.2f, 0.6f, 0.3f, 1.0f };
            color.colorEnd = { 0.22f, 0.22f, 0.24f, 1.0f };
            sub.modules.push_back(color);

            VfxModule pulse;
            pulse.type = VfxModuleType::ScalePulse;
            pulse.amplitude = 0.08f;
            pulse.frequency = 1.2f;
            sub.modules.push_back(pulse);

            VfxModule shake;
            shake.type = VfxModuleType::Shake;
            shake.amplitude = 0.06f;
            shake.frequency = 1.5f;
            sub.modules.push_back(shake);
            break;
        }

        default:
            break;
        }
    }

} // namespace YoRigine
#endif

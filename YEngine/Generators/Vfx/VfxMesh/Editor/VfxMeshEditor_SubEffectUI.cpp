#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "Core/Editor/Widgets/YEditorWidget.h"
#include <Vfx/VfxMesh/Core/VfxMeshRegistry.h>
#include <Vfx/VfxMesh/Core/GeometryRegistry.h>
#include <Vfx/VfxMesh/Core/MaterialRegistry.h>

namespace YoRigine {

    namespace {
        constexpr float kRad2Deg = 57.29577951308232f;
        constexpr float kDeg2Rad = 0.017453292519943295f;

        // レジストリの displayName から現在 type のインデックスを引く
        template <typename Registry, typename TypeEnum>
        int CurrentComboIndex(const Registry& reg, TypeEnum type) {
            const auto& all = reg.GetAll();
            for (int i = 0; i < static_cast<int>(all.size()); ++i)
                if (all[i].type == type) return i;
            return 0;
        }
    }

    // 形状（Composed）1個分の編集 UI
    void VfxMeshEditor::DrawComposedElementBody(VfxElement& sub)
    {
        auto* sel = Selected();
        if (!sel) return;

        auto& geomReg = GeometryRegistry::Instance();
        auto& matReg  = MaterialRegistry::Instance();

        // ── 回転（Cone の向き / 地面デカール用） ──
        {
            Vector3 euler = QuaternionToEuler(sub.rotation);
            Vector3 deg = { euler.x * kRad2Deg, euler.y * kRad2Deg, euler.z * kRad2Deg };
            VfxEffectAsset b = sel->asset;
            if (YEditorWidget::DragVec3("回転(deg)##rot", deg, 0.5f, -180.f, 180.f, "%.1f")) {
                Vector3 rad = { deg.x * kDeg2Rad, deg.y * kDeg2Rad, deg.z * kDeg2Rad };
                sub.rotation = EulerToQuaternion(rad);
                CommitChange(b, "エレメント 回転");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("地面に寝かせる##rot")) {
                sub.rotation = MakeRotateAxisAngleQuaternion(Vector3{ 1,0,0 }, -1.57079633f);
                CommitChange(b, "エレメント 回転(地面)");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("向きリセット##rot")) {
                sub.rotation = Quaternion::Identity();
                CommitChange(b, "エレメント 回転リセット");
            }
        }

        ImGui::Spacing();

        // ── 形状（Geometry）選択 ──
        YEditorWidget::SectionHeader("形状 (Geometry)");
        {
            const auto& all = geomReg.GetAll();
            std::vector<const char*> names;
            names.reserve(all.size());
            for (const auto& d : all) names.push_back(d.displayName);
            int cur = CurrentComboIndex(geomReg, sub.GeometryType());
            ImGui::SetNextItemWidth(180);
            VfxEffectAsset b = sel->asset;
            if (ImGui::Combo("形状##geomType", &cur, names.data(), static_cast<int>(names.size()))) {
                sub.geom = geomReg.MakeDefault(all[cur].type);
                CommitChange(b, "形状 変更");
            }
        }
        {
            VfxEffectAsset b = sel->asset;
            if (geomReg.DrawUI(sub.GeometryType(), sub.geom))
                CommitChange(b, "形状パラメータ");
        }

        ImGui::Spacing();

        // ── マテリアル（Material）選択 ──
        YEditorWidget::SectionHeader("マテリアル (Material)");
        {
            const auto& all = matReg.GetAll();
            std::vector<const char*> names;
            names.reserve(all.size());
            for (const auto& d : all) names.push_back(d.displayName);
            int cur = CurrentComboIndex(matReg, sub.MaterialType());
            ImGui::SetNextItemWidth(180);
            VfxEffectAsset b = sel->asset;
            if (ImGui::Combo("マテリアル##matType", &cur, names.data(), static_cast<int>(names.size()))) {
                sub.mat = matReg.MakeDefault(all[cur].type);
                CommitChange(b, "マテリアル 変更");
            }
        }
        {
            VfxEffectAsset b = sel->asset;
            if (matReg.DrawUI(sub.MaterialType(), sub.mat))
                CommitChange(b, "マテリアルパラメータ");
        }
    }

    // 一覧・見出しに出すエレメント名（表示名があればそれ、無ければ「形状 × マテリアル」/ 特殊エフェクト名）
    std::string VfxMeshEditor::ElementDisplayName(const VfxElement& sub)
    {
        if (!sub.label.empty()) return sub.label;
        if (sub.kind == VfxElementKind::Composed) {
            const auto* gd = GeometryRegistry::Instance().FindDesc(sub.GeometryType());
            const auto* md = MaterialRegistry::Instance().FindDesc(sub.MaterialType());
            return std::string(gd ? gd->displayName : "Shape") + " × " + (md ? md->displayName : "?");
        }
        const VfxElementDesc* desc = VfxMeshRegistry::Instance().FindDesc(sub.type);
        return desc ? desc->displayName : VfxElementTypeName(sub.type);
    }

    // エレメント追加メニュー（BeginPopup の中で呼ぶ）。追加したら true
    bool VfxMeshEditor::DrawAddElementMenuItems()
    {
        auto* sel = Selected();
        if (!sel) return false;
        auto& subs = sel->asset.elements;
        auto& geomReg = GeometryRegistry::Instance();
        auto& matReg  = MaterialRegistry::Instance();

        bool added = false;
        ImGui::TextDisabled("形状 (Geometry × Material)");
        ImGui::Separator();
        for (const auto& gd : geomReg.GetAll()) {
            if (ImGui::MenuItem(gd.displayName)) {
                VfxEffectAsset b = sel->asset;
                VfxElement e;
                e.kind = VfxElementKind::Composed;
                e.geom = geomReg.MakeDefault(gd.type);
                e.mat  = matReg.MakeDefault(VfxMaterialType::Noise);
                subs.push_back(std::move(e));
                CommitChange(b, "形状を追加");
                added = true;
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("特殊エフェクト (専用メッシュ)");
        ImGui::Separator();
        auto addMono = [&](VfxElementType t, const char* name) {
            if (ImGui::MenuItem(name)) {
                VfxEffectAsset b = sel->asset;
                VfxElement e;
                e.kind = VfxElementKind::Monolithic;
                e.type = t;
                if (auto* d = VfxMeshRegistry::Instance().FindDesc(t); d && d->applyDefaults)
                    d->applyDefaults(e);
                subs.push_back(std::move(e));
                CommitChange(b, "特殊エフェクトを追加");
                added = true;
            }
        };
        addMono(VfxElementType::LightningBolt, "Lightning (稲妻)");
        addMono(VfxElementType::LightVolume,   "LightVolume (光ボリューム)");
        return added;
    }

    // エレメント 1 個分のインスペクタ（共通設定 → 形状 / 特殊エフェクト → このエレメントだけの動き）
    void VfxMeshEditor::DrawElementInspector(VfxElement& sub)
    {
        auto* sel = Selected();
        if (!sel) return;

        // ── 表示名（一覧に出る名前） ──
        {
            char labelBuf[128] = {};
            strncpy_s(labelBuf, sizeof(labelBuf), sub.label.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(220);
            VfxEffectAsset b = sel->asset;
            if (ImGui::InputTextWithHint("表示名##sub", ElementDisplayName(sub).c_str(), labelBuf, sizeof(labelBuf))) {
                sub.label = labelBuf;
                CommitChange(b, "エレメント 表示名");
            }
            YEditorWidget::ItemTooltip("空にすると「形状 × マテリアル」の名前になります");
        }

        {
            VfxEffectAsset b = sel->asset;
            if (YEditorWidget::DragVec3("オフセット##sub", sub.offset, 0.05f, -50.f, 50.f, "%.2f"))
                CommitChange(b, "エレメント オフセット");
        }

        {
            // ブレンドモード上書き（HDRで加算が重なって飽和する場合に Normal/Screen 等へ）
            // 先頭 "既定" = マテリアル本来のブレンド（内部値 -1）
            static const char* kBlendNames[] = {
                "既定(マテリアル)", "None", "Normal", "Add", "Subtract", "Multiply", "Screen"
            };
            int cur = sub.blendModeOverride + 1; // -1(既定)→0
            if (cur < 0 || cur >= IM_ARRAYSIZE(kBlendNames)) cur = 0;
            ImGui::SetNextItemWidth(200);
            VfxEffectAsset b = sel->asset;
            if (ImGui::Combo("ブレンド##sub", &cur, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
                sub.blendModeOverride = cur - 1; // 0→-1(既定)
                CommitChange(b, "エレメント ブレンドモード");
            }
            YEditorWidget::ItemTooltip(
                "加算(Add)で重なると光が飽和して真っ白になる場合、Normal/Screen に変えると合成を抑えられます。\n"
                "既定(マテリアル)は各エフェクト本来のブレンド。");
        }

        ImGui::Spacing();

        auto commitFn = [this](const VfxEffectAsset& before, const char* label) {
            CommitChange(before, label);
        };
        if (sub.kind == VfxElementKind::Composed) {
            DrawComposedElementBody(sub);
        } else {
            if (sub.type == VfxElementType::LightVolume) {
                ImGui::Checkbox("OBB ワイヤーフレーム表示", &showVolumeDebug_);
            }
            ImGui::Spacing();
            VfxMeshRegistry::Instance().DrawUI(sub.type, sub, sel->asset, commitFn);
        }

        ImGui::Spacing();
        YEditorWidget::SectionHeader("このエレメントだけの動き");
        ImGui::TextDisabled("  ※エフェクト全体の動き（スタックの「全体の動き」）に足し合わせて適用されます");
        DrawModuleListUI(sub.modules, false, "エレメントモジュール編集");
    }

} // namespace YoRigine
#endif

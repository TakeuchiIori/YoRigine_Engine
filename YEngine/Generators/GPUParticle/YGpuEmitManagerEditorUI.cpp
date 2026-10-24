#include "YGpuEmitManagerEditorUI.h"
#include "Debugger/ImGuiManager.h"
#ifdef USE_IMGUI

#include "YGpuEmitManager.h"
#include <Debugger/Logger.h>
#include <IconsFontAwesome5.h>
#include <Loaders/Texture/TextureManager.h>
#include <ModelManager.h>
#include "imgui.h"
#include <implot/implot.h>
#include "Core/Editor/Command/CommandHistory.h"
#include "Core/Editor/Widgets/YEditorWidget_Notify.h"
#include "Core/Editor/Widgets/YEditorWidget_Tooltip.h"
#include "Core/Editor/Editor.h"
#include "Debugger/Gizmo/GizmoController.h"
#include "Debugger/Gizmo/IGizmable.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Paramモジュール（パーティクルパラメータ編集はここへ委譲）
#include "Modules/Lifetime/YLifetimeModule.h"
#include "Modules/ScaleOverLife/YScaleOverLifeModule.h"
#include "Modules/ColorOverLife/YColorOverLifeModule.h"
#include "Modules/Velocity/YVelocityModule.h"
#include "Modules/Rotation/YRotationModule.h"
// 拡張Paramモジュール（任意ON/OFF。集約構造体 GpuExtModules 経由で参照）
#include "Modules/GpuExtModules.h"

namespace YoRigine {

namespace {

	//=================================================================
	// 編集状態（選択・履歴・ダイアログ）
	//=================================================================

	/// スタックで選べる項目
	enum class GpuItem {
		Shape,      // 発生形状
		Look,       // 見た目（テクスチャ・粒子メッシュ・ビルボード）
		Lifetime, Scale, Rotation, Velocity, Color, Trail, // コア
		Ext,        // 拡張モジュール（index = 拡張モジュール表の番号）
		Force, Noise, Accel, // フィールド（index = 配列の番号）
		Stats,
	};

	/// グループ 1 つ分の編集履歴
	struct GroupEditState {
		CommandHistory history;
		nlohmann::json last;  // 直近に確定した状態
		nlohmann::json saved; // 最後に保存した状態
	};

	struct GpuEditorState {
		std::unordered_map<std::string, std::unique_ptr<GroupEditState>> groups;
		std::string boundGroup;

		GpuItem item = GpuItem::Shape;
		int     itemIndex = -1;

		// 描画中にコンテナを壊さないよう、構造変更は描画後にまとめて行う
		std::function<void()> pendingOp;
		std::string           pendingLabel;

		char  treeFilter[64] = "";
		float treeWidth = 220.0f;
		float stackWidth = 260.0f;

		bool showNewGroup = false;
		char newGroupName[128] = "";
		bool showNewEmitter = false;
		char newEmitterName[128] = "";
		int  newEmitterShape = 0;
		bool showDeleteGroup = false;
		bool showTextureBrowser = false;

		// ギズモ（ゲーム画面上で位置を動かす）
		bool showGizmo = true;
		bool gizmoInitialized = false;
		bool gizmoWasUsing = false;
		int  lastPanelFrame = -100; // パネルを最後に描いたフレーム（閉じているときはギズモを出さない）

		// 粒子数の推移（ImPlot）
		static constexpr int kStatsSamples = 240;
		std::array<float, kStatsSamples> aliveHistory{};
		int aliveHead = 0;
	};

	/// 位置の読み書きを関数で受け取る IGizmable。
	/// GizmoController は IGizmable* を保持するので、毎フレーム作り直さず 1 つを使い回す。
	class FunctionGizmable : public IGizmable {
	public:
		void Set(std::string label, std::function<Vector3()> get, std::function<void(const Vector3&)> set) {
			label_ = std::move(label);
			get_ = std::move(get);
			set_ = std::move(set);
		}
		std::string GetGizmoLabel() const override { return label_; }
		Vector3 GetGizmoPosition() const override { return get_ ? get_() : Vector3{}; }
		void SetGizmoPosition(const Vector3& pos) override { if (set_) set_(pos); }
		// 回転・スケールは扱わない（位置調整専用）
		Vector3 GetGizmoRotation() const override { return {}; }
		void SetGizmoRotation(const Vector3&) override {}
		Vector3 GetGizmoScale() const override { return { 1.0f, 1.0f, 1.0f }; }
		void SetGizmoScale(const Vector3&) override {}

	private:
		std::string label_;
		std::function<Vector3()> get_;
		std::function<void(const Vector3&)> set_;
	};

	GizmoController& Gizmo() {
		static GizmoController gizmo;
		return gizmo;
	}

	FunctionGizmable& GizmoTarget() {
		static FunctionGizmable target;
		return target;
	}

	GpuEditorState& State() {
		static GpuEditorState state;
		return state;
	}

	GroupEditState& GetGroupState(const std::string& groupName) {
		auto& slot = State().groups[groupName];
		if (!slot) slot = std::make_unique<GroupEditState>();
		return *slot;
	}

	//=================================================================
	// Undo 1 回分: グループの編集前後の JSON を丸ごと持つ
	//=================================================================

	class GroupSnapshotCommand : public ICommand {
	public:
		using ApplyFn = std::function<void(const nlohmann::json&)>;

		GroupSnapshotCommand(nlohmann::json before, nlohmann::json after, std::string label, ApplyFn apply)
			: before_(std::move(before)), after_(std::move(after)), label_(std::move(label)), apply_(std::move(apply)) {}

		void Execute() override {
			// 履歴に積む時点で既に after の状態なので、初回は何もしない（Redo のときだけ適用）
			if (firstExecute_) { firstExecute_ = false; return; }
			apply_(after_);
		}
		void Undo() override { apply_(before_); }
		const char* GetName() const override { return label_.c_str(); }

	private:
		nlohmann::json before_;
		nlohmann::json after_;
		std::string    label_;
		ApplyFn        apply_;
		bool           firstExecute_ = true;
	};

	//=================================================================
	// 見た目の部品
	//=================================================================

	ImVec4 ToImVec4(const Vector4& c) { return { c.x, c.y, c.z, c.w }; }

	/// 開始色 → 終了色 の帯（色の要約）
	void DrawColorStrip(const Vector4& a, const Vector4& b, const ImVec2& size) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const ImU32 ca = ImGui::ColorConvertFloat4ToU32(ToImVec4(a));
		const ImU32 cb = ImGui::ColorConvertFloat4ToU32(ToImVec4(b));
		dl->AddRectFilledMultiColor(p, { p.x + size.x, p.y + size.y }, ca, cb, cb, ca);
		dl->AddRect(p, { p.x + size.x, p.y + size.y }, IM_COL32(90, 90, 100, 255));
		ImGui::Dummy(size);
	}

	std::string FileNameOf(const std::string& path) {
		return path.empty() ? std::string("(なし)") : std::filesystem::path(path).filename().string();
	}

	bool MatchFilter(const std::string& name, const char* filter) {
		if (!filter || !filter[0]) return true;
		std::string a = name, b = filter;
		std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a.find(b) != std::string::npos;
	}

	template <class TMap>
	std::vector<std::string> SortedKeys(const TMap& map) {
		std::vector<std::string> keys;
		keys.reserve(map.size());
		for (const auto& [k, v] : map) keys.push_back(k);
		std::sort(keys.begin(), keys.end());
		return keys;
	}

	//=================================================================
	// 拡張モジュールの表（追加メニュー・スタック行・インスペクタで共用）
	// モジュールを増やすときはここに 1 行足すだけでよい
	//=================================================================

	struct ExtModuleEntry {
		const char* label;               // 表示名
		const char* hint;                // 用途の説明（追加メニューのツールチップ）
		bool*       isEnable;            // 追加済みフラグ
		std::function<bool()> drawBody;  // 調整 UI（変更があれば true）
		std::function<void()> reset;     // 削除時に既定値へ戻す
	};

	std::vector<ExtModuleEntry> GetExtEntries(GpuExtModules& m) {
		return {
			{ "発光", "HDR 輝度倍率。Bloom を乗せる", &m.emissive.isEnable,
			  [&] { return YEmissiveModule::DrawImGui(m.emissive); }, [&] { m.emissive = EmissiveParams{}; } },
			{ "フリップブック", "爆発 / 炎のスプライトシート", &m.flipbook.isEnable,
			  [&] { return YFlipbookModule::DrawImGui(m.flipbook); }, [&] { m.flipbook = FlipbookParams{}; } },
			{ "UV スクロール", "炎 / 溶岩 / エネルギー系の定番", &m.uvScroll.isEnable,
			  [&] { return YUVScrollModule::DrawImGui(m.uvScroll); }, [&] { m.uvScroll = UVScrollParams{}; } },
			{ "スケール脈動", "発光オーブ / 鼓動の定番", &m.scalePulse.isEnable,
			  [&] { return YScalePulseModule::DrawImGui(m.scalePulse); }, [&] { m.scalePulse = ScalePulseParams{}; } },
			{ "色の明滅", "炎 / 電撃 / 魔法の定番", &m.colorFlicker.isEnable,
			  [&] { return YColorFlickerModule::DrawImGui(m.colorFlicker); }, [&] { m.colorFlicker = ColorFlickerParams{}; } },
			{ "空気抵抗", "爆散した破片が失速する", &m.drag.isEnable,
			  [&] { return YDragModule::DrawImGui(m.drag); }, [&] { m.drag = DragParams{}; } },
			{ "速度方向へ伸縮", "火花 / スピード線の定番", &m.stretch.isEnable,
			  [&] { return YStretchByVelocityModule::DrawImGui(m.stretch); }, [&] { m.stretch = StretchByVelocityParams{}; } },
			{ "床で跳ねる", "破片のバウンド", &m.bounce.isEnable,
			  [&] { return YBounceModule::DrawImGui(m.bounce); }, [&] { m.bounce = BounceParams{}; } },
		};
	}

	//=================================================================
	// フィールド 1 個分の編集 UI
	//=================================================================

	bool DrawForceField(GpuForceFieldParams& ff) {
		bool changed = false;
		const char* shapeNames[] = { "球 (Sphere)", "箱 (AABB)" };
		const char* modeNames[]  = { "一定方向に加速", "中心へ収束", "中心から反発" };
		changed |= ImGui::Checkbox("有効", &ff.isEnable);
		int shapeIdx = static_cast<int>(ff.shape);
		if (ImGui::Combo("形状", &shapeIdx, shapeNames, 2)) { ff.shape = static_cast<GpuFieldShape>(shapeIdx); changed = true; }
		changed |= ImGui::DragFloat3("中心", &ff.center.x, 0.1f);
		if (ff.shape == GpuFieldShape::Sphere) changed |= ImGui::DragFloat("半径", &ff.radius, 0.1f, 0.1f, 100.f);
		else                                   changed |= ImGui::DragFloat3("半サイズ", &ff.halfExtents.x, 0.1f, 0.1f, 100.f);
		int modeIdx = static_cast<int>(ff.mode);
		if (ImGui::Combo("モード", &modeIdx, modeNames, 3)) { ff.mode = static_cast<GpuFieldMode>(modeIdx); changed = true; }
		if (ff.mode == GpuFieldMode::DirectionalAccel) changed |= ImGui::DragFloat3("方向", &ff.direction.x, 0.01f, -1.f, 1.f);
		changed |= ImGui::DragFloat("強度", &ff.strength, 0.1f, 0.0f, 500.f);
		changed |= ImGui::DragFloat("距離減衰", &ff.falloff, 0.01f, 0.0f, 1.f);
		if (ff.mode == GpuFieldMode::ConvergeToCenter) {
			ImGui::SeparatorText("収束の味付け");
			changed |= ImGui::DragFloat("螺旋 最小", &ff.spiralStrengthMin, 0.01f, 0.0f, 10.f);
			changed |= ImGui::DragFloat("螺旋 最大", &ff.spiralStrengthMax, 0.01f, 0.0f, 10.f);
			changed |= ImGui::DragFloat("ランダム軸ブレンド", &ff.randomAxisBlend, 0.01f, 0.0f, 1.f);
			changed |= ImGui::DragFloat("軌道保持率", &ff.orbitHoldRatio, 0.01f, 0.0f, 1.f);
			changed |= ImGui::DragFloat("収束遅延ばらつき", &ff.approachVariance, 0.01f, 0.0f, 1.f);
			changed |= ImGui::DragFloat("消滅半径", &ff.killRadius, 0.01f, 0.0f, 20.f);
		}
		changed |= ImGui::DragFloat("最大速度 (0=無制限)", &ff.maxSpeed, 0.1f, 0.0f, 200.f);
		return changed;
	}

	bool DrawNoiseField(GpuNoiseParams& nf) {
		bool changed = false;
		const char* typeNames[] = { "Curl（渦を巻いて流れる）", "Turbulence（細かく乱れる）", "Vortex（軸の周りを回る）" };
		changed |= ImGui::Checkbox("有効", &nf.isEnable);
		int typeIdx = static_cast<int>(nf.type);
		if (ImGui::Combo("種類", &typeIdx, typeNames, 3)) { nf.type = static_cast<GpuNoiseType>(typeIdx); changed = true; }
		changed |= ImGui::DragFloat("周波数", &nf.frequency, 0.01f, 0.0f, 10.f);
		changed |= ImGui::DragFloat("強さ", &nf.amplitude, 0.05f, 0.0f, 50.f);
		if (nf.type == GpuNoiseType::Turbulence) {
			int octaves = static_cast<int>(nf.octaves);
			if (ImGui::DragInt("オクターブ数", &octaves, 1, 1, 6)) { nf.octaves = static_cast<uint32_t>(octaves); changed = true; }
			changed |= ImGui::DragFloat("周波数倍率", &nf.lacunarity, 0.05f, 1.0f, 4.0f);
			changed |= ImGui::DragFloat("減衰率", &nf.gain, 0.02f, 0.0f, 1.0f);
		}
		changed |= ImGui::DragFloat3("スクロール速度", &nf.scrollSpeed.x, 0.01f);
		if (nf.type == GpuNoiseType::Vortex) changed |= ImGui::DragFloat3("回転軸", &nf.axis.x, 0.01f, -1.f, 1.f);
		changed |= ImGui::DragFloat3("中心", &nf.center.x, 0.1f);
		changed |= ImGui::DragFloat("半径 (0=無限)", &nf.radius, 0.05f, 0.0f, 50.f);
		return changed;
	}

	bool DrawAccelerationField(GpuAccelerationParams& af) {
		bool changed = false;
		const char* shapeNames[] = { "球 (Sphere)", "箱 (AABB)" };
		changed |= ImGui::Checkbox("有効", &af.isEnable);
		int shapeIdx = static_cast<int>(af.shape);
		if (ImGui::Combo("形状", &shapeIdx, shapeNames, 2)) { af.shape = static_cast<GpuFieldShape>(shapeIdx); changed = true; }
		changed |= ImGui::DragFloat3("中心", &af.center.x, 0.1f);
		if (af.shape == GpuFieldShape::Sphere) changed |= ImGui::DragFloat("半径", &af.radius, 0.1f, 0.1f, 200.f);
		else                                   changed |= ImGui::DragFloat3("半サイズ", &af.halfExtents.x, 0.1f, 0.1f, 200.f);
		changed |= ImGui::DragFloat3("加速方向", &af.direction.x, 0.01f, -1.f, 1.f);
		changed |= ImGui::DragFloat("強度", &af.strength, 0.1f, 0.0f, 500.f);
		changed |= ImGui::DragFloat("距離減衰", &af.falloff, 0.01f, 0.0f, 1.f);
		ImGui::TextDisabled("範囲は黄緑の太線でシーンに表示されます（エミッタ選択中のみ）");
		return changed;
	}

} // namespace

	// ===== 見た目: 粒子メッシュ形状とビルボード（既存の UI をそのまま使う） =====
	// ===== カテゴリタブ: 「形状/描画」（1粒子の見た目・ビルボード） =====
	static bool DrawShapeAndRenderCategory(YGpuEmitManager::EmitterData* emitterData)
	{
		bool changed = false;

		static const char* meshShapeNames[] = {
			"板ポリ(Plane)", "立方体(Box)", "リング(Ring)", "円柱(Cylinder)",
			"球(Sphere)", "円錐(Cone)", "扇形(Fan)"
		};
		bool meshDirty = false;

		int shapeIdx = static_cast<int>(emitterData->particleMeshShape);
		if (ImGui::Combo("粒子メッシュ形状", &shapeIdx, meshShapeNames, IM_ARRAYSIZE(meshShapeNames))) {
			emitterData->particleMeshShape = static_cast<ParticleMeshShape>(shapeIdx);
			meshDirty = true;
		}

		// 形状ごとの生成パラメータ（実サイズは粒子scaleで拡縮）
		auto& mp = emitterData->particleMeshParams;
		int divideI = static_cast<int>(mp.divide);
		int subdivI = static_cast<int>(mp.subdivisions);
		switch (emitterData->particleMeshShape) {
		case ParticleMeshShape::Plane:
			meshDirty |= ImGui::DragFloat("幅",   &mp.width,  0.01f, 0.01f, 100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("高さ", &mp.height, 0.01f, 0.01f, 100.0f, "%.3f");
			break;
		case ParticleMeshShape::Box:
			meshDirty |= ImGui::DragFloat("幅",     &mp.width,  0.01f, 0.01f, 100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("高さ",   &mp.height, 0.01f, 0.01f, 100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("奥行き", &mp.depth,  0.01f, 0.01f, 100.0f, "%.3f");
			break;
		case ParticleMeshShape::Ring:
			meshDirty |= ImGui::DragFloat("外周半径", &mp.outerRadius, 0.01f, 0.01f, 100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("内周半径", &mp.innerRadius, 0.01f, 0.0f,  100.0f, "%.3f");
			if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
			break;
		case ParticleMeshShape::Cylinder:
			meshDirty |= ImGui::DragFloat("外周半径", &mp.outerRadius, 0.01f, 0.01f, 100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("内周半径", &mp.innerRadius, 0.01f, 0.0f,  100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("高さ",     &mp.height,      0.01f, 0.01f, 100.0f, "%.3f");
			if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
			break;
		case ParticleMeshShape::Sphere:
			meshDirty |= ImGui::DragFloat("半径", &mp.radius, 0.01f, 0.01f, 100.0f, "%.3f");
			if (ImGui::DragInt("細分化レベル", &subdivI, 1, 0, 5)) { mp.subdivisions = static_cast<uint32_t>(subdivI); meshDirty = true; }
			break;
		case ParticleMeshShape::Cone:
			meshDirty |= ImGui::DragFloat("半径", &mp.radius, 0.01f, 0.01f, 100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("高さ", &mp.height, 0.01f, 0.01f, 100.0f, "%.3f");
			if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
			break;
		case ParticleMeshShape::Fan:
			meshDirty |= ImGui::DragFloat("半径",     &mp.radius,      0.01f, 0.01f, 100.0f, "%.3f");
			meshDirty |= ImGui::DragFloat("扇の角度", &mp.angleDegree, 1.0f,  1.0f,  360.0f, "%.1f°");
			if (ImGui::DragInt("分割数", &divideI, 1, 3, 128)) { mp.divide = static_cast<uint32_t>(divideI); meshDirty = true; }
			break;
		}

		// 形状 or パラメータが変わったらメッシュを再生成
		if (meshDirty && emitterData->emitter) {
			emitterData->emitter->SetParticleMesh(emitterData->particleMeshShape, emitterData->particleMeshParams);
			changed = true;
		}

		if (emitterData->particleMeshShape != ParticleMeshShape::Plane) {
			ImGui::TextDisabled("立体メッシュは「ビルボード」をOFFにすると向きが分かりやすくなります");
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		changed |= ImGui::Checkbox("ビルボードを有効", &emitterData->particleParams.isBillboard);
		ImGui::TextDisabled("パーティクルが常にカメラの方向を向きます");

		return changed;
	}

	//=================================================================
	// 入口
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawImGui(YGpuEmitManager& manager)
	{
		auto& st = State();
		st.lastPanelFrame = ImGui::GetFrameCount();

		// 選択中グループの履歴を結び直す（初めて触るグループは今の状態を「保存済み」とする）
		if (st.boundGroup != manager.selectedGroupName_) {
			st.boundGroup = manager.selectedGroupName_;
			if (EmitterGroup* g = manager.GetGroup(st.boundGroup)) {
				auto& gs = GetGroupState(st.boundGroup);
				gs.last = MakeGroupSnapshot(manager, *g);
				if (gs.saved.is_null()) gs.saved = gs.last;
			}
			st.aliveHistory.fill(0.0f);
		}

		HandleShortcuts(manager);
		DrawToolbar(manager);
		ImGui::Separator();

		// 左: グループ / エミッターのツリー
		ImGui::BeginChild("##gpuTree", ImVec2(st.treeWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
		st.treeWidth = ImGui::GetWindowWidth();
		DrawTree(manager);
		ImGui::EndChild();

		ImGui::SameLine();

		// 右: 選んだものの編集
		ImGui::BeginChild("##gpuEdit", ImVec2(0, 0));
		EmitterGroup* group = manager.GetGroup(manager.selectedGroupName_);
		EmitterData* emitter = group ? manager.GetEmitter(manager.selectedGroupName_, manager.selectedEmitterName_) : nullptr;
		if (emitter && emitter->emitter) {
			ImGui::BeginChild("##gpuStack", ImVec2(st.stackWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
			st.stackWidth = ImGui::GetWindowWidth();
			DrawEmitterStack(manager, *emitter);
			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("##gpuInspector", ImVec2(0, 0), ImGuiChildFlags_Borders);
			DrawEmitterInspector(manager, *emitter);
			ImGui::EndChild();
		} else if (group) {
			ImGui::BeginChild("##gpuGroup", ImVec2(0, 0), ImGuiChildFlags_Borders);
			DrawGroupInspector(manager, *group);
			ImGui::EndChild();
		} else {
			ImGui::Spacing();
			ImGui::TextDisabled("左のツリーからグループかエミッターを選ぶか、「+ グループ」で作成してください。");
		}
		ImGui::EndChild();

		DrawPopups(manager);

		// 描画が終わってから構造変更を反映し、変化があれば履歴に積む
		if (st.pendingOp) {
			auto op = std::move(st.pendingOp);
			st.pendingOp = nullptr;
			op();
		}
		TrackChanges(manager);
	}

	//=================================================================
	// ギズモ（ゲーム画面上で位置を動かす）
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawGizmo(YGpuEmitManager& manager)
	{
		auto& st = State();
		// ギズモ OFF / パネルが閉じている / カメラ未設定なら出さない
		if (!st.showGizmo || !manager.camera_ || ImGui::GetFrameCount() - st.lastPanelFrame > 1) return;

		EmitterGroup* group = manager.GetGroup(manager.selectedGroupName_);
		if (!group) return;

		auto& gizmo = Gizmo();
		if (!st.gizmoInitialized) {
			gizmo.Initialize();
			st.gizmoInitialized = true;
		}
		// 位置調整専用（回転・拡縮は扱わない）
		gizmo.SetOperation(GizmoController::Operation::Translate);

		// 操作対象を決める。ラムダは毎回名前で引き直すので、対象が消えても危なくない
		const std::string groupName = group->name;
		const std::string emitterName = manager.selectedEmitterName_;
		EmitterData* e = manager.GetEmitter(groupName, emitterName);
		auto& target = GizmoTarget();
		bool undoable = true; // グループ位置は Undo の対象外（ゲーム側が上書きする値のため）

		auto origin = [&manager, groupName] {
			EmitterGroup* g = manager.GetGroup(groupName);
			return g ? g->translate : Vector3{};
		};

		const int i = st.itemIndex;
		if (e && st.item == GpuItem::Force && i >= 0 && i < static_cast<int>(e->forceFields.size())) {
			target.Set("GPU::Force::" + emitterName,
				[&manager, groupName, emitterName, i, origin] {
					auto* em = manager.GetEmitter(groupName, emitterName);
					return (em && i < static_cast<int>(em->forceFields.size())) ? origin() + em->forceFields[i].center : origin();
				},
				[&manager, groupName, emitterName, i, origin](const Vector3& pos) {
					auto* em = manager.GetEmitter(groupName, emitterName);
					if (em && i < static_cast<int>(em->forceFields.size())) em->forceFields[i].center = pos - origin();
				});
		} else if (e && st.item == GpuItem::Noise && i >= 0 && i < static_cast<int>(e->noiseFields.size())) {
			target.Set("GPU::Noise::" + emitterName,
				[&manager, groupName, emitterName, i, origin] {
					auto* em = manager.GetEmitter(groupName, emitterName);
					return (em && i < static_cast<int>(em->noiseFields.size())) ? origin() + em->noiseFields[i].center : origin();
				},
				[&manager, groupName, emitterName, i, origin](const Vector3& pos) {
					auto* em = manager.GetEmitter(groupName, emitterName);
					if (em && i < static_cast<int>(em->noiseFields.size())) em->noiseFields[i].center = pos - origin();
				});
		} else if (e && st.item == GpuItem::Accel && i >= 0 && i < static_cast<int>(e->accelerationFields.size())) {
			target.Set("GPU::Accel::" + emitterName,
				[&manager, groupName, emitterName, i, origin] {
					auto* em = manager.GetEmitter(groupName, emitterName);
					return (em && i < static_cast<int>(em->accelerationFields.size())) ? origin() + em->accelerationFields[i].center : origin();
				},
				[&manager, groupName, emitterName, i, origin](const Vector3& pos) {
					auto* em = manager.GetEmitter(groupName, emitterName);
					if (em && i < static_cast<int>(em->accelerationFields.size())) em->accelerationFields[i].center = pos - origin();
				});
		} else if (e) {
			target.Set("GPU::Emitter::" + emitterName,
				[&manager, groupName, emitterName, origin] {
					auto* em = manager.GetEmitter(groupName, emitterName);
					return em ? origin() + manager.GetEmitterLocalOffset(em) : origin();
				},
				[&manager, groupName, emitterName, origin](const Vector3& pos) {
					if (auto* em = manager.GetEmitter(groupName, emitterName)) manager.SetEmitterLocalOffset(em, pos - origin());
				});
		} else {
			undoable = false;
			target.Set("GPU::Group::" + groupName, origin,
				[&manager, groupName](const Vector3& pos) {
					if (EmitterGroup* g = manager.GetGroup(groupName)) g->translate = pos;
				});
		}

		std::vector<IGizmable*> targets{ &target };
		gizmo.Draw(manager.camera_, targets, Editor::GetInstance()->GetGameViewPos(), Editor::GetInstance()->GetGameViewSize());
		// ギズモ側の履歴は使わない（GPU エディタのスナップショット Undo に一本化し、二重 Undo を防ぐ）
		gizmo.ClearHistory();

		// ドラッグを離した瞬間の変更を「ギズモで移動」という 1 件の履歴にする
		const bool usingNow = gizmo.IsUsing();
		if (st.gizmoWasUsing && !usingNow && undoable && st.pendingLabel.empty()) st.pendingLabel = "ギズモで移動";
		st.gizmoWasUsing = usingNow;
	}

	//=================================================================
	// ツールバー
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawToolbar(YGpuEmitManager& manager)
	{
		auto& st = State();
		EmitterGroup* group = manager.GetGroup(manager.selectedGroupName_);
		GroupEditState* gs = group ? &GetGroupState(group->name) : nullptr;

		ImGui::BeginDisabled(!group);
		if (ImGui::Button(ICON_FA_SAVE) && group) SaveGroup(manager, group->name);
		YEditorWidget::ItemTooltip("このグループを保存 (Ctrl+S)");
		ImGui::SameLine();
		ImGui::BeginDisabled(!gs || !gs->history.CanUndo());
		if (ImGui::Button(ICON_FA_UNDO) && gs) gs->history.Undo();
		ImGui::EndDisabled();
		YEditorWidget::ItemTooltip("元に戻す (Ctrl+Z)");
		ImGui::SameLine();
		ImGui::BeginDisabled(!gs || !gs->history.CanRedo());
		if (ImGui::Button(ICON_FA_REDO) && gs) gs->history.Redo();
		ImGui::EndDisabled();
		YEditorWidget::ItemTooltip("やり直し (Ctrl+Y)");

		// 再生 / 停止
		ImGui::SameLine(0, 16);
		ImGui::TextDisabled("|");
		ImGui::SameLine(0, 16);
		if (group && group->isPlaying) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImGuiManager::GetDangerColor());
			if (ImGui::Button(ICON_FA_STOP " 停止")) manager.StopEmitterGroup(group->name);
			ImGui::PopStyleColor();
		} else {
			ImGui::PushStyleColor(ImGuiCol_Button, ImGuiManager::GetSuccessColor());
			if (ImGui::Button(ICON_FA_PLAY " 再生") && group) manager.PlayEmitterGroup(group->name);
			ImGui::PopStyleColor();
		}
		if (group) {
			ImGui::SameLine();
			ImGui::TextDisabled("%.2f 秒", group->currentTime);
		}
		ImGui::EndDisabled();

		ImGui::SameLine(0, 16);
		ImGui::Checkbox(ICON_FA_DRAW_POLYGON " 形状表示", &manager.showEmitterGizmos_);
		YEditorWidget::ItemTooltip("選択グループのエミッタ形状をシーンにライン表示（黄 = 選択中 / 水色 = 同じグループの他）");
		ImGui::SameLine();
		ImGui::Checkbox(ICON_FA_ARROWS_ALT " ギズモ", &st.showGizmo);
		YEditorWidget::ItemTooltip("ゲーム画面上の矢印で位置を動かす\n"
			"・フィールドを選択中 → そのフィールドの中心\n"
			"・エミッターを選択中 → エミッターの発生位置\n"
			"・グループだけ選択中 → グループ全体（プレビュー位置）");

		// ファイル（上級者向けの一括操作）
		ImGui::SameLine(0, 16);
		if (ImGui::Button(ICON_FA_FOLDER " ファイル")) ImGui::OpenPopup("##gpuFileMenu");
		if (ImGui::BeginPopup("##gpuFileMenu")) {
			ImGui::TextDisabled("通常はグループ単位で保存（Ctrl+S）してください。以下は全グループの一括操作です。");
			ImGui::SetNextItemWidth(360);
			ImGui::InputText("一括ファイル", manager.saveFilePath_, sizeof(manager.saveFilePath_));
			if (ImGui::MenuItem(ICON_FA_SAVE " 全グループを一括保存")) {
				if (manager.SaveToFile(manager.saveFilePath_)) YEditorWidget::NotifySuccess("一括保存しました: %s", manager.saveFilePath_);
				else                                           YEditorWidget::NotifyError("一括保存に失敗しました: %s", manager.saveFilePath_);
			}
			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " 一括ファイルを読み込み")) {
				st.groups.clear(); // 読み直したグループの履歴は意味を失う
				if (manager.LoadFromFile(manager.saveFilePath_)) YEditorWidget::NotifySuccess("読み込みました: %s", manager.saveFilePath_);
				else                                             YEditorWidget::NotifyError("読み込みに失敗しました: %s", manager.saveFilePath_);
			}
			ImGui::EndPopup();
		}

		// 未保存の表示
		if (group && gs) {
			ImGui::SameLine(0, 16);
			if (IsGroupDirty(group->name)) ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "* 未保存");
			else                           ImGui::TextDisabled("保存済み");
			YEditorWidget::ItemTooltip(("保存先: " + group->sourceFilePath).c_str());
		}
	}

	//=================================================================
	// ツリー（左）
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawTree(YGpuEmitManager& manager)
	{
		auto& st = State();

		if (ImGui::Button(ICON_FA_PLUS " グループ")) {
			st.newGroupName[0] = '\0';
			st.showNewGroup = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##filter", ICON_FA_SEARCH " 検索", st.treeFilter, sizeof(st.treeFilter));
		ImGui::Separator();

		for (const auto& groupName : SortedKeys(manager.groups_)) {
			EmitterGroup* group = manager.GetGroup(groupName);
			if (!group) continue;

			// グループ名かエミッター名のどちらかが一致すれば表示する
			bool visible = MatchFilter(groupName, st.treeFilter);
			for (const auto& [en, e] : group->emitters) visible |= MatchFilter(en, st.treeFilter);
			if (!visible) continue;

			ImGui::PushID(groupName.c_str());
			const bool groupSelected = (manager.selectedGroupName_ == groupName);
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_AllowOverlap;
			if (groupSelected && manager.selectedEmitterName_.empty()) flags |= ImGuiTreeNodeFlags_Selected;
			if (groupSelected || st.treeFilter[0]) ImGui::SetNextItemOpen(true, ImGuiCond_Once);

			const std::string label = std::string(group->isPlaying ? ICON_FA_PLAY " " : "") + groupName
				+ (IsGroupDirty(groupName) ? " *" : "");
			const bool open = ImGui::TreeNodeEx("##group", flags, "%s", label.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				manager.selectedGroupName_ = groupName;
				manager.selectedEmitterName_.clear();
			}
			if (ImGui::BeginPopupContextItem("##groupMenu")) {
				manager.selectedGroupName_ = groupName;
				if (ImGui::MenuItem(ICON_FA_SAVE " 保存")) SaveGroup(manager, groupName);
				if (ImGui::MenuItem(ICON_FA_PLUS " エミッターを追加...")) {
					manager.selectedEmitterName_.clear();
					st.newEmitterName[0] = '\0';
					st.showNewEmitter = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem(ICON_FA_TRASH " グループを削除...")) st.showDeleteGroup = true;
				ImGui::EndPopup();
			}

			if (open) {
				for (const auto& emitterName : SortedKeys(group->emitters)) {
					EmitterData* e = group->emitters[emitterName].get();
					if (!MatchFilter(emitterName, st.treeFilter) && !MatchFilter(groupName, st.treeFilter)) continue;

					ImGui::PushID(emitterName.c_str());
					ImGui::Checkbox("##active", &e->isActive);
					YEditorWidget::ItemTooltip("有効 / 無効");
					ImGui::SameLine();
					const bool selected = groupSelected && manager.selectedEmitterName_ == emitterName;
					if (!e->isActive) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
					if (ImGui::Selectable(emitterName.c_str(), selected, ImGuiSelectableFlags_AllowOverlap)) {
						manager.selectedGroupName_ = groupName;
						manager.selectedEmitterName_ = emitterName;
					}
					if (!e->isActive) ImGui::PopStyleColor();
					if (ImGui::BeginPopupContextItem("##emitterMenu")) {
						manager.selectedGroupName_ = groupName;
						manager.selectedEmitterName_ = emitterName;
						if (ImGui::MenuItem(ICON_FA_CLONE " 複製", "Ctrl+D")) RequestDuplicateEmitter(manager, groupName, emitterName);
						ImGui::Separator();
						if (ImGui::MenuItem(ICON_FA_TRASH " 削除", "Del")) RequestDeleteEmitter(manager, groupName, emitterName);
						ImGui::EndPopup();
					}
					ImGui::SameLine();
					ImGui::TextDisabled("%s", YGpuEmitManager::shapeNames_[static_cast<int>(e->shape)]);
					ImGui::PopID();
				}

				// グループ内にそのまま追加できるようにする
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				if (ImGui::Selectable(ICON_FA_PLUS " エミッターを追加")) {
					manager.selectedGroupName_ = groupName;
					manager.selectedEmitterName_.clear();
					st.newEmitterName[0] = '\0';
					st.showNewEmitter = true;
				}
				ImGui::PopStyleColor();
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		if (manager.groups_.empty()) ImGui::TextDisabled("(グループなし)");

		// ツリーにフォーカスがあるときだけ、選択中のエミッターへショートカットを効かせる
		// （他のパネルで Delete を押しても消えないように。削除は Undo で戻せる）
		if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && !manager.selectedEmitterName_.empty()
			&& manager.GetEmitter(manager.selectedGroupName_, manager.selectedEmitterName_)) {
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
				RequestDeleteEmitter(manager, manager.selectedGroupName_, manager.selectedEmitterName_);
			} else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)) {
				RequestDuplicateEmitter(manager, manager.selectedGroupName_, manager.selectedEmitterName_);
			}
		}
	}

	void YGpuEmitManagerEditorUI::HandleShortcuts(YGpuEmitManager& manager)
	{
		const bool panelFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (!panelFocused) return;

		EmitterGroup* group = manager.GetGroup(manager.selectedGroupName_);
		if (!group) return;
		auto& gs = GetGroupState(group->name);

		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) SaveGroup(manager, group->name);
		// 文字入力中の Ctrl+Z は入力欄自身の取り消しに使われるので、グループの Undo は走らせない
		if (!ImGui::GetIO().WantTextInput) gs.history.HandleKeyInput();
	}

	//=================================================================
	// グループのインスペクタ（グループだけ選んでいるとき）
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawGroupInspector(YGpuEmitManager& manager, EmitterGroup& group)
	{
		auto& st = State();
		ImGui::SeparatorText((std::string(ICON_FA_LAYER_GROUP " グループ: ") + group.name).c_str());

		if (ImGui::BeginTable("##groupProps", 2, ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableSetupColumn("項目", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.0f);
			ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch);

			auto row = [](const char* label) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);
			};

			row("有効");
			ImGui::Checkbox("##active", &group.isActive);
			row("システム寿命");
			ImGui::DragFloat("##duration", &group.systemDuration, 0.1f, 0.0f, 60.0f, "%.1f 秒 (0=無限)");
			row("位置");
			ImGui::DragFloat3("##translate", &group.translate.x, 0.1f);
			YEditorWidget::ItemTooltip("プレビュー用の発生位置。ゲーム中は発生位置で上書きされます（Undo の対象外）");
			row("エミッター数");
			ImGui::Text("%zu", group.emitters.size());
			row("保存先");
			ImGui::TextDisabled("%s", group.sourceFilePath.c_str());
			ImGui::EndTable();
		}

		ImGui::Spacing();
		if (ImGui::Button(ICON_FA_PLUS " エミッターを追加")) {
			st.newEmitterName[0] = '\0';
			st.showNewEmitter = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_SAVE " 保存")) SaveGroup(manager, group.name);
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImGuiManager::GetDangerColor());
		if (ImGui::Button(ICON_FA_TRASH " グループを削除...")) st.showDeleteGroup = true;
		ImGui::PopStyleColor();

		ImGui::Spacing();
		ImGui::TextDisabled("左のツリーでエミッターを選ぶと、そのエミッターを編集できます。");
	}

	//=================================================================
	// エミッターのスタック（中）
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawEmitterStack(YGpuEmitManager& manager, EmitterData& e)
	{
		auto& st = State();
		auto& pp = e.particleParams;

		// 1 行分: 選択できる名前 + 右側の要約
		auto row = [&](GpuItem item, int index, const char* label, const std::function<void()>& summary) {
			ImGui::PushID(static_cast<int>(item) * 100 + index);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const bool selected = (st.item == item && st.itemIndex == index);
			if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
				st.item = item;
				st.itemIndex = index;
			}
			ImGui::TableSetColumnIndex(1);
			if (summary) summary();
			ImGui::PopID();
		};
		auto text = [](const std::string& s) { return [s] { ImGui::TextDisabled("%s", s.c_str()); }; };
		auto beginSection = [](const char* title) {
			ImGui::SeparatorText(title);
			return ImGui::BeginTable("##rows", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX);
		};
		auto setupColumns = [] {
			ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("##summary", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.0f);
		};

		ImGui::TextUnformatted(e.name.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("/ %s", manager.selectedGroupName_.c_str());

		// ── 基本 ──
		ImGui::PushID("basic");
		if (beginSection("基本")) {
			setupColumns();
			const float count = [&] {
				switch (e.shape) {
				case EmitterShape::Sphere:   return e.sphereParams.count;
				case EmitterShape::Box:      return e.boxParams.count;
				case EmitterShape::Triangle: return e.triangleParams.count;
				case EmitterShape::Cone:     return e.coneParams.count;
				case EmitterShape::Mesh:     return e.meshParams.count;
				case EmitterShape::Ring:     return e.ringParams.count;
				case EmitterShape::Line:     return e.lineParams.count;
				}
				return 0.0f;
				}();
			row(GpuItem::Shape, -1, ICON_FA_SHAPES " 発生形状",
				text(std::format("{} {:.0f}個", YGpuEmitManager::shapeNames_[static_cast<int>(e.shape)], count)));
			row(GpuItem::Look, -1, ICON_FA_IMAGE " 見た目", text(FileNameOf(e.texturePath)));
			ImGui::EndTable();
		}
		ImGui::PopID();

		// ── コア（全粒子が持つ基本プロパティ） ──
		ImGui::PushID("core");
		if (beginSection("コア")) {
			setupColumns();
			row(GpuItem::Lifetime, -1, "生存時間", text(std::format("{:.2f} ±{:.2f} 秒", pp.lifeTime, pp.lifeTimeVariance)));
			row(GpuItem::Scale, -1, "スケール", text(std::format("{:.2f} → {:.2f}", pp.startScale.x, pp.endScale.x)));
			row(GpuItem::Rotation, -1, "回転", text(std::format("{:.1f} /秒", pp.rotationSpeed)));
			row(GpuItem::Velocity, -1, "速度・重力", text(std::format("Y {:.2f} / 重力 {:.2f}", pp.velocity.y, pp.gravity)));
			row(GpuItem::Color, -1, "色", [&] {
				const float h = ImGui::GetTextLineHeight() * 0.8f;
				DrawColorStrip(pp.startColor, pp.endColor, ImVec2(ImGui::GetContentRegionAvail().x, h));
				});
			row(GpuItem::Trail, -1, "トレイル", text(pp.child.isTrail ? "ON" : "OFF"));
			ImGui::EndTable();
		}
		ImGui::PopID();

		// ── 拡張モジュール（任意で追加する演出） ──
		ImGui::PushID("ext");
		auto entries = GetExtEntries(e.extModules);
		int addedCount = 0;
		for (const auto& entry : entries) addedCount += *entry.isEnable ? 1 : 0;
		if (beginSection("拡張モジュール")) {
			setupColumns();
			for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
				if (!*entries[i].isEnable) continue;
				row(GpuItem::Ext, i, entries[i].label, nullptr);
				if (ImGui::BeginPopupContextItem("##extMenu")) {
					if (ImGui::MenuItem(ICON_FA_TRASH " 削除")) {
						*entries[i].isEnable = false;
						entries[i].reset();
						manager.UpdateParticleParams(&e);
					}
					ImGui::EndPopup();
				}
			}
			ImGui::EndTable();
		}
		if (addedCount == 0) ImGui::TextDisabled("  (なし)");
		if (ImGui::Button(ICON_FA_PLUS " 追加##ext", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##addExt");
		if (ImGui::BeginPopup("##addExt")) {
			bool any = false;
			for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
				if (*entries[i].isEnable) continue;
				any = true;
				if (ImGui::Selectable(entries[i].label)) {
					*entries[i].isEnable = true;
					manager.UpdateParticleParams(&e);
					st.item = GpuItem::Ext;
					st.itemIndex = i;
					st.pendingLabel = std::string("追加: ") + entries[i].label;
				}
				ImGui::SameLine();
				ImGui::TextDisabled("%s", entries[i].hint);
			}
			if (!any) ImGui::TextDisabled("すべて追加済みです");
			ImGui::EndPopup();
		}
		ImGui::PopID();

		// ── フィールド（範囲内の粒子に力を加える） ──
		ImGui::PushID("field");
		const bool hasField = !e.forceFields.empty() || !e.noiseFields.empty() || !e.accelerationFields.empty();
		if (beginSection("フィールド")) {
			setupColumns();
			for (int i = 0; i < static_cast<int>(e.forceFields.size()); ++i) {
				const auto& f = e.forceFields[i];
				row(GpuItem::Force, i, std::format("フォース {}", i).c_str(), text(f.isEnable ? std::format("強度 {:.1f}", f.strength) : "無効"));
			}
			for (int i = 0; i < static_cast<int>(e.noiseFields.size()); ++i) {
				const auto& f = e.noiseFields[i];
				row(GpuItem::Noise, i, std::format("ノイズ {}", i).c_str(), text(f.isEnable ? std::format("強さ {:.1f}", f.amplitude) : "無効"));
			}
			for (int i = 0; i < static_cast<int>(e.accelerationFields.size()); ++i) {
				const auto& f = e.accelerationFields[i];
				row(GpuItem::Accel, i, std::format("加速 {}", i).c_str(), text(f.isEnable ? std::format("強度 {:.1f}", f.strength) : "無効"));
			}
			ImGui::EndTable();
		}
		if (!hasField) ImGui::TextDisabled("  (なし)");
		if (ImGui::Button(ICON_FA_PLUS " 追加##field", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##addField");
		if (ImGui::BeginPopup("##addField")) {
			auto addItem = [&](const char* label, const char* hint, bool canAdd, GpuItem item, auto& vec) {
				ImGui::BeginDisabled(!canAdd);
				if (ImGui::Selectable(label)) {
					vec.emplace_back();
					st.item = item;
					st.itemIndex = static_cast<int>(vec.size()) - 1;
					st.pendingLabel = std::string("追加: ") + label;
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::TextDisabled("%s", canAdd ? hint : "上限に達しています");
			};
			addItem("フォースフィールド", "範囲内で加速 / 収束 / 反発", e.forceFields.size() < YGpuEmitter::kMaxForceFields, GpuItem::Force, e.forceFields);
			addItem("ノイズフィールド", "Curl / 乱流 / 渦で揺らす", e.noiseFields.size() < YGpuEmitter::kMaxNoiseFields, GpuItem::Noise, e.noiseFields);
			addItem("加速フィールド", "範囲内で一定方向へ加速", e.accelerationFields.size() < YGpuEmitter::kMaxAccelerationFields, GpuItem::Accel, e.accelerationFields);
			ImGui::EndPopup();
		}
		ImGui::PopID();

		// ── 統計 ──
		ImGui::PushID("stats");
		if (beginSection("統計")) {
			setupColumns();
			auto stats = e.emitter->GetYGpuParticle()->GetCachedStats();
			row(GpuItem::Stats, -1, ICON_FA_CHART_AREA " 粒子数",
				text(stats.isValid ? std::format("{} / {}", stats.activeCount, stats.maxParticles) : std::string("読込中")));
			ImGui::EndTable();
		}
		ImGui::PopID();

		// ツリー側のショートカットと同じく、ここでも Delete は「選んだフィールド / 拡張」を消すだけにしない
		// （誤操作を避けるため、削除はインスペクタのボタンか右クリックで行う）
	}

	//=================================================================
	// エミッターのインスペクタ（右）
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawEmitterInspector(YGpuEmitManager& manager, EmitterData& e)
	{
		auto& st = State();
		auto& pp = e.particleParams;
		bool particleChanged = false;

		// 削除ボタン付きの見出し（フィールド・拡張モジュール用）
		auto headerWithDelete = [&](const std::string& title) {
			ImGui::SeparatorText(title.c_str());
			ImGui::PushStyleColor(ImGuiCol_Button, ImGuiManager::GetDangerColor());
			const bool pressed = ImGui::Button(ICON_FA_TRASH " 削除");
			ImGui::PopStyleColor();
			return pressed;
		};

		switch (st.item) {
		case GpuItem::Shape: {
			ImGui::SeparatorText(ICON_FA_SHAPES " 発生形状");
			int currentShape = static_cast<int>(e.shape);
			if (ImGui::Combo("形状", &currentShape, YGpuEmitManager::shapeNames_, static_cast<int>(kEmitterShapeCount))) {
				e.shape = static_cast<EmitterShape>(currentShape);
				e.emitter->SetEmitterShape(e.shape);
				manager.UpdateEmitterParams(&e);
			}
			ImGui::Spacing();
			if (DrawShapeEditor(manager, &e)) manager.UpdateEmitterParams(&e);
			break;
		}
		case GpuItem::Look:
			ImGui::SeparatorText(ICON_FA_IMAGE " 見た目");
			DrawTextureSection(manager, e);
			ImGui::Spacing();
			particleChanged |= DrawShapeAndRenderCategory(&e);
			break;

		case GpuItem::Lifetime: ImGui::SeparatorText("生存時間");   particleChanged |= YLifetimeModule::DrawImGui(pp);      break;
		case GpuItem::Scale:    ImGui::SeparatorText("スケール");   particleChanged |= YScaleOverLifeModule::DrawImGui(pp); break;
		case GpuItem::Rotation: ImGui::SeparatorText("回転");       particleChanged |= YRotationModule::DrawImGui(pp);      break;
		case GpuItem::Velocity: ImGui::SeparatorText("速度・重力"); particleChanged |= YVelocityModule::DrawImGui(pp);      break;
		case GpuItem::Color:    ImGui::SeparatorText("色");         particleChanged |= YColorOverLifeModule::DrawImGui(pp); break;

		case GpuItem::Trail:
			ImGui::SeparatorText("トレイル（粒子が動いた軌跡に子パーティクルを出す）");
			particleChanged |= ImGui::Checkbox("有効化", &pp.child.isTrail);
			ImGui::BeginDisabled(!pp.child.isTrail);
			particleChanged |= ImGui::Checkbox("親のスケールを継承", &pp.child.isInheritScale);
			particleChanged |= ImGui::DragFloat("寿命", &pp.child.lifeTime, 0.01f, 0.01f, 30.0f);
			particleChanged |= ImGui::DragFloat("生成距離", &pp.child.minDistance, 0.01f, 0.01f, 1000.0f);
			particleChanged |= ImGui::DragFloat("開始スケール", &pp.child.startScale, 0.01f, 0.01f, 100.0f);
			particleChanged |= ImGui::DragFloat("終了スケール", &pp.child.endScale, 0.01f, 0.0f, 100.0f);
			particleChanged |= ImGui::DragInt("1回の生成数", &pp.child.emissionCount, 1.0f, 1, 100);
			ImGui::EndDisabled();
			break;

		case GpuItem::Ext: {
			auto entries = GetExtEntries(e.extModules);
			if (st.itemIndex < 0 || st.itemIndex >= static_cast<int>(entries.size()) || !*entries[st.itemIndex].isEnable) {
				st.item = GpuItem::Shape;
				break;
			}
			auto& entry = entries[st.itemIndex];
			if (headerWithDelete(std::string("拡張: ") + entry.label)) {
				*entry.isEnable = false;
				entry.reset();
				particleChanged = true;
				st.item = GpuItem::Shape;
				st.pendingLabel = std::string("削除: ") + entry.label;
				break;
			}
			ImGui::TextDisabled("%s", entry.hint);
			particleChanged |= entry.drawBody();
			break;
		}

		case GpuItem::Force:
		case GpuItem::Noise:
		case GpuItem::Accel: {
			const int i = st.itemIndex;
			const char* names[] = { "フォースフィールド", "ノイズフィールド", "加速フィールド" };
			const int kind = (st.item == GpuItem::Force) ? 0 : (st.item == GpuItem::Noise) ? 1 : 2;
			const size_t size = (kind == 0) ? e.forceFields.size() : (kind == 1) ? e.noiseFields.size() : e.accelerationFields.size();
			if (i < 0 || static_cast<size_t>(i) >= size) {
				st.item = GpuItem::Shape;
				break;
			}
			if (headerWithDelete(std::format("{} {}", names[kind], i))) {
				if (kind == 0)      e.forceFields.erase(e.forceFields.begin() + i);
				else if (kind == 1) e.noiseFields.erase(e.noiseFields.begin() + i);
				else                e.accelerationFields.erase(e.accelerationFields.begin() + i);
				st.item = GpuItem::Shape;
				st.pendingLabel = std::string("削除: ") + names[kind];
				break;
			}
			// フィールドは Update() が毎フレーム転送するので、値を書き換えるだけでよい
			if (kind == 0)      DrawForceField(e.forceFields[i]);
			else if (kind == 1) DrawNoiseField(e.noiseFields[i]);
			else                DrawAccelerationField(e.accelerationFields[i]);
			break;
		}

		case GpuItem::Stats:
			ImGui::SeparatorText(ICON_FA_CHART_AREA " 粒子数");
			DrawStats(e);
			break;
		}

		if (particleChanged) manager.UpdateParticleParams(&e);
	}

	void YGpuEmitManagerEditorUI::DrawTextureSection(YGpuEmitManager& manager, EmitterData& e)
	{
		auto& st = State();

		ImGui::Text("テクスチャ: %s", FileNameOf(e.texturePath).c_str());
		YEditorWidget::ItemTooltip(e.texturePath.c_str());
		ImGui::SameLine();
		if (ImGui::Button(st.showTextureBrowser ? "閉じる##tex" : "変更...##tex")) {
			st.showTextureBrowser = !st.showTextureBrowser;
			if (st.showTextureBrowser) manager.ScanTextureDirectory("Resources/Textures/");
		}

		if (st.showTextureBrowser) {
			// クリックしたテクスチャをその場で反映する（実行中でも即時に切り替わる）
			const std::string groupName = manager.selectedGroupName_;
			const std::string emitterName = e.name;
			manager.textureBrowser_.SetOnFileSelected([&manager, groupName, emitterName](const std::string& path) {
				manager.SetEmitterTexture(groupName, emitterName, path);
				State().showTextureBrowser = false;
				});
			manager.textureBrowser_.Draw("##textureBrowser", ImVec2(0, ImGui::GetFontSize() * 18.0f));
		}
	}

	void YGpuEmitManagerEditorUI::DrawStats(EmitterData& e)
	{
		auto& st = State();
		auto stats = e.emitter->GetYGpuParticle()->GetCachedStats();
		if (!stats.isValid) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "統計情報を読み込み中...");
			return;
		}

		st.aliveHistory[st.aliveHead] = static_cast<float>(stats.activeCount);
		st.aliveHead = (st.aliveHead + 1) % GpuEditorState::kStatsSamples;

		ImGui::Text("生存 %u / %u（空き %u）", stats.activeCount, stats.maxParticles, stats.freeCount);
		ImGui::ProgressBar(stats.usagePercent / 100.0f, ImVec2(-1, 0), std::format("{:.1f}%", stats.usagePercent).c_str());
		if (stats.freeListIndex < 0) ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "空きがありません。発生数か寿命を減らしてください");

		if (ImPlot::BeginPlot("##gpuAlive", ImVec2(-FLT_MIN, ImGui::GetFontSize() * 7.0f), ImPlotFlags_CanvasOnly)) {
			ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxisLimits(ImAxis_X1, 0, GpuEditorState::kStatsSamples - 1, ImPlotCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0, (std::max)(static_cast<double>(stats.maxParticles), 1.0), ImPlotCond_Always);
			ImPlotSpec spec;
			spec.Offset = st.aliveHead;
			spec.FillAlpha = 0.3f;
			ImPlot::PlotShaded("##fill", st.aliveHistory.data(), GpuEditorState::kStatsSamples, 0.0, 1.0, 0.0, spec);
			ImPlot::PlotLine("##line", st.aliveHistory.data(), GpuEditorState::kStatsSamples, 1.0, 0.0, spec);
			ImPlot::EndPlot();
		}

		if (ImGui::Button("詳細統計を表示", ImVec2(-1, 0))) e.emitter->GetYGpuParticle()->DrawStatsImGui();
	}

	//=================================================================
	// ダイアログ
	//=================================================================

	void YGpuEmitManagerEditorUI::DrawPopups(YGpuEmitManager& manager)
	{
		auto& st = State();

		// ── 新規グループ ──
		if (st.showNewGroup && !ImGui::IsPopupOpen("新規 GPU グループ")) ImGui::OpenPopup("新規 GPU グループ");
		if (ImGui::BeginPopupModal("新規 GPU グループ", &st.showNewGroup, ImGuiWindowFlags_AlwaysAutoResize)) {
			if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
			ImGui::InputText("名前", st.newGroupName, sizeof(st.newGroupName));
			ImGui::TextDisabled("保存先: Resources/Json/GpuEmitters/%s.json", st.newGroupName);
			const bool exists = manager.GetGroup(st.newGroupName) != nullptr;
			if (exists) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "同じ名前のグループがあります");
			ImGui::BeginDisabled(st.newGroupName[0] == '\0' || exists);
			if (ImGui::Button("作成", ImVec2(120, 0))) {
				// エディタで最後に選んだ JSON に引きずられないよう、保存先は名前から明示的に決める
				const std::string name = st.newGroupName;
				if (manager.CreateEmitterGroup(name, "Resources/Json/GpuEmitters/" + name + ".json")) {
					manager.selectedGroupName_ = name;
					manager.selectedEmitterName_.clear();
				}
				st.showNewGroup = false;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("キャンセル", ImVec2(120, 0))) st.showNewGroup = false;
			if (!st.showNewGroup) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// ── 新規エミッター ──
		if (st.showNewEmitter && !ImGui::IsPopupOpen("新規エミッター")) ImGui::OpenPopup("新規エミッター");
		if (ImGui::BeginPopupModal("新規エミッター", &st.showNewEmitter, ImGuiWindowFlags_AlwaysAutoResize)) {
			EmitterGroup* group = manager.GetGroup(manager.selectedGroupName_);
			if (!group) {
				st.showNewEmitter = false;
			} else {
				ImGui::Text("追加先: %s", group->name.c_str());
				if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
				ImGui::InputText("名前", st.newEmitterName, sizeof(st.newEmitterName));
				ImGui::Combo("発生形状", &st.newEmitterShape, YGpuEmitManager::shapeNames_, static_cast<int>(kEmitterShapeCount));
				ImGui::TextDisabled("テクスチャは UV チェッカーで作成されます。「見た目」で変更できます");
				const bool exists = group->emitters.count(st.newEmitterName) > 0;
				if (exists) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "同じ名前のエミッターがあります");
				ImGui::BeginDisabled(st.newEmitterName[0] == '\0' || exists);
				if (ImGui::Button("作成", ImVec2(120, 0))) {
					const std::string groupName = group->name;
					const std::string name = st.newEmitterName;
					const EmitterShape shape = static_cast<EmitterShape>(st.newEmitterShape);
					st.pendingOp = [&manager, groupName, name, shape] {
						std::string tex;
						if (manager.CreateEmitter(groupName, name, tex, shape)) {
							manager.selectedGroupName_ = groupName;
							manager.selectedEmitterName_ = name;
							State().item = GpuItem::Shape;
						}
						};
					st.pendingLabel = "追加: " + name;
					st.showNewEmitter = false;
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("キャンセル", ImVec2(120, 0))) st.showNewEmitter = false;
			}
			if (!st.showNewEmitter) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// ── グループ削除（グループごと消すと履歴も消えるので確認する。ファイルは消さない）──
		if (st.showDeleteGroup && !ImGui::IsPopupOpen("グループの削除")) ImGui::OpenPopup("グループの削除");
		if (ImGui::BeginPopupModal("グループの削除", &st.showDeleteGroup, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("グループ「%s」とその中の全エミッターを削除しますか？", manager.selectedGroupName_.c_str());
			ImGui::TextDisabled("JSON ファイルは削除されません。元に戻せません。");
			ImGui::PushStyleColor(ImGuiCol_Button, ImGuiManager::GetDangerColor());
			if (ImGui::Button("削除する", ImVec2(120, 0))) {
				const std::string name = manager.selectedGroupName_;
				st.pendingOp = [&manager, name] {
					State().groups.erase(name);
					manager.DeleteEmitterGroup(name);
					YEditorWidget::NotifyInfo("グループを削除しました: %s", name.c_str());
					};
				st.showDeleteGroup = false;
			}
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::Button("キャンセル", ImVec2(120, 0))) st.showDeleteGroup = false;
			if (!st.showDeleteGroup) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}

	//=================================================================
	// 構造変更（描画後に実行）
	//=================================================================

	void YGpuEmitManagerEditorUI::RequestDeleteEmitter(YGpuEmitManager& manager, const std::string& group, const std::string& emitter)
	{
		auto& st = State();
		st.pendingOp = [&manager, group, emitter] { manager.DeleteEmitter(group, emitter); };
		// Undo で戻せるので確認は出さない
		st.pendingLabel = "削除: " + emitter;
	}

	void YGpuEmitManagerEditorUI::RequestDuplicateEmitter(YGpuEmitManager& manager, const std::string& group, const std::string& emitter)
	{
		EmitterGroup* g = manager.GetGroup(group);
		EmitterData* e = manager.GetEmitter(group, emitter);
		if (!g || !e) return;

		// 空いている名前を探して、今の値を JSON に写したエミッターを作る
		std::string newName = emitter + "_copy";
		for (int i = 1; g->emitters.count(newName); ++i) newName = emitter + "_copy" + std::to_string(i);
		nlohmann::json j = manager.SerializeEmitter(*e);
		j["name"] = newName;

		auto& st = State();
		st.pendingOp = [&manager, group, newName, j] {
			if (manager.LoadEmitterFromJson(group, j)) {
				manager.selectedGroupName_ = group;
				manager.selectedEmitterName_ = newName;
			}
			};
		st.pendingLabel = "複製: " + emitter;
	}

	//=================================================================
	// Undo（グループ単位の JSON スナップショット）と保存
	//=================================================================

	nlohmann::json YGpuEmitManagerEditorUI::MakeGroupSnapshot(YGpuEmitManager& manager, EmitterGroup& group)
	{
		nlohmann::json j;
		j["isActive"] = group.isActive;
		j["systemDuration"] = group.systemDuration;
		// translate は入れない: ゲーム側（GpuParticleHandle / Composite プレビュー）が発生のたびに上書きするため、
		// 入れると「発生させただけで履歴が積まれ未保存になる」
		// 名前をキーにした object にする（unordered_map の巡回順に左右されない）
		j["emitters"] = nlohmann::json::object();
		for (const auto& [name, e] : group.emitters) {
			j["emitters"][name] = manager.SerializeEmitter(*e);
		}
		return j;
	}

	void YGpuEmitManagerEditorUI::ApplyGroupSnapshot(YGpuEmitManager& manager, const std::string& groupName, const nlohmann::json& snapshot)
	{
		EmitterGroup* group = manager.GetGroup(groupName);
		if (!group) return;

		group->isActive = snapshot.value("isActive", group->isActive);
		group->systemDuration = snapshot.value("systemDuration", group->systemDuration);

		const auto& emitters = snapshot["emitters"];

		// スナップショットに無いエミッターを消す（追加の取り消し）
		std::vector<std::string> toDelete;
		for (const auto& [name, e] : group->emitters) {
			if (!emitters.contains(name)) toDelete.push_back(name);
		}
		for (const auto& name : toDelete) manager.DeleteEmitter(groupName, name);

		for (const auto& [name, j] : emitters.items()) {
			EmitterData* e = manager.GetEmitter(groupName, name);
			if (!e) {
				// 削除の取り消し: このエミッターだけ作り直す
				manager.LoadEmitterFromJson(groupName, j);
				continue;
			}

			// 既存のエミッターには値だけを流し込み、GPU 側は変わったところだけ作り直す
			const nlohmann::json before = manager.SerializeEmitter(*e);
			if (before == j) continue;
			manager.DeserializeEmitter(*e, j);
			e->texturePath = YGpuEmitManager::ResolveTexturePath(e->texturePath); // 空パスで止まらないように

			if (before.value("shape", -1) != j.value("shape", -1)) e->emitter->SetEmitterShape(e->shape);
			if (before.value("textureFilePath", std::string{}) != e->texturePath) e->emitter->SetTexture(e->texturePath);
			if (before["particleMeshShape"] != j["particleMeshShape"] || before["particleMeshParams"] != j["particleMeshParams"]) {
				e->emitter->SetParticleMesh(e->particleMeshShape, e->particleMeshParams);
			}
			manager.UpdateEmitterParams(e);
			manager.UpdateParticleParams(e);
		}
	}

	void YGpuEmitManagerEditorUI::TrackChanges(YGpuEmitManager& manager)
	{
		// ドラッグやテキスト入力・ギズモ操作の途中は確定を待つ（1 回のドラッグ = 1 回の Undo にするため）
		if (ImGui::IsAnyItemActive() || Gizmo().IsUsing()) return;

		auto& st = State();
		EmitterGroup* group = manager.GetGroup(manager.selectedGroupName_);
		if (!group) return;

		auto& gs = GetGroupState(group->name);
		nlohmann::json now = MakeGroupSnapshot(manager, *group);
		if (gs.last.is_null()) {
			gs.last = now;
			if (gs.saved.is_null()) gs.saved = now;
			return;
		}
		if (now == gs.last) {
			// 変化が無かった操作（ギズモをクリックしただけ等）のラベルは次の変更に持ち越さない
			st.pendingLabel.clear();
			return;
		}

		std::string label = st.pendingLabel.empty() ? std::string("値の変更") : st.pendingLabel;
		st.pendingLabel.clear();

		const std::string groupName = group->name;
		gs.history.Execute(std::make_unique<GroupSnapshotCommand>(gs.last, now, std::move(label),
			[&manager, groupName](const nlohmann::json& snapshot) {
				ApplyGroupSnapshot(manager, groupName, snapshot);
				// 適用後の状態を「直近の状態」にする（Undo 自体を新しい変更として積まないため）
				if (EmitterGroup* g = manager.GetGroup(groupName)) {
					GetGroupState(groupName).last = MakeGroupSnapshot(manager, *g);
				}
				// 選択中のエミッターが消えていたら選択を外す
				if (!manager.GetEmitter(groupName, manager.selectedEmitterName_)) manager.selectedEmitterName_.clear();
			}));
		gs.last = std::move(now);
	}

	bool YGpuEmitManagerEditorUI::SaveGroup(YGpuEmitManager& manager, const std::string& groupName)
	{
		EmitterGroup* group = manager.GetGroup(groupName);
		if (!group) return false;

		if (!manager.SaveGroupToFile(groupName)) {
			YEditorWidget::NotifyError("保存に失敗しました: %s", group->sourceFilePath.c_str());
			return false;
		}
		auto& gs = GetGroupState(groupName);
		gs.last = MakeGroupSnapshot(manager, *group);
		gs.saved = gs.last;
		YEditorWidget::NotifySuccess("保存しました: %s", FileNameOf(group->sourceFilePath).c_str());
		return true;
	}

	bool YGpuEmitManagerEditorUI::IsGroupDirty(const std::string& groupName)
	{
		auto& groups = State().groups;
		auto it = groups.find(groupName);
		if (it == groups.end() || it->second->saved.is_null()) return false;
		return it->second->last != it->second->saved;
	}

	/// <summary>
	/// 現在の形状に応じたパラメータ編集を表示
	/// </summary>
	bool YGpuEmitManagerEditorUI::DrawShapeEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		switch (emitterData->shape)
		{
		case EmitterShape::Sphere:   return DrawSphereEditor(manager, emitterData);
		case EmitterShape::Box:      return DrawBoxEditor(manager, emitterData);
		case EmitterShape::Triangle: return DrawTriangleEditor(manager, emitterData);
		case EmitterShape::Cone:     return DrawConeEditor(manager, emitterData);
		case EmitterShape::Mesh:     return DrawMeshEditor(manager, emitterData);
		case EmitterShape::Ring:     return DrawRingEditor(manager, emitterData);
		case EmitterShape::Line:     return DrawLineEditor(manager, emitterData);
		}
		return false;
	}

	/// <summary>
	/// Ring パラメータの ImGui 編集（衝撃波の輪・魔法陣向け）
	/// </summary>
	bool YGpuEmitManagerEditorUI::DrawRingEditor([[maybe_unused]] YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->ringParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("リング面の法線", &p.normal.x, 0.01f, -1.0f, 1.0f);
		changed |= ImGui::DragFloat("内周半径", &p.innerRadius, 0.1f, 0.0f, 10000.0f);
		changed |= ImGui::DragFloat("外周半径", &p.outerRadius, 0.1f, 0.0f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, YGpuParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);

		ImGui::TextDisabled("内周=外周にすると細い輪、差を付けるとドーナツ状に分布します");
		ImGui::TextDisabled("粒子はリング面内で中心から外向きに飛びます（衝撃波向き）");

#endif
		return changed;
	}

	/// <summary>
	/// Line パラメータの ImGui 編集（レーザー・ビーム向け）
	/// </summary>
	bool YGpuEmitManagerEditorUI::DrawLineEditor([[maybe_unused]] YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->lineParams;

		changed |= ImGui::DragFloat3("始点", &p.start.x, 0.1f);
		changed |= ImGui::DragFloat3("終点", &p.end.x, 0.1f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, YGpuParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);

		ImGui::TextDisabled("粒子は線の軸に直交する向きへ飛びます（ビームから横に噴き出す形）");
		ImGui::TextDisabled("グループ追従時は中点を基準に、長さと向きを保ったまま移動します");

#endif
		return changed;
	}

	/// <summary>
	/// Sphere パラメータの ImGui 編集
	/// </summary>
	bool YGpuEmitManagerEditorUI::DrawSphereEditor([[maybe_unused]] YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->sphereParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat("半径", &p.radius, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, YGpuParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);


#endif
		return changed;
	}

	/// <summary>
	/// Box パラメータの ImGui 編集
	/// </summary>
	bool YGpuEmitManagerEditorUI::DrawBoxEditor([[maybe_unused]] YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->boxParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("サイズ", &p.size.x, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, YGpuParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);


#endif
		return changed;
	}

	/// <summary>
	/// Triangle パラメータの ImGui 編集
	/// </summary>
	bool YGpuEmitManagerEditorUI::DrawTriangleEditor([[maybe_unused]] YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->triangleParams;

		changed |= ImGui::DragFloat3("頂点 1", &p.v1.x, 0.1f);
		changed |= ImGui::DragFloat3("頂点 2", &p.v2.x, 0.1f);
		changed |= ImGui::DragFloat3("頂点 3", &p.v3.x, 0.1f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, YGpuParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);
#endif
		return changed;
	}

	/// <summary>
	/// Cone パラメータの ImGui 編集
	/// </summary>
	bool YGpuEmitManagerEditorUI::DrawConeEditor([[maybe_unused]] YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->coneParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("とんがる方向", &p.direction.x, 0.01f, -1.0f, 1.0f);
		changed |= ImGui::DragFloat("半径", &p.radius, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("高さ", &p.height, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, YGpuParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);

#endif
		return changed;
	}

	bool YGpuEmitManagerEditorUI::DrawMeshEditor(YGpuEmitManager& manager, YGpuEmitManager::EmitterData* emitterData)
	{
		(void)emitterData;
#ifdef USE_IMGUI
		bool changed = false;
		auto& p = emitterData->meshParams;

		// -------------------------
		// モデル選択コンボボックス
		// -------------------------
		assert(manager.modelManager_ && "YGpuEmitManager : SetModelManager() を先に呼ぶこと");
		auto modelKeys = manager.modelManager_->GetModelKeys();
		static int selected = -1;

		// 現在の選択を反映
		if (p.model != nullptr) {
			std::string currentKey = p.model->GetName();
			for (int i = 0; i < modelKeys.size(); i++) {
				if (modelKeys[i] == currentKey) {
					selected = i;
					break;
				}
			}
		}

		if (ImGui::BeginCombo("使用モデル", selected >= 0 ? modelKeys[selected].c_str() : "未選択"))
		{
			for (int i = 0; i < modelKeys.size(); i++)
			{
				bool isSelected = (selected == i);
				if (ImGui::Selectable(modelKeys[i].c_str(), isSelected)) {
					selected = i;
					p.model = manager.modelManager_->FindModel(modelKeys[i]);
					changed = true;
				}
			}
			ImGui::EndCombo();
		}

		// -------------------------
		// 通常パラメータ
		// -------------------------

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("スケール", &p.scale.x, 0.1f);

		float r[4] = { p.rotation.x, p.rotation.y, p.rotation.z, p.rotation.w };
		if (ImGui::DragFloat4("回転(Quat)", r, 0.01f)) {
			p.rotation = Quaternion(r[0], r[1], r[2], r[3]);
			changed = true;
		}

		changed |= ImGui::DragFloat("射出数", &p.count, 1.0f);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f);

		const char* modeList[] = { "Surface", "Volume", "Edge" };
		int modeIndex = static_cast<int>(p.emitMode);
		if (ImGui::Combo("Emit Mode", &modeIndex, modeList, 3)) {
			p.emitMode = static_cast<MeshEmitMode>(modeIndex);
			changed = true;
		}

		return changed;
#else
		return false;
#endif
	}

} // namespace YoRigine

#endif

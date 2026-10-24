#ifdef USE_IMGUI

#include "YParticleSystemInspector.h"
#include "ColorGradient.h"
#include "YParticleManager.h"
#include "YParticleModuleFactory.h"
#include "YParticleSystem.h"
#include "Core/Editor/Widgets/YEditorWidget_Tooltip.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <implot/implot.h>
#include <algorithm>
#include <cctype>
#include <cfloat>

namespace {

	using Category = ParticleModuleCategory;

	//=================================================================
	// 見た目の定義
	//=================================================================

	/// カテゴリごとの色（一覧のタグと追加メニューの見出しで使う）
	ImVec4 CategoryColor(Category c) {
		switch (c) {
		case Category::Emission:  return { 1.00f, 0.65f, 0.30f, 1.0f };
		case Category::Color:     return { 1.00f, 0.50f, 0.70f, 1.0f };
		case Category::Transform: return { 0.95f, 0.85f, 0.35f, 1.0f };
		case Category::Motion:    return { 0.40f, 0.85f, 1.00f, 1.0f };
		case Category::Force:     return { 0.70f, 0.55f, 1.00f, 1.0f };
		case Category::Look:      return { 0.45f, 0.90f, 0.50f, 1.0f };
		case Category::Child:     return { 0.55f, 0.65f, 1.00f, 1.0f };
		default:                  return { 0.65f, 0.65f, 0.65f, 1.0f };
		}
	}

	/// グラデーションの小さな帯を描く（一覧の要約欄用）
	void DrawGradientStrip(const ColorGradient& g, const ImVec2& size) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		constexpr int kSteps = 24;
		for (int i = 0; i < kSteps; ++i) {
			const float t0 = static_cast<float>(i) / kSteps;
			const float t1 = static_cast<float>(i + 1) / kSteps;
			const Vector4 a = g.Sample(t0);
			const Vector4 b = g.Sample(t1);
			const ImU32 ca = ImGui::ColorConvertFloat4ToU32({ a.x, a.y, a.z, a.w });
			const ImU32 cb = ImGui::ColorConvertFloat4ToU32({ b.x, b.y, b.z, b.w });
			dl->AddRectFilledMultiColor({ p.x + size.x * t0, p.y }, { p.x + size.x * t1, p.y + size.y }, ca, cb, cb, ca);
		}
		dl->AddRect(p, { p.x + size.x, p.y + size.y }, IM_COL32(90, 90, 100, 255));
		ImGui::Dummy(size);
	}

	//=================================================================
	// Undo 1 回分: 編集前後のシステム JSON を丸ごと持つ
	//=================================================================

	class SnapshotCommand : public ICommand {
	public:
		SnapshotCommand(std::string systemName, nlohmann::json before, nlohmann::json after,
			std::string label, std::function<void(const nlohmann::json&)> onApplied)
			: systemName_(std::move(systemName)), before_(std::move(before)), after_(std::move(after)),
			label_(std::move(label)), onApplied_(std::move(onApplied)) {}

		void Execute() override {
			// 履歴に積む時点で既に after の状態になっているので、初回は何もしない（Redo のときだけ適用）
			if (firstExecute_) { firstExecute_ = false; return; }
			Apply(after_);
		}
		void Undo() override { Apply(before_); }
		const char* GetName() const override { return label_.c_str(); }

	private:
		void Apply(const nlohmann::json& json) {
			// システムはマネージャに名前で取り直す（削除されていたら何もしない）
			if (auto* system = YParticleManager::GetInstance().GetSystem(systemName_)) {
				system->LoadFromJson(json);
				// インスペクタの「直近の状態」も合わせる（Undo 自体を新しい変更として積まないため）
				if (onApplied_) onApplied_(json);
			}
		}

		std::string    systemName_;
		nlohmann::json before_;
		nlohmann::json after_;
		std::string    label_;
		std::function<void(const nlohmann::json&)> onApplied_;
		bool firstExecute_ = true;
	};

	//=================================================================
	// モジュール一覧（追加メニュー用）
	//=================================================================

	struct CatalogEntry {
		std::string typeName;    // JSON の "type"
		std::string displayName; // GetName()
		std::string description; // 一行説明
		Category    category;
	};

	// 今の色を置き換えるか（前の色を消すか）。掛け合わせも「生成時の色 × 表」で今の色を置き換える
	bool IsColorOverwriter(IParticleModule::ColorWrite w) {
		return w == IParticleModule::ColorWrite::Overwrite || w == IParticleModule::ColorWrite::Multiply;
	}

	/// Factory に登録された全モジュール（カテゴリ順 → 表示名順）。初回だけ生成してキャッシュする
	const std::vector<CatalogEntry>& GetCatalog(bool spawn) {
		static std::vector<CatalogEntry> spawnCatalog;
		static std::vector<CatalogEntry> updateCatalog;
		auto& catalog = spawn ? spawnCatalog : updateCatalog;
		if (!catalog.empty()) return catalog;

		auto& factory = YParticleModuleFactory::GetInstance();
		const auto types = spawn ? factory.GetSpawnModuleTypes() : factory.GetUpdateModuleTypes();
		for (const auto& type : types) {
			std::shared_ptr<IParticleModule> m = spawn
				? std::static_pointer_cast<IParticleModule>(factory.CreateSpawnModule(type))
				: std::static_pointer_cast<IParticleModule>(factory.CreateUpdateModule(type));
			// 旧モジュール（置き換え先があるもの）は新しく追加させない。読み込みは引き続きできる
			if (YParticleModuleFactory::GetReplacement(type)) continue;
			if (m) catalog.push_back({ type, m->GetName(), YParticleModuleFactory::GetDescription(type), YParticleModuleFactory::GetCategory(type) });
		}
		std::sort(catalog.begin(), catalog.end(), [](const CatalogEntry& a, const CatalogEntry& b) {
			if (a.category != b.category) return a.category < b.category;
			return a.displayName < b.displayName;
			});
		return catalog;
	}

	/// 大文字小文字を無視した部分一致（英字のみ。日本語はそのまま比較）
	bool ContainsIgnoreCase(const std::string& text, const char* query) {
		if (!query || !query[0]) return true;
		auto lower = [](std::string s) {
			std::transform(s.begin(), s.end(), s.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
			};
		return lower(text).find(lower(query)) != std::string::npos;
	}

	/// 「+ モジュールを追加」ボタンと検索ポップアップ。選ばれたモジュールの型名を返す（未選択なら空）
	std::string DrawAddModuleButton(const char* popupId, bool spawn) {
		std::string picked;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.60f, 0.33f, 1.0f));
		if (ImGui::Button(ICON_FA_PLUS " 追加", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup(popupId);
		ImGui::PopStyleColor(2);

		ImGui::SetNextWindowSizeConstraints(ImVec2(300, 0), ImVec2(FLT_MAX, 460));
		if (ImGui::BeginPopup(popupId)) {
			static char filter[64] = "";
			if (ImGui::IsWindowAppearing()) {
				filter[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(-FLT_MIN);
			const bool enter = ImGui::InputTextWithHint("##filter", ICON_FA_SEARCH " 名前で検索（Enter で先頭を追加）",
				filter, sizeof(filter), ImGuiInputTextFlags_EnterReturnsTrue);

			const CatalogEntry* firstHit = nullptr;
			Category lastCategory = Category::Count;
			for (const auto& entry : GetCatalog(spawn)) {
				if (!ContainsIgnoreCase(entry.displayName, filter) && !ContainsIgnoreCase(entry.typeName, filter) &&
					!ContainsIgnoreCase(entry.description, filter)) continue;
				if (!firstHit) firstHit = &entry;

				// 検索していないときはカテゴリ見出しで区切る
				if (entry.category != lastCategory && filter[0] == '\0') {
					ImGui::PushStyleColor(ImGuiCol_Text, CategoryColor(entry.category));
					ImGui::SeparatorText(YParticleModuleFactory::GetCategoryName(entry.category));
					ImGui::PopStyleColor();
					lastCategory = entry.category;
				}

				ImGui::PushID(entry.typeName.c_str());
				if (ImGui::Selectable(entry.displayName.c_str())) picked = entry.typeName;
				ImGui::SameLine();
				ImGui::TextDisabled("%s", entry.description.c_str());
				YEditorWidget::ItemTooltip(entry.typeName.c_str());
				ImGui::PopID();
			}
			if (!firstHit) ImGui::TextDisabled("一致するモジュールがありません");

			if (enter && firstHit) picked = firstHit->typeName;
			if (!picked.empty()) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		return picked;
	}

	//=================================================================
	// Spawn / Update の違いを吸収するヘルパー
	//=================================================================

	size_t ModuleCount(const YParticleSystem& s, bool spawn) {
		return spawn ? s.GetSpawnModules().size() : s.GetUpdateModules().size();
	}

	IParticleModule* ModuleAt(const YParticleSystem& s, bool spawn, int index) {
		if (index < 0 || static_cast<size_t>(index) >= ModuleCount(s, spawn)) return nullptr;
		return spawn ? static_cast<IParticleModule*>(s.GetSpawnModules()[index].get())
			: static_cast<IParticleModule*>(s.GetUpdateModules()[index].get());
	}

	/// 型名から新しいモジュールを作り、index の位置へ差し込む（data があればその値で初期化）
	bool InsertNewModule(YParticleSystem& s, bool spawn, size_t at, const std::string& type,
		const nlohmann::json* data, bool enabled, const std::string& label = {}) {
		auto& factory = YParticleModuleFactory::GetInstance();
		if (spawn) {
			auto m = factory.CreateSpawnModule(type);
			if (!m) return false;
			if (data) m->LoadFromJson(*data);
			m->SetEnabled(enabled);
			m->SetLabel(label);
			s.InsertSpawnModule(at, m);
		} else {
			auto m = factory.CreateUpdateModule(type);
			if (!m) return false;
			if (data) m->LoadFromJson(*data);
			m->SetEnabled(enabled);
			m->SetLabel(label);
			s.InsertUpdateModule(at, m);
		}
		return true;
	}

	void DeleteModuleAt(YParticleSystem& s, bool spawn, int index) {
		if (index < 0 || static_cast<size_t>(index) >= ModuleCount(s, spawn)) return;
		if (spawn) s.DeleteSpawnModule(s.GetSpawnModules()[index]);
		else       s.DeleteUpdateModule(s.GetUpdateModules()[index]);
	}

} // namespace

//=================================================================
// 描画の入口
//=================================================================

void YParticleSystemInspector::Draw(YParticleSystem& system, Layout layout) {
	Bind(system);
	layout_ = layout;
	// 文字入力中の Ctrl+Z は入力欄自身の取り消しに使われるので、システムの Undo は走らせない
	if (!ImGui::GetIO().WantTextInput) history_.HandleKeyInput();

	if (layout == Layout::SideBySide) {
		// 左: スタック / 右: インスペクタ（境界はドラッグで幅を変えられる）
		ImGui::BeginChild("##stack", ImVec2(stackWidth_, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
		stackWidth_ = ImGui::GetWindowWidth();
		DrawStack(system);
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##inspector", ImVec2(0, 0), ImGuiChildFlags_Borders);
		DrawInspector(system);
		ImGui::EndChild();
	} else {
		DrawStack(system);
		ImGui::Separator();
		DrawInspector(system);
	}

	// 描画が終わってから構造変更を反映し、変化があれば履歴に積む
	if (pendingOp_.apply) {
		pendingOp_.apply(system);
		pendingOp_.apply = nullptr;
	}
	TrackChanges(system);
}

void YParticleSystemInspector::Bind(YParticleSystem& system) {
	if (boundSystem_ == &system && boundName_ == system.GetName()) return;

	// 別のシステムに切り替わった: 履歴と選択をリセットし、今の状態を「保存済み」とみなす
	boundSystem_ = &system;
	boundName_ = system.GetName();
	history_.Clear();
	lastSnapshot_ = nlohmann::json{};
	system.SaveToJson(lastSnapshot_);
	savedSnapshot_ = lastSnapshot_;
	selection_ = {};
	pendingOp_ = {};
	aliveHistory_.fill(0.0f);
}

void YParticleSystemInspector::TrackChanges(YParticleSystem& system) {
	// ドラッグやテキスト入力の途中は確定を待つ（1 回のドラッグ = 1 回の Undo にするため）
	if (ImGui::IsAnyItemActive()) return;

	nlohmann::json now;
	system.SaveToJson(now);
	if (now == lastSnapshot_) return;

	std::string label = pendingOp_.label.empty() ? std::string("値の変更") : pendingOp_.label;
	pendingOp_.label.clear();

	// Undo / Redo 適用後は、システムから書き出し直したものを「直近の状態」にする。
	// （読み込み時に値を整えるモジュールがあっても、Undo 直後に差分が出て Redo が消えないように）
	history_.Execute(std::make_unique<SnapshotCommand>(
		boundName_, lastSnapshot_, now, std::move(label),
		[this](const nlohmann::json&) {
			if (auto* sys = YParticleManager::GetInstance().GetSystem(boundName_)) {
				lastSnapshot_ = nlohmann::json{};
				sys->SaveToJson(lastSnapshot_);
			}
		}));
	lastSnapshot_ = std::move(now);
}

//=================================================================
// スタック（左）
//=================================================================

//=================================================================
// 組み合わせの警告（「入れたのに効いていない」を作る人に知らせる）
//=================================================================

void YParticleSystemInspector::ComputeWarnings(const YParticleSystem& system) {
	using Factory = YParticleModuleFactory;
	const auto& spawns = system.GetSpawnModules();
	const auto& updates = system.GetUpdateModules();
	spawnWarnings_.assign(spawns.size(), {});
	updateWarnings_.assign(updates.size(), {});
	missingVelocityIntegrator_ = false;

	// ── 速度: 位置へ反映するのは「速度で移動」だけ ──
	int  integrator = -1;
	bool fixedVelocity = false;
	for (size_t i = 0; i < updates.size(); ++i) {
		const auto& m = updates[i];
		if (m->IsEnabled() && m->GetTypeName() == "UpdateVelocity") {
			integrator = static_cast<int>(i);
			fixedVelocity |= m->OverwritesVelocity();
		}
	}
	bool anyVelocityUser = false;
	auto checkVelocity = [&](const auto& list, std::vector<std::vector<std::string>>& out) {
		for (size_t i = 0; i < list.size(); ++i) {
			const auto& m = list[i];
			if (!m->IsEnabled() || !Factory::UsesVelocity(m->GetTypeName())) continue;
			anyVelocityUser = true;
			if (integrator < 0) {
				out[i].push_back("毎フレームの「速度で移動」が無いため、この速度は位置に反映されません（粒が動きません）");
			} else if (fixedVelocity) {
				out[i].push_back("「速度で移動」が固定速度モードのため、この速度は毎フレーム上書きされて効きません");
			}
		}
	};
	checkVelocity(spawns, spawnWarnings_);
	checkVelocity(updates, updateWarnings_);
	missingVelocityIntegrator_ = (integrator < 0 && anyVelocityUser);
	if (integrator >= 0 && fixedVelocity && anyVelocityUser) {
		updateWarnings_[integrator].push_back("固定速度モードです。生成時の速度・重力・乱流などの速度系モジュールが効きません（「生成時の速度を使う」を ON に）");
	}

	// ── 色: 後ろのモジュールに上書きされて消えていないか ──
	// 生成時の色 → 毎フレームの「上書き」系（掛け合わせは生成時の色を使うので消えない）
	for (size_t i = 0; i < spawns.size(); ++i) {
		const auto& m = spawns[i];
		if (!m->IsEnabled() || m->GetColorWrite() == IParticleModule::ColorWrite::None) continue;
		for (const auto& u : updates) {
			if (!u->IsEnabled() || u->GetColorWrite() != IParticleModule::ColorWrite::Overwrite) continue;
			std::string text = "毎フレームの「" + u->GetDisplayName() + "」が色を上書きするため、この色は消えます";
			if (u->GetTypeName() == "UpdateColorOverLifetime" || u->GetTypeName() == "UpdateColorGradient") {
				text += "（そちらの「生成時の色を掛ける」を ON にすると活かせます）";
			}
			spawnWarnings_[i].push_back(text);
			break;
		}
	}
	// 毎フレームの色 → それより後ろの「上書き / 掛け合わせ」系（どちらも今の色を置き換える）
	for (size_t i = 0; i < updates.size(); ++i) {
		const auto& m = updates[i];
		if (!m->IsEnabled() || m->GetColorWrite() == IParticleModule::ColorWrite::None) continue;
		for (size_t j = i + 1; j < updates.size(); ++j) {
			const auto& later = updates[j];
			if (!later->IsEnabled() || !IsColorOverwriter(later->GetColorWrite())) continue;
			updateWarnings_[i].push_back("後ろの「" + later->GetDisplayName() + "」が色を上書きするため、この効果は消えています（順番を入れ替えてください）");
			break;
		}
	}
}

void YParticleSystemInspector::DrawStack(YParticleSystem& system) {
	HandleShortcuts(system);
	ComputeWarnings(system);

	// システム設定（テクスチャ・ブレンドなど）も 1 行として扱う
	{
		const bool selected = (selection_.kind == SelectionKind::Settings);
		std::string label = std::string(ICON_FA_COG " システム設定");
		if (ImGui::Selectable(label.c_str(), selected)) selection_ = { SelectionKind::Settings, -1 };
		ImGui::SameLine();
		const std::string& tex = system.GetTextureFilePath();
		const size_t slash = tex.find_last_of("/\\");
		ImGui::TextDisabled("%s", tex.empty() ? "(テクスチャなし)" : tex.substr(slash == std::string::npos ? 0 : slash + 1).c_str());
	}

	if (system.GetUnknownModuleCount() > 0) {
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), ICON_FA_EXCLAMATION_TRIANGLE " 未登録のモジュール %zu 個",
			system.GetUnknownModuleCount());
		YEditorWidget::ItemTooltip("JSON にあるが、エンジンに存在しない型のモジュールです。\n"
			"動作はしませんが、保存しても消えずに残ります（ログに型名を出しています）。");
	}

	DrawModuleSection(system, SelectionKind::Spawn);
	DrawModuleSection(system, SelectionKind::Update);

	ImGui::Spacing();
	DrawStats(system);
}

void YParticleSystemInspector::DrawModuleSection(YParticleSystem& system, SelectionKind kind) {
	const bool spawn = (kind == SelectionKind::Spawn);
	const char* payloadType = spawn ? "YPMOD_SPAWN" : "YPMOD_UPDATE";

	ImGui::PushID(spawn ? "spawn" : "update");
	ImGui::SeparatorText(spawn ? "発生したとき（Spawn）" : "毎フレーム（Update）");
	YEditorWidget::ItemTooltip(spawn
		? "粒が生まれた瞬間に 1 回だけ、上から順に実行されます"
		: "生きている粒に毎フレーム、上から順に実行されます（順番で結果が変わることがあります）");

	const int count = static_cast<int>(ModuleCount(system, spawn));
	if (count == 0) ImGui::TextDisabled("  (なし)");

	if (count > 0 && ImGui::BeginTable("##modules", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX)) {
		ImGui::TableSetupColumn("##on", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##summary", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.5f);

		for (int i = 0; i < count; ++i) {
			IParticleModule* module = ModuleAt(system, spawn, i);
			if (!module) continue;
			const std::string type = module->GetTypeName();
			const Category category = YParticleModuleFactory::GetCategory(type);

			ImGui::PushID(i);
			ImGui::TableNextRow();

			// 有効チェック（外すと削除せずに効果だけ止まる。比較に便利）
			ImGui::TableSetColumnIndex(0);
			bool enabled = module->IsEnabled();
			if (ImGui::Checkbox("##enabled", &enabled)) module->SetEnabled(enabled);
			YEditorWidget::ItemTooltip("有効 / 無効（無効にすると削除せずに効果だけ止まる）");

			// カテゴリ色の印 + 名前（行全体が選択範囲）
			ImGui::TableSetColumnIndex(1);
			ImGui::AlignTextToFramePadding();
			// 警告があればカテゴリ印の代わりに注意アイコンを出す
			const auto& warnings = spawn ? spawnWarnings_ : updateWarnings_;
			const bool hasWarning = (i < static_cast<int>(warnings.size())) && !warnings[i].empty();
			if (hasWarning) {
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), ICON_FA_EXCLAMATION_TRIANGLE);
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
					ImGui::BeginTooltip();
					for (const auto& w : warnings[i]) ImGui::BulletText("%s", w.c_str());
					ImGui::EndTooltip();
				}
			} else {
				ImGui::TextColored(CategoryColor(category), "■");
			}
			ImGui::SameLine(0, 4);
			const bool selected = (selection_.kind == kind && selection_.index == i);
			if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			if (ImGui::Selectable(module->GetDisplayName().c_str(), selected,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
				selection_ = { kind, i };
			}
			if (!enabled) ImGui::PopStyleColor();
			YEditorWidget::ItemTooltip((module->GetName() + "（" + YParticleModuleFactory::GetCategoryName(category) + " / " + type + "）\n" +
				YParticleModuleFactory::GetDescription(type)).c_str());

			// ドラッグで並べ替え（同じ区分の中だけ）
			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload(payloadType, &i, sizeof(int));
				ImGui::Text(ICON_FA_ARROWS_ALT_V " %s", module->GetDisplayName().c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType)) {
					RequestMove(kind, *static_cast<const int*>(payload->Data), i);
				}
				ImGui::EndDragDropTarget();
			}

			// 右クリックメニュー
			if (ImGui::BeginPopupContextItem("##rowMenu")) {
				selection_ = { kind, i };
				if (ImGui::MenuItem(ICON_FA_CLONE " 複製", "Ctrl+D")) RequestDuplicate(system, kind, i);
				if (ImGui::MenuItem(ICON_FA_ARROW_UP " 上へ", "Ctrl+↑", false, i > 0)) RequestMove(kind, i, i - 1);
				if (ImGui::MenuItem(ICON_FA_ARROW_DOWN " 下へ", "Ctrl+↓", false, i + 1 < count)) RequestMove(kind, i, i + 1);
				if (ImGui::MenuItem(ICON_FA_UNDO " 既定値に戻す")) RequestReset(system, kind, i);
				ImGui::Separator();
				if (ImGui::MenuItem(ICON_FA_TRASH " 削除", "Del")) RequestDelete(system, kind, i);
				ImGui::EndPopup();
			}

			// 要約（色帯 or 一行テキスト）
			ImGui::TableSetColumnIndex(2);
			if (const ColorGradient* gradient = module->GetPreviewGradient()) {
				const float h = ImGui::GetTextLineHeight() * 0.8f;
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetFrameHeight() - h) * 0.5f);
				DrawGradientStrip(*gradient, ImVec2(ImGui::GetContentRegionAvail().x, h));
			} else {
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("%s", module->GetSummary().c_str());
			}

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	// 追加（Factory に登録された全モジュールから検索）
	const std::string picked = DrawAddModuleButton("##addModule", spawn);
	if (!picked.empty()) {
		const size_t at = ModuleCount(system, spawn);
		pendingOp_.apply = [this, spawn, kind, picked, at](YParticleSystem& s) {
			if (InsertNewModule(s, spawn, at, picked, nullptr, true)) selection_ = { kind, static_cast<int>(at) };
			};
		pendingOp_.label = "追加: " + picked;
	}

	ImGui::PopID();
}

void YParticleSystemInspector::HandleShortcuts(YParticleSystem& system) {
	// スタック自身のウィンドウにフォーカスがあり、文字入力中でないときだけ効かせる。
	// （インスペクタ側の Delete はグラデーションのマーカー削除などに使われるため、そちらへは効かせない。
	//   縦積みレイアウトはスタック専用ウィンドウが無いので、ショートカット自体を無効にする）
	if (layout_ != Layout::SideBySide) return;
	if (!ImGui::IsWindowFocused() || ImGui::GetIO().WantTextInput) return;
	if (selection_.kind == SelectionKind::Settings || selection_.index < 0) return;

	const bool spawn = (selection_.kind == SelectionKind::Spawn);
	const int  count = static_cast<int>(ModuleCount(system, spawn));
	const int  i = selection_.index;
	if (i >= count) return;

	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) RequestDelete(system, selection_.kind, i);
	else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)) RequestDuplicate(system, selection_.kind, i);
	else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_UpArrow) && i > 0) RequestMove(selection_.kind, i, i - 1);
	else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_DownArrow) && i + 1 < count) RequestMove(selection_.kind, i, i + 1);
}

//=================================================================
// 構造変更のリクエスト（描画が終わってから実行される）
//=================================================================

void YParticleSystemInspector::RequestMove(SelectionKind kind, int from, int to) {
	if (from == to) return;
	const bool spawn = (kind == SelectionKind::Spawn);
	pendingOp_.apply = [this, spawn, kind, from, to](YParticleSystem& s) {
		if (spawn) s.MoveSpawnModule(from, to);
		else       s.MoveUpdateModule(from, to);
		selection_ = { kind, to };
		};
	pendingOp_.label = "並べ替え";
}

void YParticleSystemInspector::RequestDuplicate(YParticleSystem& system, SelectionKind kind, int index) {
	const bool spawn = (kind == SelectionKind::Spawn);
	const IParticleModule* module = ModuleAt(system, spawn, index);
	if (!module) return;

	// 今の値を JSON に写して同じ型を作り、元の直後に置く
	nlohmann::json data;
	module->SaveToJson(data);
	const std::string type = module->GetTypeName();
	const bool enabled = module->IsEnabled();
	const std::string label = module->GetDisplayName();
	pendingOp_.apply = [this, spawn, kind, index, type, data, enabled, label](YParticleSystem& s) {
		if (InsertNewModule(s, spawn, index + 1, type, &data, enabled, label)) selection_ = { kind, index + 1 };
		};
	pendingOp_.label = "複製: " + label;
}

void YParticleSystemInspector::RequestReset(YParticleSystem& system, SelectionKind kind, int index) {
	const bool spawn = (kind == SelectionKind::Spawn);
	const IParticleModule* module = ModuleAt(system, spawn, index);
	if (!module) return;

	const std::string type = module->GetTypeName();
	const bool enabled = module->IsEnabled();
	const std::string label = module->GetDisplayName(); // 付けた名前は値と違って残す
	pendingOp_.apply = [spawn, index, type, enabled, label](YParticleSystem& s) {
		DeleteModuleAt(s, spawn, index);
		InsertNewModule(s, spawn, index, type, nullptr, enabled, label);
		};
	pendingOp_.label = "既定値に戻す: " + label;
}

void YParticleSystemInspector::RequestDelete(YParticleSystem& system, SelectionKind kind, int index) {
	const bool spawn = (kind == SelectionKind::Spawn);
	const IParticleModule* module = ModuleAt(system, spawn, index);
	if (!module) return;

	pendingOp_.apply = [this, spawn, kind, index](YParticleSystem& s) {
		DeleteModuleAt(s, spawn, index);
		// 削除後は 1 つ上（無ければシステム設定）を選ぶ。Undo で戻せるので確認は出さない
		const int remain = static_cast<int>(ModuleCount(s, spawn));
		selection_ = (remain > 0) ? Selection{ kind, (std::min)(index, remain - 1) } : Selection{};
		};
	pendingOp_.label = "削除: " + module->GetDisplayName();
}

//=================================================================
// インスペクタ（右）
//=================================================================

void YParticleSystemInspector::DrawInspector(YParticleSystem& system) {
	if (selection_.kind == SelectionKind::Settings) {
		ImGui::SeparatorText((std::string(ICON_FA_COG " システム設定: ") + system.GetName()).c_str());
		system.DrawSettingsEditor();
		return;
	}

	const bool spawn = (selection_.kind == SelectionKind::Spawn);
	IParticleModule* module = ModuleAt(system, spawn, selection_.index);
	if (!module) {
		// Undo などで選択先が消えた
		selection_ = {};
		ImGui::TextDisabled("左の一覧から項目を選んでください");
		return;
	}

	const std::string type = module->GetTypeName();
	const Category category = YParticleModuleFactory::GetCategory(type);

	// 見出し: カテゴリ / 名前 / 型名 / タイミング
	ImGui::TextColored(CategoryColor(category), "■ %s", YParticleModuleFactory::GetCategoryName(category));
	ImGui::SameLine();
	// 表示名（作者が付ける名前。例: 「爆発初速」）。空にすると既定の名前に戻る
	{
		char nameBuf[128];
		strncpy_s(nameBuf, module->GetDisplayName().c_str(), _TRUNCATE);
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
		if (ImGui::InputText("##label", nameBuf, sizeof(nameBuf))) module->SetLabel(nameBuf);
		YEditorWidget::ItemTooltip("表示名（一覧に出る名前）。空にすると既定の名前に戻ります");
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s / %s", module->GetName().c_str(), spawn ? "発生時" : "毎フレーム");
	YEditorWidget::ItemTooltip(type.c_str());
	if (const char* desc = YParticleModuleFactory::GetDescription(type); desc && desc[0]) {
		ImGui::TextDisabled("%s", desc);
	}

	// 旧モジュール: 推奨モジュールへその場で変換できる（値・表示名・有効状態は引き継ぐ）
	if (const char* replacement = YParticleModuleFactory::GetReplacement(type)) {
		const std::string replacementType = replacement;
		std::string replacementName = replacementType;
		for (const auto& entry : GetCatalog(spawn)) {
			if (entry.typeName == replacementType) replacementName = entry.displayName;
		}
		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), ICON_FA_INFO_CIRCLE " 旧モジュールです。「%s」に変換できます", replacementName.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("変換する")) {
			nlohmann::json data;
			module->SaveToJson(data);
			const int index = selection_.index;
			const bool en = module->IsEnabled();
			const std::string label = (module->GetDisplayName() == module->GetName()) ? std::string{} : module->GetDisplayName();
			pendingOp_.apply = [spawn, index, replacementType, data, en, label](YParticleSystem& s) {
				DeleteModuleAt(s, spawn, index);
				InsertNewModule(s, spawn, index, replacementType, &data, en, label);
				};
			pendingOp_.label = "変換: " + replacementName;
		}
	}

	// 組み合わせの警告とクイック修正
	const auto& warnings = spawn ? spawnWarnings_ : updateWarnings_;
	if (selection_.index < static_cast<int>(warnings.size()) && !warnings[selection_.index].empty()) {
		for (const auto& w : warnings[selection_.index]) {
			ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), ICON_FA_EXCLAMATION_TRIANGLE " %s", w.c_str());
		}
		if (missingVelocityIntegrator_ && ImGui::SmallButton(ICON_FA_PLUS " 「速度で移動」を追加して直す")) {
			pendingOp_.apply = [](YParticleSystem& s) {
				InsertNewModule(s, /*spawn=*/false, 0, "UpdateVelocity", nullptr, true);
				};
			pendingOp_.label = "追加: 速度で移動";
		}
	}

	bool enabled = module->IsEnabled();
	if (ImGui::Checkbox("有効", &enabled)) module->SetEnabled(enabled);
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_CLONE " 複製")) RequestDuplicate(system, selection_.kind, selection_.index);
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_UNDO " 既定値")) RequestReset(system, selection_.kind, selection_.index);
	YEditorWidget::ItemTooltip("このモジュールの値を作成直後の状態に戻す");
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
	if (ImGui::Button(ICON_FA_TRASH " 削除")) RequestDelete(system, selection_.kind, selection_.index);
	ImGui::PopStyleColor();
	ImGui::Separator();

	ImGui::BeginDisabled(!enabled);
	ImGui::PushID(module);
	module->DrawEditor();
	ImGui::PopID();
	ImGui::EndDisabled();
}

//=================================================================
// 統計（粒子数の推移）
//=================================================================

void YParticleSystemInspector::DrawStats(const YParticleSystem& system) {
	int alive = 0;
	for (const auto& a : system.GetAttributes()) {
		if (a.isActive) ++alive;
	}
	aliveHistory_[aliveHead_] = static_cast<float>(alive);
	aliveHead_ = (aliveHead_ + 1) % kStatsSamples;

	const float maxParticles = static_cast<float>(system.GetMaxParticles());
	const bool  nearFull = alive >= static_cast<int>(maxParticles * 0.95f);
	ImGui::SeparatorText("粒子数");
	ImGui::TextColored(nearFull ? ImVec4(1.0f, 0.5f, 0.3f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text),
		"生存 %d / %u", alive, system.GetMaxParticles());
	if (nearFull) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), ICON_FA_EXCLAMATION_TRIANGLE);
		YEditorWidget::ItemTooltip("最大数に近いため、新しい粒が出せなくなっています。最大数を増やすか発生数を減らしてください");
	}

	if (ImPlot::BeginPlot("##alive", ImVec2(-FLT_MIN, ImGui::GetFontSize() * 5.0f), ImPlotFlags_CanvasOnly)) {
		ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoTickLabels);
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, kStatsSamples - 1, ImPlotCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0, (std::max)(maxParticles, 1.0f), ImPlotCond_Always);

		ImPlotSpec spec;
		spec.Offset = aliveHead_; // 最も古いサンプルから描く
		spec.FillAlpha = 0.3f;
		ImPlot::PlotShaded("##aliveFill", aliveHistory_.data(), kStatsSamples, 0.0, 1.0, 0.0, spec);
		ImPlot::PlotLine("##aliveLine", aliveHistory_.data(), kStatsSamples, 1.0, 0.0, spec);
		ImPlot::EndPlot();
	}
}

#endif // USE_IMGUI

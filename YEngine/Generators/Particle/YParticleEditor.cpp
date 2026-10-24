#ifdef USE_IMGUI

#include "YParticleEditor.h"
#include "YEmitterGroupEditor.h"
#include "YParticleManager.h"
#include "YParticleSystem.h"
#include "Core/Editor/Widgets/YEditorWidget_Notify.h"
#include "Core/Editor/Widgets/YEditorWidget_Tooltip.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <filesystem>
#include <fstream>

namespace {

	/// 名前で探す（大文字小文字を無視、英字のみ）
	bool MatchFilter(const std::string& name, const char* filter) {
		if (!filter || !filter[0]) return true;
		std::string a = name, b = filter;
		std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a.find(b) != std::string::npos;
	}

	/// パスからファイル名だけを取り出す（一覧のツールチップ用）
	std::string FileNameOf(const std::string& path) {
		return std::filesystem::path(path).filename().string();
	}

	/// ファイルブラウザ入りのモーダル。onSaveAs を渡すと「新しい名前で保存」欄が付く
	void DrawBrowserPopup(const char* popupId, bool& showFlag, const char* title,
		YoRigine::FileBrowser& browser, char* nameBuf, size_t nameBufSize,
		const std::function<void(const std::string&)>& onSaveAs) {
		if (showFlag && !ImGui::IsPopupOpen(popupId)) ImGui::OpenPopup(popupId);

		ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Appearing);
		if (ImGui::BeginPopupModal(popupId, &showFlag, ImGuiWindowFlags_NoTitleBar)) {
			ImGui::TextUnformatted(title);
			ImGui::Separator();

			const float footer = ImGui::GetFrameHeightWithSpacing() * (onSaveAs ? 2.0f : 1.0f) + ImGui::GetStyle().ItemSpacing.y;
			browser.Draw("##browser", ImVec2(0, -footer));

			if (onSaveAs) {
				ImGui::SetNextItemWidth(-140);
				const bool enter = ImGui::InputTextWithHint("##saveAs", "新しいファイル名.json", nameBuf, static_cast<int>(nameBufSize),
					ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::SameLine();
				if ((ImGui::Button(ICON_FA_SAVE " この名前で保存", ImVec2(-FLT_MIN, 0)) || enter) && nameBuf[0] != '\0') {
					std::string name = nameBuf;
					if (std::filesystem::path(name).extension() != ".json") name += ".json";
					onSaveAs(browser.GetCurrentDir() + name);
					showFlag = false;
				}
			}

			if (ImGui::Button("キャンセル", ImVec2(-FLT_MIN, 0))) showFlag = false;
			if (!showFlag) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}

} // namespace

//=================================================================
// 初期化
//=================================================================

YParticleEditor::YParticleEditor()
	: saveAsBrowser_(jsonDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
	, loadBrowser_(jsonDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
	, saveAllBrowser_(jsonDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List) {

	// 既存ファイルをクリック = そこへ上書き保存
	saveAsBrowser_.SetOnFileSelected([this](const std::string& path) {
		if (auto* system = GetSelectedSystemPtr()) SaveSystemIntoFile(*system, path);
		showSaveAsPopup_ = false;
		});
	loadBrowser_.SetOnFileSelected([this](const std::string& path) {
		LoadFile(path);
		showLoadPopup_ = false;
		});
	saveAllBrowser_.SetOnFileSelected([this](const std::string& path) {
		SaveAllSystemsToFile(path);
		showSaveAllPopup_ = false;
		});
}

//=================================================================
// メイン
//=================================================================

void YParticleEditor::ShowEditorWindow() {
	YParticleSystem* system = GetSelectedSystemPtr();

	HandleShortcuts(system);
	UpdateTestEmit(system);

	DrawMenuBar();

	if (ImGui::BeginTabBar("##particleEditorTabs")) {
		if (ImGui::BeginTabItem(ICON_FA_FIRE " パーティクルシステム")) {
			DrawToolbar(system);
			ImGui::Separator();

			// 左: システム一覧（幅はドラッグで変更可）
			ImGui::BeginChild("##systemList", ImVec2(listWidth_, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
			listWidth_ = ImGui::GetWindowWidth();
			DrawSystemList();
			ImGui::EndChild();

			ImGui::SameLine();

			// 右: スタック＋インスペクタ
			ImGui::BeginChild("##systemEdit", ImVec2(0, 0));
			if (system = GetSelectedSystemPtr(); system) {
				GetInspector(system->GetName()).Draw(*system, YParticleSystemInspector::Layout::SideBySide);
			} else {
				ImGui::Spacing();
				ImGui::TextDisabled("左の一覧からシステムを選ぶか、「+ 新規」で作成してください。");
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(ICON_FA_LAYER_GROUP " エミッタグループ")) {
			YEmitterGroupEditor::GetInstance().ShowEditor();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	// どのタブを開いていてもダイアログが出るよう、タブの外で毎フレーム描く
	DrawPopups();
}

void YParticleEditor::DrawMenuBar() {
	if (!ImGui::BeginMenuBar()) return;

	if (ImGui::BeginMenu("ファイル")) {
		const bool hasSel = GetSelectedSystemPtr() != nullptr;
		if (ImGui::MenuItem(ICON_FA_SAVE " 保存", "Ctrl+S", false, hasSel)) SaveSelected();
		if (ImGui::MenuItem("名前を付けて保存...", nullptr, false, hasSel)) OpenSaveAs(selectedSystemName_ + ".json");
		if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " 開く...")) {
			loadBrowser_.Scan();
			showLoadPopup_ = true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("全システムを 1 ファイルに保存...")) {
			saveAllBrowser_.Scan();
			saveAsNameBuf_[0] = '\0';
			showSaveAllPopup_ = true;
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("編集")) {
		if (YParticleSystem* system = GetSelectedSystemPtr()) {
			auto& history = GetInspector(system->GetName()).GetHistory();
			if (ImGui::MenuItem(ICON_FA_UNDO " 元に戻す", "Ctrl+Z", false, history.CanUndo())) history.Undo();
			if (ImGui::MenuItem(ICON_FA_REDO " やり直し", "Ctrl+Y", false, history.CanRedo())) history.Redo();
		} else {
			ImGui::MenuItem("（システム未選択）", nullptr, false, false);
		}
		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

void YParticleEditor::DrawToolbar(YParticleSystem* system) {
	ImGui::BeginDisabled(system == nullptr);

	// 保存 / Undo / Redo
	if (ImGui::Button(ICON_FA_SAVE)) SaveSelected();
	YEditorWidget::ItemTooltip("保存 (Ctrl+S)\n読み込み元のファイルへ上書きします");
	ImGui::SameLine();

	CommandHistory* history = system ? &GetInspector(system->GetName()).GetHistory() : nullptr;
	ImGui::BeginDisabled(!history || !history->CanUndo());
	if (ImGui::Button(ICON_FA_UNDO) && history) history->Undo();
	ImGui::EndDisabled();
	YEditorWidget::ItemTooltip("元に戻す (Ctrl+Z)");
	ImGui::SameLine();
	ImGui::BeginDisabled(!history || !history->CanRedo());
	if (ImGui::Button(ICON_FA_REDO) && history) history->Redo();
	ImGui::EndDisabled();
	YEditorWidget::ItemTooltip("やり直し (Ctrl+Y)");

	// テスト発生
	ImGui::SameLine(0, 16);
	ImGui::TextDisabled("|");
	ImGui::SameLine(0, 16);
	if (ImGui::Button(ICON_FA_PLAY " 発生") && system) system->Emit(testEmitPos_, testEmitCount_);
	YEditorWidget::ItemTooltip("このシステムの粒をその場で出して見た目を確認する");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.0f);
	ImGui::DragInt("##count", &testEmitCount_, 1.0f, 1, 5000, "%d 個");
	ImGui::SameLine();
	ImGui::Checkbox(ICON_FA_SYNC " 連続", &testEmitLoop_);
	YEditorWidget::ItemTooltip("一定間隔で発生し続ける（パラメータを触りながら確認する用）");
	if (testEmitLoop_) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.0f);
		ImGui::DragFloat("##interval", &testEmitInterval_, 0.01f, 0.02f, 5.0f, "%.2f 秒");
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
	ImGui::DragFloat3("##pos", &testEmitPos_.x, 0.05f);
	YEditorWidget::ItemTooltip("発生位置（ワールド座標）");

	ImGui::EndDisabled();

	// 保存先と未保存の表示
	if (system) {
		ImGui::SameLine(0, 16);
		const bool dirty = GetInspector(system->GetName()).IsDirty();
		if (dirty) ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "* 未保存");
		else       ImGui::TextDisabled("保存済み");
		const std::string& src = system->GetSourceFilePath();
		YEditorWidget::ItemTooltip(src.empty() ? "保存先: 未設定（保存時に名前を決めます）" : ("保存先: " + src).c_str());
	}
}

void YParticleEditor::DrawSystemList() {
	auto& manager = YParticleManager::GetInstance();

	if (ImGui::Button(ICON_FA_PLUS " 新規")) {
		const std::string uniq = MakeUniqueSystemName("NewParticle");
		strncpy_s(newNameBuffer_, uniq.c_str(), _TRUNCATE);
		newMaxParticles_ = 1000;
		showNewDialog_ = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##filter", ICON_FA_SEARCH " 検索", listFilter_, sizeof(listFilter_));
	ImGui::Separator();

	auto names = manager.GetAllSystemNames();
	std::sort(names.begin(), names.end());

	for (const auto& name : names) {
		if (!MatchFilter(name, listFilter_)) continue;

		auto* system = manager.GetSystem(name);
		auto it = inspectors_.find(name);
		const bool dirty = (it != inspectors_.end()) && it->second->IsDirty();

		std::string label = dirty ? (name + " *") : name;
		if (ImGui::Selectable(label.c_str(), name == selectedSystemName_)) selectedSystemName_ = name;
		if (system) {
			const std::string& src = system->GetSourceFilePath();
			YEditorWidget::ItemTooltip(src.empty() ? "（未保存の新規システム）" : src.c_str());
		}

		if (ImGui::BeginPopupContextItem()) {
			selectedSystemName_ = name;
			if (ImGui::MenuItem(ICON_FA_SAVE " 保存")) SaveSelected();
			if (ImGui::MenuItem(ICON_FA_CLONE " 複製") && system) {
				// JSON に写して別名のシステムを作る（保存先は未設定 = 新規扱い）
				nlohmann::json json;
				system->SaveToJson(json);
				const std::string newName = MakeUniqueSystemName(name + "_copy");
				if (auto* copy = manager.CreateSystem(newName, system->GetMaxParticles())) {
					copy->LoadFromJson(json);
					selectedSystemName_ = newName;
					YEditorWidget::NotifyInfo("複製しました: %s", newName.c_str());
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_TRASH " 削除...")) showDeleteConfirm_ = true;
			ImGui::EndPopup();
		}
	}
	if (names.empty()) ImGui::TextDisabled("(システムなし)");
}

void YParticleEditor::DrawPopups() {
	// ── 新規作成 ───────────────────────────────────────────
	if (showNewDialog_ && !ImGui::IsPopupOpen("新規パーティクルシステム")) ImGui::OpenPopup("新規パーティクルシステム");
	if (ImGui::BeginPopupModal("新規パーティクルシステム", &showNewDialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::InputText("名前", newNameBuffer_, sizeof(newNameBuffer_));
		ImGui::InputInt("最大粒子数", &newMaxParticles_, 100, 1000);
		newMaxParticles_ = std::clamp(newMaxParticles_, 1, 200000);

		const bool exists = YParticleManager::GetInstance().GetSystem(newNameBuffer_) != nullptr;
		if (exists) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "同じ名前のシステムがあります");

		ImGui::BeginDisabled(newNameBuffer_[0] == '\0' || exists);
		if (ImGui::Button("作成", ImVec2(120, 0))) {
			YParticleManager::GetInstance().CreateSystem(newNameBuffer_, static_cast<uint32_t>(newMaxParticles_));
			selectedSystemName_ = newNameBuffer_;
			showNewDialog_ = false;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120, 0))) showNewDialog_ = false;
		if (!showNewDialog_) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	// ── 削除確認（システムの削除は Undo できないので確認する。ファイルは消さない）──
	if (showDeleteConfirm_ && !ImGui::IsPopupOpen("システムの削除")) ImGui::OpenPopup("システムの削除");
	if (ImGui::BeginPopupModal("システムの削除", &showDeleteConfirm_, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("「%s」をメモリから削除しますか？", selectedSystemName_.c_str());
		ImGui::TextDisabled("JSON ファイルは削除されません。元に戻せません。");
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
		if (ImGui::Button("削除する", ImVec2(120, 0))) {
			inspectors_.erase(selectedSystemName_);
			YParticleManager::GetInstance().RemoveSystem(selectedSystemName_);
			YEditorWidget::NotifyInfo("削除しました: %s", selectedSystemName_.c_str());
			selectedSystemName_.clear();
			showDeleteConfirm_ = false;
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120, 0))) showDeleteConfirm_ = false;
		if (!showDeleteConfirm_) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	// ── ファイルダイアログ ──────────────────────────────────
	DrawBrowserPopup("##saveAs", showSaveAsPopup_,
		"名前を付けて保存 — 既存ファイルをクリックで上書き（バンドルなら同名システムだけ差し替え）",
		saveAsBrowser_, saveAsNameBuf_, sizeof(saveAsNameBuf_),
		[this](const std::string& path) {
			if (auto* system = GetSelectedSystemPtr()) SaveSystemIntoFile(*system, path);
		});
	DrawBrowserPopup("##load", showLoadPopup_,
		"開く — 単体ファイル / エフェクトバンドル / 一括ファイルのどれでも読み込めます",
		loadBrowser_, saveAsNameBuf_, sizeof(saveAsNameBuf_), nullptr);
	DrawBrowserPopup("##saveAll", showSaveAllPopup_,
		"全システムを 1 ファイルに保存",
		saveAllBrowser_, saveAsNameBuf_, sizeof(saveAsNameBuf_),
		[this](const std::string& path) { SaveAllSystemsToFile(path); });
}

void YParticleEditor::HandleShortcuts(YParticleSystem* system) {
	// このパネルにフォーカスがあるときだけ（他のエディタの Ctrl+S と衝突させない）
	if (!system || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) SaveSelected();
}

void YParticleEditor::UpdateTestEmit(YParticleSystem* system) {
	if (!system || !testEmitLoop_) {
		testEmitTimer_ = 0.0f;
		return;
	}
	testEmitTimer_ += ImGui::GetIO().DeltaTime;
	if (testEmitTimer_ >= testEmitInterval_) {
		testEmitTimer_ = 0.0f;
		system->Emit(testEmitPos_, testEmitCount_);
	}
}

//=================================================================
// ファイル操作
//=================================================================

void YParticleEditor::SaveSelected() {
	YParticleSystem* system = GetSelectedSystemPtr();
	if (!system) return;

	if (system->GetSourceFilePath().empty()) {
		OpenSaveAs(system->GetName() + ".json");
		return;
	}
	SaveSystemIntoFile(*system, system->GetSourceFilePath());
}

bool YParticleEditor::SaveSystemIntoFile(YParticleSystem& system, const std::string& path) {
	nlohmann::json sysJson;
	system.SaveToJson(sysJson);

	// 既存ファイルを読み、バンドル（"systems" 配列あり）なら同名システムだけ差し替える
	nlohmann::json root;
	if (std::ifstream in(path); in.is_open()) {
		try { in >> root; } catch (...) { root = nullptr; }
	}

	bool replacedInBundle = false;
	if (root.is_object() && root.contains("systems") && root["systems"].is_array()) {
		bool found = false;
		for (auto& s : root["systems"]) {
			if (s.value("name", "") == system.GetName()) {
				s = sysJson;
				found = true;
				break;
			}
		}
		if (!found) root["systems"].push_back(sysJson);
		replacedInBundle = true;
	} else {
		root = std::move(sysJson);
	}

	std::ofstream out(path);
	if (!out.is_open()) {
		YEditorWidget::NotifyError("保存に失敗しました: %s", path.c_str());
		return false;
	}
	out << root.dump(4);

	system.SetSourceFilePath(path);
	GetInspector(system.GetName()).MarkSaved();
	YEditorWidget::NotifySuccess(replacedInBundle ? "保存しました（バンドル内を更新）: %s" : "保存しました: %s",
		FileNameOf(path).c_str());
	return true;
}

bool YParticleEditor::LoadFile(const std::string& path) {
	nlohmann::json root;
	if (std::ifstream in(path); in.is_open()) {
		try { in >> root; } catch (const std::exception& e) {
			YEditorWidget::NotifyError("JSON の読み込みに失敗しました: %s\n%s", FileNameOf(path).c_str(), e.what());
			return false;
		}
	} else {
		YEditorWidget::NotifyError("ファイルを開けません: %s", path.c_str());
		return false;
	}

	auto& manager = YParticleManager::GetInstance();
	bool ok = false;
	if (root.contains("systems")) {
		// バンドル / 一括ファイル（グループもあれば一緒に読む）
		ok = manager.LoadEffectBundle(path);
	} else {
		ok = manager.LoadSystemsFromFile(path);
		if (ok && root.contains("name")) selectedSystemName_ = root["name"];
	}

	// 読み直したシステムの履歴は意味を失うので捨てる
	inspectors_.clear();

	if (ok) YEditorWidget::NotifySuccess("読み込みました: %s", FileNameOf(path).c_str());
	else    YEditorWidget::NotifyError("読み込みに失敗しました: %s", FileNameOf(path).c_str());
	return ok;
}

bool YParticleEditor::SaveAllSystemsToFile(const std::string& path) {
	nlohmann::json root;
	root["version"] = "1.0";
	root["systems"] = nlohmann::json::array();

	auto& manager = YParticleManager::GetInstance();
	for (const auto& name : manager.GetAllSystemNames()) {
		if (auto* system = manager.GetSystem(name)) {
			nlohmann::json j;
			system->SaveToJson(j);
			root["systems"].push_back(std::move(j));
		}
	}

	std::ofstream out(path);
	if (!out.is_open()) {
		YEditorWidget::NotifyError("保存に失敗しました: %s", path.c_str());
		return false;
	}
	out << root.dump(4);
	YEditorWidget::NotifySuccess("全システムを保存しました: %s", FileNameOf(path).c_str());
	return true;
}

//=================================================================
// ヘルパー
//=================================================================

YParticleSystem* YParticleEditor::GetSelectedSystemPtr() {
	if (selectedSystemName_.empty()) return nullptr;
	return YParticleManager::GetInstance().GetSystem(selectedSystemName_);
}

YParticleSystemInspector& YParticleEditor::GetInspector(const std::string& systemName) {
	auto& slot = inspectors_[systemName];
	if (!slot) slot = std::make_unique<YParticleSystemInspector>();
	return *slot;
}

std::string YParticleEditor::MakeUniqueSystemName(const std::string& base) const {
	auto& manager = YParticleManager::GetInstance();
	if (!manager.GetSystem(base)) return base;
	for (int i = 1;; ++i) {
		std::string candidate = base + std::to_string(i);
		if (!manager.GetSystem(candidate)) return candidate;
	}
}

void YParticleEditor::OpenSaveAs(const std::string& defaultFileName) {
	saveAsBrowser_.Scan();
	strncpy_s(saveAsNameBuf_, defaultFileName.c_str(), _TRUNCATE);
	showSaveAsPopup_ = true;
}

#endif // USE_IMGUI

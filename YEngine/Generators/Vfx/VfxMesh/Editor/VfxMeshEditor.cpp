// ===========================================================
// VfxMeshEditor.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "VfxMeshEditor.h"
#include "DirectXCommon.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include "Debugger/Logger.h"
#include <Loaders/Texture/TextureManager.h>
#include <IconsFontAwesome5.h>
#include "Core/Editor/Widgets/YEditorWidget.h"
#include <cfloat>
#include <cctype>


namespace fs = std::filesystem;

namespace {

	/// 名前で探す（大文字小文字を無視。英字のみ）
	bool MatchesFilter(const std::string& name, const char* filter) {
		if (!filter || !filter[0]) return true;
		std::string a = name, b = filter;
		std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a.find(b) != std::string::npos;
	}

} // namespace

namespace YoRigine {

	VfxMeshEditor::VfxMeshEditor()
	{
		// ---- ランプテクスチャ用 (t1: gTexRamp) ----
		rampBrowser_ = FileBrowser(
			"Resources/Textures/",
			{ ".png", ".jpg", ".jpeg", ".dds" },
			FileBrowser::DisplayMode::Grid);

		rampBrowser_.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
			TextureManager::GetInstance()->LoadTexture(path);
			auto handle = TextureManager::GetInstance()->GetsrvHandleGPU(path);
			return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
			});

		rampBrowser_.SetOnFileSelected([this](const std::string& path) {
			auto* sel = Selected();
			if (!sel) return;
			VfxEffectAsset before = sel->asset;
			sel->asset.trail.texturePath = path;
			CommitChange(before, "Trail ランプテクスチャ");
			showRampPopup_ = false;
			});

		// ---- ノイズテクスチャ用 (t0: gTexNoise) ----
		noiseBrowser_ = FileBrowser(
			"Resources/Textures/",
			{ ".png", ".jpg", ".jpeg", ".dds" },
			FileBrowser::DisplayMode::Grid);

		noiseBrowser_.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
			TextureManager::GetInstance()->LoadTexture(path);
			auto handle = TextureManager::GetInstance()->GetsrvHandleGPU(path);
			return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
			});

		noiseBrowser_.SetOnFileSelected([this](const std::string& path) {
			auto* sel = Selected();
			if (!sel) return;
			VfxEffectAsset before = sel->asset;
			sel->asset.trail.noiseTexturePath = path;
			CommitChange(before, "Trail ノイズテクスチャ");
			showNoisePopup_ = false;
			});
	}

	VfxMeshEditor* VfxMeshEditor::GetInstance()
	{
		static VfxMeshEditor instance;
		return &instance;
	}

	void VfxMeshEditor::Initialize(const std::string& scanRoot)
	{
		dxCommon_ = DirectXCommon::GetInstance();
		scanRoot_ = scanRoot;

		// Trail用のプレビューはEmitterに委譲
		previewTrailEmitter_ = std::make_unique<TrailMeshEmitter>();

		ScanDirectory(scanRoot_);

		if (!entries_.empty()) {
			SelectEffect(0);
		}
	}

	void VfxMeshEditor::Finalize()
	{
		previewElements_.clear();   // ~PreviewElement が CB を Unmap する
		entries_.clear();
		selectedIndex_ = -1;
	}

	void VfxMeshEditor::ScanDirectory(const std::string& dir)
	{
		entries_.clear();

		std::error_code ec;
		if (!fs::exists(dir, ec)) {
			Logger("VfxMeshEditor: スキャン対象ディレクトリが存在しません: " + dir);
			return;
		}

		for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
			if (ec) break;
			if (entry.path().extension() != ".json") continue;

			VfxEffectEntry e;
			e.filePath = entry.path().string();
			if (e.asset.LoadFromJson(e.filePath)) {
				entries_.push_back(std::move(e));
				Logger("VfxMeshEditor: ロード -> " + entries_.back().filePath);
			}
		}
		Logger("VfxMeshEditor: " + std::to_string(entries_.size()) + " エフェクトをロードしました");
	}

	void VfxMeshEditor::SelectEffect(int index)
	{
		if (index < 0 || index >= static_cast<int>(entries_.size())) return;

		selectedIndex_ = index;
		history_.Clear();
		pendingCommit_ = false;

		previewTimer_ = 0.f;
		previewPlaying_ = false;

		// Emitter に現在のアセットを反映させる
		if (previewTrailEmitter_) {
			previewTrailEmitter_->Stop();
			previewTrailEmitter_->SetCamera(camera_);
			previewTrailEmitter_->LoadAsset(entries_[selectedIndex_].filePath);
			previewTrailEmitter_->SetAsset(entries_[selectedIndex_].asset); // 最新状態を同期
		}

		previewTrailEmitter_->Play();

		SyncPreviewElements();
	}

	// ===========================================================
	// 描画（cmdListを受け取らず、Emitter側のDrawを呼ぶ）
	// ===========================================================
	void VfxMeshEditor::DrawImGui()
	{
		// 文字入力中の Ctrl+Z は入力欄自身の取り消しに使われるので、エディタの Undo は走らせない
		if (!ImGui::GetIO().WantTextInput) history_.HandleKeyInput();
		// Ctrl+S はこのパネルにフォーカスがあるときだけ（他のエディタと衝突させない）
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
			SaveCurrent();
		}

		ImGui::SetNextWindowSize(ImVec2(760, 680), ImGuiCond_FirstUseEver);

		ImGui::BeginChild("##list", ImVec2(listWidth_, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
		listWidth_ = ImGui::GetWindowWidth();
		DrawListPanel();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##edit", ImVec2(0, 0), true);
		DrawEditPanel();
		ImGui::EndChild();

		DrawNewEffectDialog();
		DrawConfirmDialogs();
		FlushPendingCommit();
	}

	void VfxMeshEditor::DrawConfirmDialogs()
	{
		// ── 削除（ディスク上の JSON も消すので必ず確認する。元に戻せない）──
		if (deleteRequestIndex_ >= 0 && !ImGui::IsPopupOpen("VFX エフェクトの削除")) ImGui::OpenPopup("VFX エフェクトの削除");
		if (ImGui::BeginPopupModal("VFX エフェクトの削除", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			const bool valid = deleteRequestIndex_ >= 0 && deleteRequestIndex_ < static_cast<int>(entries_.size());
			if (valid) {
				ImGui::Text("「%s」を削除しますか？", entries_[deleteRequestIndex_].asset.name.c_str());
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), ICON_FA_EXCLAMATION_TRIANGLE " JSON ファイルもディスクから削除されます。元に戻せません。");
				ImGui::TextDisabled("%s", entries_[deleteRequestIndex_].filePath.c_str());
			}
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
			if (ImGui::Button("削除する", ImVec2(120, 0)) && valid) {
				selectedIndex_ = deleteRequestIndex_;
				DeleteCurrent();
				deleteRequestIndex_ = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::Button("キャンセル", ImVec2(120, 0)) || !valid) {
				deleteRequestIndex_ = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// ── 再読み込み（未保存の変更が消えるので確認する）──
		if (showRescanConfirm_ && !ImGui::IsPopupOpen("VFX の再読み込み")) ImGui::OpenPopup("VFX の再読み込み");
		if (ImGui::BeginPopupModal("VFX の再読み込み", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			int dirtyCount = 0;
			for (const auto& e : entries_) dirtyCount += e.isDirty ? 1 : 0;
			ImGui::Text("未保存の変更が %d 件あります。破棄してフォルダから読み直しますか？", dirtyCount);
			if (ImGui::Button("破棄して読み直す", ImVec2(160, 0))) {
				Rescan();
				showRescanConfirm_ = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
				showRescanConfirm_ = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void VfxMeshEditor::Rescan()
	{
		const int prevSel = selectedIndex_;
		ScanDirectory(scanRoot_);
		selectedIndex_ = -1;
		if (prevSel >= 0 && prevSel < static_cast<int>(entries_.size())) SelectEffect(prevSel);
		else if (!entries_.empty())                                    SelectEffect(0);
		YEditorWidget::NotifyInfo("VFX を読み直しました（%d 件）", static_cast<int>(entries_.size()));
	}

	void VfxMeshEditor::DrawListPanel()
	{
		if (ImGui::Button(ICON_FA_PLUS " 新規")) {
			std::string uniq = MakeUniqueEffectName("NewEffect");
			strncpy_s(newNameBuffer_, sizeof(newNameBuffer_), uniq.c_str(), _TRUNCATE);
			newPathBuffer_[0] = '\0';
			showNewDialog_ = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_SYNC)) {
			// 未保存の変更があるときは黙って捨てない
			bool anyDirty = false;
			for (const auto& e : entries_) anyDirty |= e.isDirty;
			if (anyDirty) showRescanConfirm_ = true;
			else          Rescan();
		}
		YEditorWidget::ItemTooltip("フォルダから読み直す");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##filter", ICON_FA_SEARCH " 検索", listFilter_, sizeof(listFilter_));

		ImGui::Separator();

		for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
			const auto& e = entries_[i];
			if (!MatchesFilter(e.asset.name, listFilter_)) continue;

			std::string label = e.asset.name;
			if (e.isDirty) label += " *";

			bool selected = (selectedIndex_ == i);
			if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
				SelectEffect(i);
			}

			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem(ICON_FA_SAVE " 保存")) {
					selectedIndex_ = i;
					SaveCurrent();
				}
				ImGui::Separator();
				// ディスク上のファイルも消えるので、確認ダイアログを経由する
				if (ImGui::MenuItem(ICON_FA_TRASH " 削除...")) deleteRequestIndex_ = i;
				ImGui::EndPopup();
			}

			YEditorWidget::ItemTooltip(e.filePath.c_str());
		}

		if (entries_.empty()) {
			ImGui::TextDisabled("(エフェクトなし)");
		}
	}

	void VfxMeshEditor::DrawEditPanel()
	{
		auto* sel = Selected();
		if (!sel) {
			ImGui::TextDisabled("エフェクトを選択してください");
			return;
		}
		auto& asset = sel->asset;

		// CPU / GPU エディタと同じ並びのツールバー
		if (ImGui::Button(ICON_FA_SAVE)) SaveCurrent();
		YEditorWidget::ItemTooltip("保存 (Ctrl+S)");
		ImGui::SameLine();
		ImGui::BeginDisabled(!history_.CanUndo());
		if (ImGui::Button(ICON_FA_UNDO)) history_.Undo();
		ImGui::EndDisabled();
		YEditorWidget::ItemTooltip("元に戻す (Ctrl+Z)");
		ImGui::SameLine();
		ImGui::BeginDisabled(!history_.CanRedo());
		if (ImGui::Button(ICON_FA_REDO)) history_.Redo();
		ImGui::EndDisabled();
		YEditorWidget::ItemTooltip("やり直し (Ctrl+Y)");
		// 再生操作は常にここに出す（以前は画面の一番下で、項目が多いとスクロールしないと届かなかった）
		ImGui::SameLine(0, 16);
		ImGui::TextDisabled("|");
		ImGui::SameLine(0, 16);
		DrawPlaybackControls();
		ImGui::SameLine(0, 16);
		if (sel->isDirty) ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "* 未保存");
		else              ImGui::TextDisabled("保存済み");
		YEditorWidget::ItemTooltip(("保存先: " + sel->filePath).c_str());
		ImGui::Separator();

		// 名前（Enter で確定。JSON のファイル名も合わせて変わる）と寿命無限
		// 入力中は ImGui が内部の文字列を使うので、毎フレーム写しても打っている文字は消えない
		strncpy_s(nameBuffer_, asset.name.c_str(), sizeof(nameBuffer_));
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
		if (ImGui::InputText("名前##effectname", nameBuffer_, sizeof(nameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
			VfxEffectAsset before = asset;
			asset.name = nameBuffer_;
			CommitChange(before, "名前変更");
			RenameCurrentFile(nameBuffer_);
		}
		YEditorWidget::ItemTooltip("Enter で確定（JSON のファイル名も変わります）");
		ImGui::SameLine(0, 16);
		{
			const bool prev = asset.loopForever;
			if (ImGui::Checkbox("寿命を無限にする", &asset.loopForever)) {
				VfxEffectAsset bLoop = asset;
				bLoop.loopForever = prev;
				CommitChange(bLoop, "寿命無限 切替");
			}
			YEditorWidget::ItemTooltip("ON: Stop するまで消えない（バフエリア等） / OFF: ワンショット（Lifetime に従う）");
		}
		ImGui::Separator();

		// 左: スタック（項目一覧） / 右: インスペクタ（選んだ 1 件だけ）
		ImGui::BeginChild("##vfxStack", ImVec2(stackWidth_, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
		stackWidth_ = ImGui::GetWindowWidth();
		DrawStackPanel();
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("##vfxInspector", ImVec2(0, 0), ImGuiChildFlags_Borders);
		DrawInspectorPanel();
		ImGui::EndChild();
	}


	void VfxMeshEditor::DrawNewEffectDialog()
	{
		if (!showNewDialog_) return;

		ImGui::OpenPopup("新規エフェクト作成");
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_Appearing);

		if (!ImGui::BeginPopupModal("新規エフェクト作成", &showNewDialog_, ImGuiWindowFlags_NoResize)) return;

		ImGui::Text("エフェクト名");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##newname", newNameBuffer_, sizeof(newNameBuffer_));

		ImGui::Spacing();
		ImGui::Text("保存先 JSON パス");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##newpath", newPathBuffer_, sizeof(newPathBuffer_));

		if (newPathBuffer_[0] == '\0' && newNameBuffer_[0] != '\0') {
			std::string autoPath = scanRoot_ + newNameBuffer_ + ".json";
			ImGui::TextDisabled("→ %s", autoPath.c_str());
		}

		ImGui::Spacing();
		ImGui::Text("プリセット");
		static const char* kPresetNames[] = {
			"Blank", "Trail Only", "Volume Only", "Waypoint Beam",
			"Sword", "Magic", "Explosion (モジュール駆動)"
		};
		YEditorWidget::Combo("##preset", newPresetIdx_, kPresetNames, IM_ARRAYSIZE(kPresetNames));

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		if (ImGui::Button("作成", ImVec2(120, 0))) {
			std::string path = newPathBuffer_;
			if (path.empty()) path = scanRoot_ + newNameBuffer_ + ".json";

			CreateNew(newNameBuffer_, path, static_cast<VfxPreset>(newPresetIdx_));
			showNewDialog_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
			showNewDialog_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

} // namespace YoRigine
#endif

#ifdef USE_IMGUI

#include <imgui_internal.h>

// Engine
#include "Editor.h"
#include "DirectXCommon.h"
#include "WinApp/WinApp.h"
#include "Debugger/Logger.h"
#include <SceneSystems/SceneManager.h>

// 静的メンバ
Editor* Editor::instance_ = nullptr;
bool    Editor::showEditor_ = false;


/// <summary>
/// Editor のシングルトン取得
/// </summary>
Editor* Editor::GetInstance()
{
	if (!instance_) {
		instance_ = new Editor();
	}
	return instance_;
}

/// <summary>
/// Editor 初期化（ドッキング・設定読み込みなど）
/// </summary>
void Editor::Initialize()
{
	//-------------------- ImGui 設定 --------------------//
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // ドッキング有効
	io.IniFilename = "imgui.ini";                      // 自動保存ファイル

	// 現在のシーン名取得
	auto current = SceneManager::GetInstance()->GetCurrentSceneName();
	if (!current.empty()) {
		currentScene_ = current;
	}

	// 設定読み込み & 反映
	LoadSettings();
	ApplySettings();
}

/// <summary>
/// Editor 終了処理（設定保存・クリア）
/// </summary>
void Editor::Finalize()
{
	SaveSettings();
	gameUIs_.clear();

	delete instance_;
	instance_ = nullptr;
}

/// <summary>
/// Editor メイン描画
/// </summary>
void Editor::Draw()
{
	//-------------------- F1 トグル --------------------//
	if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
		showEditor_ = !showEditor_;
	}

	if (!showEditor_) return;

	//-------------------- フルスクリーンメインウィンドウ --------------------//
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Editor", nullptr, flags);
	ImGui::PopStyleVar();

	DrawMenuBar();

	//-------------------- DockSpace --------------------//
	dockspaceID_ = ImGui::GetID("EditorDockSpace");
	ImGui::DockSpace(dockspaceID_);

	ImGui::End();

	//-------------------- 子ウィンドウ描画 --------------------//
	DrawGameWindow();
	DrawGameUIs();
}

/// <summary>
/// メニューバー描画
/// </summary>
void Editor::DrawMenuBar()
{
	if (ImGui::BeginMenuBar()) {

		//-------------------- シーン切り替え --------------------//
		if (ImGui::BeginMenu("シーン")) {
			for (const auto& sceneName : sceneNames_) {
				bool selected = (currentScene_ == sceneName);
				if (ImGui::MenuItem(sceneName.c_str(), nullptr, selected)) {
					currentScene_ = sceneName;
					if (sceneChangeCallback_) {
						sceneChangeCallback_(sceneName);
					}
				}
			}
			ImGui::EndMenu();
		}

		//-------------------- ゲームUI一覧 --------------------//
		if (ImGui::BeginMenu("UI一覧")) {
			for (auto& [name, ui] : gameUIs_) {
				ImGui::MenuItem(name.c_str(), nullptr, &ui.visible);
			}
			ImGui::EndMenu();
		}

		//-------------------- 表示メニュー --------------------//
		if (ImGui::BeginMenu("表示")) {
			if (ImGui::MenuItem("エディターの非表示", "F1")) {
				showEditor_ = false;
			}
			if (ImGui::MenuItem("設定をセーブ")) {
				SaveSettings();
			}
			ImGui::EndMenu();
		}

		//-------------------- 外部ツールの追加メニュー --------------------//
		for (auto& cb : menuCallbacks_) {
			if (cb) cb();
		}

		ImGui::EndMenuBar();
	}
}

/// <summary>
/// ゲーム画面（FinalResult）の表示ウィンドウ
/// </summary>
void Editor::DrawGameWindow()
{
	auto sceneName = SceneManager::GetInstance()->GetCurrentSceneName();

	if (ImGui::Begin(
		sceneName.c_str(), nullptr,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoTitleBar))
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		auto dxCommon = YoRigine::DirectXCommon::GetInstance();

		if (dxCommon) {
			ImTextureID textureID = (ImTextureID)dxCommon->GetFinalResultGPUHandle().ptr;

			int renderW = WinApp::GetInstance()->kClientWidth;
			int renderH = WinApp::GetInstance()->kClientHeight;
			float aspect = (float)renderW / (float)renderH;

			//-----------------------------------------
			// アスペクト比維持の Image サイズ計算
			//-----------------------------------------
			ImVec2 imageSize;
			float availAspect = avail.x / avail.y;

			if (availAspect > aspect) {
				imageSize.y = avail.y;
				imageSize.x = imageSize.y * aspect;
			} else {
				imageSize.x = avail.x;
				imageSize.y = imageSize.x / aspect;
			}

			//-----------------------------------------
			// Image の中央配置
			//-----------------------------------------
			ImVec2 offset = {
				(avail.x - imageSize.x) * 0.5f,
				(avail.y - imageSize.y) * 0.5f
			};
			ImGui::SetCursorPos(ImVec2(
				ImGui::GetCursorPos().x + offset.x,
				ImGui::GetCursorPos().y + offset.y
			));

			//-----------------------------------------
			// ゲームビューの矩形情報を保持
			//-----------------------------------------
			gameViewPos_ = ImGui::GetCursorScreenPos();
			gameViewSize_ = imageSize;

			//-----------------------------------------
			// 描画
			//-----------------------------------------
			ImGui::Image(textureID, imageSize);
		}
	}

	ImGui::End();
}

/// <summary>
/// 各 GameUI を描画
/// </summary>
void Editor::DrawGameUIs()
{
	auto currentScene = SceneManager::GetInstance()->GetCurrentSceneName();

	for (auto& [name, ui] : gameUIs_) {
		if (!ui.visible || !ui.drawFunc) continue;

		// シーン限定 UI のフィルタリング
		if (ui.sceneName != "AllScene" && ui.sceneName != currentScene) {
			continue;
		}

		// ImGui が自動 Dock/配置
		if (ImGui::Begin(name.c_str(), &ui.visible)) {
			ui.drawFunc();
		}
		ImGui::End();
	}
}

/// <summary>
/// GameUI の登録
/// </summary>
void Editor::RegisterGameUI(
	const std::string& name,
	std::function<void()> drawFunc,
	const std::string& sceneName)
{
	GameUI newUI = { name, sceneName, drawFunc, true };

	// 保存済み設定があれば適用
	if (settingsLoaded_) {
		auto it = savedSettings_.find(name);
		if (it != savedSettings_.end() && it->second.hasSettings) {
			newUI.visible = it->second.visible;
		}
	}

	gameUIs_[name] = newUI;
}

/// <summary>
/// UI の登録解除
/// </summary>
void Editor::UnregisterGameUI(const std::string& name)
{
	gameUIs_.erase(name);
}

/// <summary>
/// Editor 設定保存
/// </summary>
void Editor::SaveSettings()
{
	std::ofstream file("editor_settings.ini");
	if (!file.is_open()) return;

	file << "[Editor]\n";
	file << "ShowEditor=" << (showEditor_ ? "1" : "0") << "\n";
	file << "CurrentScene=" << currentScene_ << "\n\n";

	file << "[GameUI]\n";
	for (const auto& [name, ui] : gameUIs_) {
		file << name << "_visible=" << (ui.visible ? "1" : "0") << "\n";
	}

	file.close();
}

/// <summary>
/// Editor 設定読み込み
/// </summary>
void Editor::LoadSettings()
{
	std::ifstream file("editor_settings.ini");
	if (!file.is_open()) {
		settingsLoaded_ = true;
		return;
	}

	std::string line;
	std::string section;

	while (std::getline(file, line)) {

		if (line.empty() || line[0] == '#') continue;

		// セクション名判定
		if (line.front() == '[' && line.back() == ']') {
			section = line.substr(1, line.length() - 2);
			continue;
		}

		// key=value
		size_t pos = line.find('=');
		if (pos == std::string::npos) continue;

		std::string key = line.substr(0, pos);
		std::string value = line.substr(pos + 1);

		//-----------------------------------------
		// Editor セクション
		//-----------------------------------------
		if (section == "Editor") {

			if (key == "ShowEditor") {
				showEditor_ = (value == "1");
			} else if (key == "CurrentScene") {
				currentScene_ = value;
			}
		}

		//-----------------------------------------
		// GameUI セクション
		//-----------------------------------------
		else if (section == "GameUI") {

			size_t under = key.rfind('_');
			if (under == std::string::npos) continue;

			std::string uiName = key.substr(0, under);
			std::string property = key.substr(under + 1);

			if (property == "visible") {

				if (savedSettings_.find(uiName) == savedSettings_.end()) {
					savedSettings_[uiName] = SavedSettings();
				}
				savedSettings_[uiName].visible = (value == "1");
				savedSettings_[uiName].hasSettings = true;
			}
		}
	}
	settingsLoaded_ = true;
	file.close();
}

/// <summary>
/// 読み込んだ設定を UI に反映
/// </summary>
void Editor::ApplySettings()
{
	for (auto& [name, ui] : gameUIs_) {
		auto it = savedSettings_.find(name);
		if (it != savedSettings_.end() && it->second.hasSettings) {
			ui.visible = it->second.visible;
		}
	}
}
void Editor::DrawLog()
{
	const auto& logs = LogSystem::Get().GetLogs();
	for (const auto& line : logs) {
		ImGui::TextUnformatted(line.c_str());
	}

	// 自動スクロール
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		ImGui::SetScrollHereY(1.0f);
}
#endif // USE_IMGUI

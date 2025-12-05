#pragma once
#ifdef USE_IMGUI


#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <fstream>

/// <summary>
/// このエンジンのエディタ機能を管理するクラス
/// </summary>
class Editor {
public:
	///************************* 基本的な関数 *************************///
	static Editor* GetInstance();
	void Initialize();
	void Finalize();
	void Draw();
	// このプロジェクトのログを出力
	void DrawLog();


	// ゲームUI登録
	void RegisterGameUI(const std::string& name, std::function<void()> drawFunc, const std::string& sceneName = "AllScene");
	// ゲームUI登録解除
	void UnregisterGameUI(const std::string& name);
	// シーン変更コールバック
	void SetSceneChangeCallback(std::function<void(const std::string&)> callback) {
		sceneChangeCallback_ = callback;
	}
	// エディターのメニューバーに登録
	void RegisterMenuBar(std::function<void()> callback) {
		menuCallbacks_.push_back(callback);
	}
	// ゲームビュー（レンダリング画像）のサイズを取得
	ImVec2 GetGameViewSize() { return gameViewSize_; }
	ImVec2 GetGameViewPos() { return gameViewPos_; }
	// ゲームウィンドウの空き領域サイズを取得
	ImVec2 GetGameWindowAvail() { return gameWindowAvail_; }
	bool GetShowEditor() { return showEditor_; }

private:
	///************************* 内部処理 *************************///
	void DrawMenuBar();
	void DrawGameWindow();
	void DrawGameUIs();
	void SaveSettings();
	void LoadSettings();
	void ApplySettings();
private:
	///************************* メンバ変数 *************************///

	// 登録されたゲームUI一覧
	struct GameUI {
		std::string name;
		std::string sceneName = "AllScene"; // AllSceneは全シーンで表示
		std::function<void()> drawFunc;
		bool visible = true;
	};
	// 保存された設定（UIの可視性のみ）
	struct SavedSettings {
		bool visible = true;
		bool hasSettings = false;
	};
	Editor() = default;
	static Editor* instance_;
	static bool showEditor_;

	std::string currentScene_ = "Title";
	std::vector<std::string> sceneNames_ = { "Title", "Game", "Clear" };
	std::unordered_map<std::string, GameUI> gameUIs_;
	std::function<void(const std::string&)> sceneChangeCallback_;

	// MenuBar 描画時に呼び出すコールバック一覧
	std::vector<std::function<void()>> menuCallbacks_;
	std::unordered_map<std::string, SavedSettings> savedSettings_;

	ImGuiID dockspaceID_ = 0;
	bool settingsLoaded_ = false;


	ImVec2 gameViewSize_ = { 0,0 };
	ImVec2 gameViewPos_ = { 0,0 };
	ImVec2 gameWindowAvail_ = { 0,0 };
};

#endif // _DEBUG
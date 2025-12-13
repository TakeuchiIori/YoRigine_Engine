#pragma once

// C++
#ifdef USE_IMGUI
#include "imgui.h"
#include <wrl.h>
#include <d3d12.h>
#endif

// Forward Declaration
class WinApp;

namespace YoRigine{
class DirectXCommon;
}


// ImGuiの初期化・描画・終了処理を一括管理する
// Editor機能全体の起点となるクラス
class ImGuiManager
{
public:
	///************************* シングルトン *************************///

	static ImGuiManager* GetInstance();

	ImGuiManager() = default;
	~ImGuiManager() = default;

public:
	///************************* 基本関数 *************************///

	void Initialize(WinApp* winApp, YoRigine::DirectXCommon* dxCommon);
	void Begin();
	void End();
	void Draw();
	void Finalize();

private:
	///************************* 内部処理 *************************///

	void InitialzeDX12();
	void CustomizeColor();
	void CustomizeEditor();

private:
	///************************* シングルトン制御 *************************///

	static ImGuiManager* instance;
	ImGuiManager(ImGuiManager&) = delete;
	ImGuiManager& operator=(ImGuiManager&) = delete;

private:
#ifdef USE_IMGUI
	///************************* メンバ変数 *************************///

	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	WinApp* winApp_ = nullptr;
#endif
};

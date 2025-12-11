#include "ImGuiManager.h"
// Engine
#include "WinApp./WinApp.h"
#include "DirectXCommon.h"
#ifdef USE_IMGUI
#include "imgui.h"
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "imgui_internal.h"        
#include <imgui_impl_dx12.cpp>
#endif
#include <iostream>


ImGuiManager* ImGuiManager::instance = nullptr;
ImGuiManager* ImGuiManager::GetInstance()
{
	if (instance == nullptr) {
		instance = new ImGuiManager;
	}
	return instance;
}

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon)
{
#ifdef USE_IMGUI
	// メンバ変数に引数を渡す
	dxCommon_ = dxCommon;

	winApp_ = winApp;

	// ImGuiのコンテキストを生成
	ImGui::CreateContext();

	// Editorの設定
	CustomizeEditor();

	// DockSpaceの設定
	CustomizeColor();

	// DirectX12の初期化
	InitialzeDX12();

#endif
}

void ImGuiManager::Begin()
{
#ifdef USE_IMGUI
	// ImGuiフレーム開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::End()
{
#ifdef USE_IMGUI
	ImGui::EndFrame();
	// 描画前準備
	ImGui::Render();
#endif
}

void ImGuiManager::Draw()
{
#ifdef USE_IMGUI
	// デスクリプターヒープの配列をセットするコマンド
	ID3D12DescriptorHeap* ppHeaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	if (ImGui::GetDrawData()->TotalVtxCount == 0) {
		printf("Warning: No vertices to render!\n");
		return;
	}
	// 描画コマンドを発行
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetCommandList().Get());

#endif
}


void ImGuiManager::InitialzeDX12()
{
#ifdef USE_IMGUI

	// DirectX12の初期化
	IMGUI_CHECKVERSION();
	// Win32用初期化
	ImGui_ImplWin32_Init(winApp_->GetHwnd());

	uint32_t srvIndex = SrvManager::GetInstance()->Allocate();
	HRESULT hr = ImGui_ImplDX12_Init(
		dxCommon_->GetDevice().Get(),
		dxCommon_->GetBackBufferCount(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		SrvManager::GetInstance()->GetDescriptorHeap(),
		SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex),
		SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex)
	);

	assert(SUCCEEDED(hr));
#endif // DEBUG
}

void ImGuiManager::CustomizeColor()
{
#ifdef USE_IMGUI
	// ベースを Dark に
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* c = style.Colors;

	//-------------------------------------------------
	//   1. ウィンドウ & タブ帯をほぼ黒グレーへ
	//-------------------------------------------------
	ImVec4 bg = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);   // 背景
	ImVec4 bgAlt = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);   // 少し明るい
	ImVec4 tab = ImVec4(0.10f, 0.10f, 0.11f, 0.97f);   // タブ通常
	ImVec4 tabAct = ImVec4(0.14f, 0.14f, 0.15f, 1.00f);   // タブ選択中

	c[ImGuiCol_WindowBg] = bg;
	c[ImGuiCol_ChildBg] = bg;
	c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.09f, 0.96f);
	c[ImGuiCol_DockingEmptyBg] = bgAlt;

	// タイトルバー = ドックタブ帯
	c[ImGuiCol_TitleBg] = bgAlt;
	c[ImGuiCol_TitleBgActive] = bgAlt;
	c[ImGuiCol_TitleBgCollapsed] = bgAlt;

	// タブ
	c[ImGuiCol_Tab] = tab;
	c[ImGuiCol_TabHovered] = tabAct;
	c[ImGuiCol_TabActive] = tabAct;
	c[ImGuiCol_TabUnfocused] = tab;
	c[ImGuiCol_TabUnfocusedActive] = tabAct;

	// メニューバー
	c[ImGuiCol_MenuBarBg] = bgAlt;
	//   3. テキストは明暗差を確保
	//-------------------------------------------------
	c[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);

	//-------------------------------------------------
	//   4. 微調整
	//-------------------------------------------------
	style.FrameRounding = 4.0f;
	style.WindowPadding = ImVec2(4, 4);
	style.ItemSpacing = ImVec2(6, 4);
	style.ScrollbarSize = 14.0f;
#endif
}

void ImGuiManager::CustomizeEditor()
{
#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// 1. 日本語フォントのロード（ベースとなるフォント）
	ImFontConfig jp_config;
	jp_config.OversampleH = 1;
	jp_config.OversampleV = 1;
	jp_config.PixelSnapH = true;
	jp_config.MergeMode = false;

	io.Fonts->AddFontFromFileTTF(
		"Resources/Fonts/ipaexg.ttf",
		14.0f,
		&jp_config,
		io.Fonts->GetGlyphRangesJapanese()
	);

	// 2. アイコンフォントのロード（日本語フォントにマージ）
	ImFontConfig icon_config;
	icon_config.OversampleH = 1;
	icon_config.OversampleV = 1;
	icon_config.PixelSnapH = true;

	// ★★★ マージモードを有効にする ★★★
	icon_config.MergeMode = true;

	// Font Awesome (または同様のアイコンフォント) の PUA 領域を定義
	// これが ImWchar の範囲に収まる、安全な領域です。
	static const ImWchar icon_ranges[] = {
		0xf000, 0xf8ff, // PUA Range (Font Awesome v5 Solid/Regular/Brandsなど)
		0, // 終端
	};

	// ★★★ アイコンフォントファイルをロードし、日本語フォントにマージ ★★★
	// ここを、ご自身で用意したアイコンフォントのパスに置き換えてください。
	io.Fonts->AddFontFromFileTTF(
		"Resources/Fonts/Free-Solid-900.otf", // 例: Font AwesomeのTTFファイルなど
		14.0f,
		&icon_config,
		icon_ranges
	);

	io.Fonts->Build();
#endif
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI
	// 後始末
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	delete instance;
	instance = nullptr;
#endif // DEBUG
}

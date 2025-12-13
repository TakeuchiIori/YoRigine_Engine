#include "DirectXCommon.h"

// C++
#include <cassert>
#include <thread>
#include <format>
#include "d3dx12.h"
#include "Debugger/Logger.h"
#include "Debugger/ConvertString.h"
#include <Debugger/DebugConsole.h>

// lib
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"dxcompiler.lib")



namespace YoRigine {
	std::unique_ptr<DirectXCommon> DirectXCommon::instance = nullptr;
	std::once_flag DirectXCommon::initInstanceFlag;
	DirectXCommon* DirectXCommon::GetInstance()
	{
		std::call_once(initInstanceFlag, []() {
			instance = std::make_unique<DirectXCommon>();
			});
		return instance.get();
	}

	void DirectXCommon::Initialize(WinApp* winApp)
	{
		// NULL検出
		assert(winApp);
		// メンバ変数に記録
		this->winApp_ = winApp;

		// FPS固定初期化
		InitializeFixFPS();

		// 各種マネージャーの初期化
		InitializeManagers();

		// レンダーターゲットと深度バッファの初期化
		InitializeRenderTargets();

		// ビューポート矩形の初期化
		InitializeViewPortRectangle();

		// シザリング矩形の初期化
		InitializeScissorRectangle();

		// DXCコンパイラの生成
		CreateDXCompiler();
	}

	void DirectXCommon::Finalize()
	{

		// コマンドマネージャーの終了処理（全フレームの完了を待機）
		if (commandManager_) {
			commandManager_->Finalize();
			commandManager_.reset();
		}

		// 各マネージャーの終了処理
		if (dsvManager_) {
			dsvManager_->Finalize();
			dsvManager_.reset();
		}
		if (rtvManager_) {
			rtvManager_->Finalize();
			rtvManager_.reset();
		}
		if (srvManager_) {
			srvManager_->Finalize();
			srvManager_ = nullptr;
		}
		if (swapChainManager_) {
			swapChainManager_->Finalize();
			swapChainManager_.reset();
		}
		if (deviceManager_) {
			deviceManager_->Finalize();
			deviceManager_.reset();
		}
		instance.reset();
	}

	void DirectXCommon::InitializeManagers()
	{
		// デバイスマネージャー
		deviceManager_ = std::make_unique<DeviceManager>();
		deviceManager_->Initialize();

		// ディスクリプタヒープ
		descriptorHeap_ = std::make_unique<DescriptorHeap>();
		descriptorHeap_->Initialize(this);

		// コマンドマネージャー
		commandManager_ = std::make_unique<CommandManager>();
		commandManager_->Initialize(deviceManager_.get());

		// スワップチェーンマネージャー
		swapChainManager_ = std::make_unique<SwapChainManager>();
		swapChainManager_->Initialize(winApp_, deviceManager_.get(), commandManager_.get());

		// SRVマネージャー
		srvManager_ = SrvManager::GetInstance();
		srvManager_->Initialize(this);

		// RTVマネージャー
		rtvManager_ = std::make_unique<RtvManager>();
		rtvManager_->Initialize(deviceManager_.get(), 16);  // 最大16個のRTV

		// DSVマネージャー
		dsvManager_ = std::make_unique<DsvManager>();
		dsvManager_->Initialize(deviceManager_.get(), 8);   // 最大8個のDSV
	}

	void DirectXCommon::InitializeRenderTargets()
	{
		// スワップチェーンのバックバッファを登録
		auto& resources = swapChainManager_->GetSwapChainResources();
		for (int i = 0; i < swapChainManager_->GetBackBuffers(); i++) {
			std::string name = "BackBuffer" + std::to_string(i);
			rtvManager_->Register(name, resources[i], DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		}

		// オフスクリーン用レンダーターゲットを作成
		rtvManager_->Create(
			"OffScreen",
			WinApp::kClientWidth,
			WinApp::kClientHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ 0.1f, 0.1f, 0.2f, 1.0f },
			true  // SRVも作成
		);

		rtvManager_->Create(
			"FinalResult",
			WinApp::kClientWidth,
			WinApp::kClientHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ 0.0f, 0.0f, 0.0f, 1.0f },
			true  // SRVも作成
		);

		// メインの深度バッファを作成
		dsvManager_->Create(
			"MainDepth",
			WinApp::kClientWidth,
			WinApp::kClientHeight,
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			true  // SRVも作成
		);

		// シャドウマップ用の深度バッファを作成
		dsvManager_->Create(
			"ShadowDepth",
			DsvManager::kShadowmapWidth,
			DsvManager::kShadowmapHeight,
			DXGI_FORMAT_D32_FLOAT,
			true  // SRVも作成
		);
	}


	void DirectXCommon::PreDrawShadow()
	{
		auto commandList = commandManager_->GetCommandList();

		// シャドウマップ用深度バッファをレンダーターゲットとして使用
		if (shadowDepthCurrentState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
			dsvManager_->TransitionBarrier(commandList.Get(), "ShadowDepth",
				shadowDepthCurrentState_,
				D3D12_RESOURCE_STATE_DEPTH_WRITE);
			shadowDepthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		// シャドウマップ用深度バッファをセット
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvManager_->GetHandle("ShadowDepth");
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

		// クリア処理
		dsvManager_->Clear("ShadowDepth", commandList.Get());

		// シャドウマップ用ビューポートとシザー矩形の設定
		commandList->RSSetViewports(1, &shadowVP_);
		commandList->RSSetScissorRects(1, &shadowSC_);
	}

	void DirectXCommon::PreDrawOffScreen()
	{
		auto commandList = commandManager_->GetCommandList();

#ifdef USE_IMGUI
		DebugConsole::GetInstance()->BeginFrame();
#endif

		// OffScreenをレンダーターゲットとして使用
		rtvManager_->TransitionBarrier(commandList.Get(), "OffScreen",
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		// 深度バッファが前フレームからDEPTH_WRITEでない場合は遷移
		if (depthCurrentState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
			dsvManager_->TransitionBarrier(commandList.Get(), "MainDepth",
				depthCurrentState_,
				D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		// シャドウマップ用デップスを読み取り可能状態に遷移
		if (shadowDepthCurrentState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
			dsvManager_->TransitionBarrier(
				commandList.Get(),
				"ShadowDepth",
				shadowDepthCurrentState_,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			shadowDepthCurrentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		// レンダーターゲットとデプスステンシルビューをセット
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvManager_->GetHandle("MainDepth");
		rtvManager_->SetRenderTargets(commandList.Get(), { "OffScreen" }, &dsvHandle);

		// クリア処理
		rtvManager_->Clear("OffScreen", commandList.Get());
		dsvManager_->Clear("MainDepth", commandList.Get());

		// ビューポートとシザー矩形の設定
		commandList->RSSetViewports(1, &viewport_);
		commandList->RSSetScissorRects(1, &scissorRect_);
	}

	void DirectXCommon::PreDraw()
	{
		auto commandList = commandManager_->GetCommandList();
		UINT backBufferIndex = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string currentBackBuffer = "BackBuffer" + std::to_string(backBufferIndex);

		// OffScreenをシェーダーリソースに遷移
		rtvManager_->TransitionBarrier(commandList.Get(), "OffScreen",
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_GENERIC_READ);

		// 深度バッファをシェーダーリソースに遷移
		dsvManager_->TransitionBarrier(commandList.Get(), "MainDepth",
			depthCurrentState_,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		depthCurrentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		// バックバッファをレンダーターゲットに遷移
		rtvManager_->TransitionBarrier(commandList.Get(), currentBackBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		//// 深度バッファを読み取り専用モードに遷移してバインド
		//dsvManager_->TransitionBarrier(commandList.Get(), "MainDepth",
		//	depthCurrentState_,
		//	D3D12_RESOURCE_STATE_DEPTH_READ);
		//depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_READ;

		// バックバッファをレンダーターゲットとして設定
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvManager_->GetHandle("MainDepth");
		rtvManager_->SetRenderTargets(commandList.Get(), { currentBackBuffer }, &dsvHandle);

		// クリア処理（白でクリア）
		auto* rt = rtvManager_->Get(currentBackBuffer);
		if (rt) {
			float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
			commandList->ClearRenderTargetView(rt->rtvHandle, clearColor, 0, nullptr);
		}

		// ビューポートとシザー矩形の設定
		commandList->RSSetViewports(1, &viewport_);
		commandList->RSSetScissorRects(1, &scissorRect_);
	}

	void DirectXCommon::DepthBarrier()
	{
		auto commandList = commandManager_->GetCommandList();
		UINT backBufferIndex = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string currentBackBuffer = "BackBuffer" + std::to_string(backBufferIndex);

		// 深度バッファを書き込み可能な状態に遷移
		dsvManager_->TransitionBarrier(commandList.Get(), "MainDepth",
			depthCurrentState_,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		// 深度バッファを再バインド（DEPTH_WRITE状態で使用するため）
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvManager_->GetHandle("MainDepth");
		rtvManager_->SetRenderTargets(commandList.Get(), { currentBackBuffer }, &dsvHandle);

		// ビューポートとシザー矩形も再設定
		commandList->RSSetViewports(1, &viewport_);
		commandList->RSSetScissorRects(1, &scissorRect_);
	}

	void DirectXCommon::CopyBackBufferToFinalResult()
	{
		auto commandList = commandManager_->GetCommandList();
		UINT backBufferIndex = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string currentBackBuffer = "BackBuffer" + std::to_string(backBufferIndex);

		// バックバッファをコピーソースに遷移
		rtvManager_->TransitionBarrier(commandList.Get(), currentBackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE);

		// FinalResultをコピー先に遷移
		rtvManager_->TransitionBarrier(commandList.Get(), "FinalResult",
			finalResultCurrentState_,
			D3D12_RESOURCE_STATE_COPY_DEST);

		// リソースをコピー
		auto* backBufferRT = rtvManager_->Get(currentBackBuffer);
		auto* finalResultRT = rtvManager_->Get("FinalResult");

		if (backBufferRT && finalResultRT) {
			commandList->CopyResource(finalResultRT->resource.Get(), backBufferRT->resource.Get());
		}

		// バックバッファをレンダーターゲットに戻す
		rtvManager_->TransitionBarrier(commandList.Get(), currentBackBuffer,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		// FinalResultをシェーダーリソースに遷移して状態を更新
		rtvManager_->TransitionBarrier(commandList.Get(), "FinalResult",
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_GENERIC_READ);

		finalResultCurrentState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	void DirectXCommon::PostDraw()
	{
		if (!swapChainManager_ || !swapChainManager_->GetSwapChain() ||
			!commandManager_ || !commandManager_->GetCommandList()) {
			return;
		}

		HRESULT hr;
		UINT backBufferIndex = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string currentBackBuffer = "BackBuffer" + std::to_string(backBufferIndex);
		auto commandList = commandManager_->GetCommandList();
		auto commandQueue = commandManager_->GetCommandQueue();

		// バックバッファを表示用に変更
		rtvManager_->TransitionBarrier(commandList.Get(), currentBackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);

		// コマンドリストの内容を確定させる
		hr = commandList->Close();
		assert(SUCCEEDED(hr));

		// コマンドリストの実行
		ID3D12CommandList* commandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, commandLists);

		// 画面の交換
		swapChainManager_->GetSwapChain()->Present(1, 0);

#ifdef USE_IMGUI
		DebugConsole::GetInstance()->EndFrame();
#endif

		// フレームの終了
		commandManager_->EndFrame();
		commandManager_->WaitForAllFrames();

		// FPS固定
		UpdateFixFPS();

		commandManager_->Reset(backBufferIndex);
	}
	void DirectXCommon::InitializeViewPortRectangle()
	{
		// クライアント領域のサイズと一緒にして画面全体に表示
		viewport_.Width = static_cast<float>(WinApp::kClientWidth);
		viewport_.Height = static_cast<float>(WinApp::kClientHeight);
		viewport_.TopLeftX = 0;
		viewport_.TopLeftY = 0;
		viewport_.MinDepth = 0.0f;
		viewport_.MaxDepth = 1.0f;

		// シャドウマップ用ビューポート
		shadowVP_.Width = static_cast<float>(DsvManager::kShadowmapWidth);
		shadowVP_.Height = static_cast<float>(DsvManager::kShadowmapHeight);
		shadowVP_.TopLeftX = 0;
		shadowVP_.TopLeftY = 0;
		shadowVP_.MinDepth = 0.0f;
		shadowVP_.MaxDepth = 1.0f;
	}

	void DirectXCommon::InitializeScissorRectangle()
	{
		// 基本的にビューポートと同じ矩形が構成されるようにする
		scissorRect_.left = 0;
		scissorRect_.right = WinApp::kClientWidth;
		scissorRect_.top = 0;
		scissorRect_.bottom = WinApp::kClientHeight;

		// シャドウマップ用シザー矩形
		shadowSC_.left = 0;
		shadowSC_.right = DsvManager::kShadowmapWidth;
		shadowSC_.top = 0;
		shadowSC_.bottom = DsvManager::kShadowmapHeight;
	}

	void DirectXCommon::CreateDXCompiler()
	{
		HRESULT hr;
		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
		assert(SUCCEEDED(hr));
		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
		assert(SUCCEEDED(hr));
		hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
		assert(SUCCEEDED(hr));
	}

	void DirectXCommon::InitializeFixFPS()
	{
		// 現在時間を記録
		reference_ = std::chrono::steady_clock::now();
	}

	void DirectXCommon::UpdateFixFPS()
	{
		// 現在時間を取得する
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

		// 前回記録からの経過時間を取得する
		std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

		// 次のフレームまでの待機時間を計算
		if (elapsed < kMinCheckTime) {
			// 待機すべき時間
			std::chrono::microseconds sleepTime = kMinCheckTime - elapsed;

			// より正確なスリープのためのスピンロック
			auto sleepEnd = now + sleepTime;
			// まず大部分の時間をsleep_forで待機
			if (sleepTime > std::chrono::microseconds(1000)) {
				std::this_thread::sleep_for(sleepTime - std::chrono::microseconds(1000));
			}
			// 残りの短い時間はスピンロックで正確に待機
			while (std::chrono::steady_clock::now() < sleepEnd) {
				// スピンロック（何もしない）
			}
		}

		// 次のフレームの基準時間を更新
		reference_ += kMinCheckTime;

		// もし大幅に遅れている場合は現在時刻に調整（フレームスキップ）
		if (std::chrono::steady_clock::now() > reference_ + kMinCheckTime) {
			reference_ = std::chrono::steady_clock::now();
		}
	}

	void DirectXCommon::ExecuteCommandList()
	{
		HRESULT hr = commandManager_->GetCommandList()->Close();
		assert(SUCCEEDED(hr));

		(void)hr;
		ID3D12CommandList* commandLists[] = { commandManager_->GetCommandList().Get() };
		commandManager_->GetCommandQueue()->ExecuteCommandLists(1, commandLists);
	}

	void DirectXCommon::WaitForGPU()
	{
		commandManager_->WaitForCurrentFrame();
	}

	void DirectXCommon::ResetCommandList()
	{
		uint32_t currentFrameIndex = commandManager_->GetCurrentFrameIndex();
		commandManager_->Reset(currentFrameIndex);
	}

	Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile)
	{
		// これからシェーダーをコンパイルする旨をログに出す
		Logger(ConvertString(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile)));

		// hlslファイルを読み込む
		IDxcBlobEncoding* shaderSource = nullptr;
		HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
		assert(SUCCEEDED(hr));

		// 読み込んだファイルの内容を設定する
		DxcBuffer shaderSourceBuffer;
		shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
		shaderSourceBuffer.Size = shaderSource->GetBufferSize();
		shaderSourceBuffer.Encoding = DXC_CP_UTF8;

		// コンパイルオプション
		LPCWSTR arguments[] = {
			filePath.c_str(),
			L"-E", L"main",
			L"-T", profile,
			L"-Zi", L"-Qembed_debug",
			L"-Od",
			L"-Zpr",
		};

		// 実際にShaderをコンパイルする
		IDxcResult* shaderResult = nullptr;
		hr = dxcCompiler_->Compile(
			&shaderSourceBuffer,
			arguments,
			_countof(arguments),
			includeHandler_,
			IID_PPV_ARGS(&shaderResult)
		);
		assert(SUCCEEDED(hr));

		// 警告・エラーチェック
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
		shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

		if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
			Logger(shaderError->GetStringPointer());
			assert(false);
		}

		// コンパイル結果から実行用のバイナリ部分を取得
		IDxcBlob* shaderBlob = nullptr;
		hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
		assert(SUCCEEDED(hr));

		// 成功したログを出す
		Logger(ConvertString(std::format(L"Compile Succeeded,path:{},profile:{}\n", filePath, profile)));

		// リソースを解放
		shaderSource->Release();
		shaderResult->Release();

		return shaderBlob;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
	{
		HRESULT hr;
		// 頂点リソース用のヒープの設定
		D3D12_HEAP_PROPERTIES uploadHeapProperties{};
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		// 頂点リソースの設定
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeInBytes;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		// 実際にリソースを作る
		Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
		hr = deviceManager_->GetDevice()->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource)
		);
		assert(SUCCEEDED(hr));

		return resource;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResourceUAV(size_t sizeInBytes)
	{
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = sizeInBytes;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&resource)
		);
		return resource;
	}

	void DirectXCommon::TransitionBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After)
	{
		// バリアの設定
		barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier_.Transition.pResource = pResource;
		barrier_.Transition.StateBefore = Before;
		barrier_.Transition.StateAfter = After;

		// TransitionBarrierを張る
		commandManager_->GetCommandList()->ResourceBarrier(1, &barrier_);
	}

	void DirectXCommon::BarrierTypeUAV(ID3D12Resource* pResource)
	{
		// バリアの設定
		barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier_.Transition.pResource = pResource;
		// Barrierを張る
		commandManager_->GetCommandList()->ResourceBarrier(1, &barrier_);
	}

	// 互換性のためのアクセッサ実装
	D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetOffScreenGPUHandle()
	{
		auto* rt = rtvManager_->Get("OffScreen");
		return rt ? rt->srvHandleGPU : D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetOffScreenCPUHandle()
	{
		auto* rt = rtvManager_->Get("OffScreen");
		return rt ? rt->srvHandleCPU : D3D12_CPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetDepthGPUHandle()
	{
		auto* ds = dsvManager_->Get("MainDepth");
		return ds ? ds->srvHandleGPU : D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDepthCPUHandle()
	{
		auto* ds = dsvManager_->Get("MainDepth");
		return ds ? ds->srvHandleCPU : D3D12_CPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetShadowDepthGPUHandle()
	{
		auto* ds = dsvManager_->Get("ShadowDepth");
		return ds ? ds->srvHandleGPU : D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetShadowDepthCPUHandle()
	{
		auto* ds = dsvManager_->Get("ShadowDepth");
		return ds ? ds->srvHandleCPU : D3D12_CPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetFinalResultGPUHandle()
	{
		auto* rt = rtvManager_->Get("FinalResult");
		return rt ? rt->srvHandleGPU : D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetFinalResultCPUHandle()
	{
		auto* rt = rtvManager_->Get("FinalResult");
		return rt ? rt->srvHandleCPU : D3D12_CPU_DESCRIPTOR_HANDLE{};
	}
}
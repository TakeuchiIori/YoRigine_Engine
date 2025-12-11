#pragma once
// C++
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include <chrono>

// Engine
#include "WinApp/WinApp.h"
#include "DeviceManager.h"
#include "CommandManager.h"
#include "SwapChainManager.h"
#include "RTVManager.h"
#include "DSVManager.h"
#include "DescriptorHeap.h"

// DirectX
#include "DirectXTex.h"

// Math
#include "Vector4.h"

using namespace std;

namespace YoRigine {
	class SrvManager;
	/// <summary>
	/// Dxの基本機能クラス
	/// </summary>
	class DirectXCommon
	{
	public:
		///************************* 基本関数 *************************///

		static DirectXCommon* GetInstance();
		DirectXCommon() = default;
		~DirectXCommon() = default;
		void Initialize(WinApp* winApp);
		void Finalize();

		// 描画前処理
		void PreDrawShadow();
		void PreDrawOffScreen();
		void PreDraw();

		// 描画後処理
		void PostDraw();

		// バリア
		void TransitionBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After);
		void BarrierTypeUAV(ID3D12Resource* pResource);


		///************************* 外部で使用する処理 *************************///
		void ExecuteCommandList();
		void WaitForGPU();
		void ResetCommandList();



	private:
		///************************* 内部処理 *************************///

		// 各種初期化
		void InitializeManagers();
		void InitializeRenderTargets();
		void InitializeViewPortRectangle();
		void InitializeScissorRectangle();
		void CreateDXCompiler();
		void InitializeFixFPS();
		void UpdateFixFPS();
	public:
		///************************* 公開関数 *************************///

		// 描画設定
		void DepthBarrier();
		// 最終描画結果をバックバッファにコピー
		void CopyBackBufferToFinalResult();
		// シェーダーのコンパイル
		Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wstring& filePath, const wchar_t* profile);

		// バッファリソースの生成
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResourceUAV(size_t sizeInBytes);
	public:
		///************************* アクセッサ *************************///

		// デバイス関連
		Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() { return deviceManager_->GetDevice(); }
		Microsoft::WRL::ComPtr<IDxcUtils> GetDxcUtils() { return dxcUtils_; }
		Microsoft::WRL::ComPtr<IDxcCompiler3> GetDxcCompiler() { return dxcCompiler_; }
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> GetIncludeHandler() { return includeHandler_; }

		// コマンド関連
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() { return commandManager_->GetCommandList(); }
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() { return commandManager_->GetCommandQueue(); }
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetCommandAllocator() { return commandManager_->GetCurrentCommandAllocator(); }

		// スワップチェーン関連
		Microsoft::WRL::ComPtr<IDXGISwapChain4> GetSwapChain() { return swapChainManager_->GetSwapChain(); }
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> GetSwapChainResources() { return swapChainManager_->GetSwapChainResources(); }
		UINT GetBackBufferCount() const { return swapChainManager_->GetBackBufferCount(); }
		UINT GetCurrentBackBufferIndex() const { return swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex(); }

		// ディスクリプタヒープ
		YoRigine::DescriptorHeap* GetDescriptorHeap() { return descriptorHeap_.get(); }

		// マネージャー取得
		YoRigine::DeviceManager* GetDeviceManager() { return deviceManager_.get(); }
		YoRigine::SrvManager* GetSrvManager() { return srvManager_; }
		YoRigine::RtvManager* GetRTVManager() { return rtvManager_.get(); }
		YoRigine::DsvManager* GetDSVManager() { return dsvManager_.get(); }

		// オフスクリーン関連
		D3D12_GPU_DESCRIPTOR_HANDLE GetOffScreenGPUHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE GetOffScreenCPUHandle();

		// 深度バッファ関連
		D3D12_GPU_DESCRIPTOR_HANDLE GetDepthGPUHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE GetDepthCPUHandle();
		D3D12_GPU_DESCRIPTOR_HANDLE GetShadowDepthGPUHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE GetShadowDepthCPUHandle();

		// 最終描画結果
		D3D12_GPU_DESCRIPTOR_HANDLE GetFinalResultGPUHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE GetFinalResultCPUHandle();

	private:
		static std::unique_ptr<DirectXCommon> instance;
		static std::once_flag initInstanceFlag;
		// シングルトンのコピー・ムーブ操作を削除
		DirectXCommon(const DirectXCommon&) = delete;
		DirectXCommon& operator=(const DirectXCommon&) = delete;
		DirectXCommon(DirectXCommon&&) = delete;
		DirectXCommon& operator=(DirectXCommon&&) = delete;


		///************************* メンバ変数 *************************///

		// WindowsAPI
		YoRigine::WinApp* winApp_ = nullptr;

		// 記録時間（FPS固定用）
		std::chrono::steady_clock::time_point reference_;
		const std::chrono::microseconds kMinTime{ static_cast<uint64_t>(1000000.0f / 60.0f) };
		const std::chrono::microseconds kMinCheckTime{ static_cast<uint64_t>(1000000.0f / 61.0f) };

		// 各種マネージャー
		std::unique_ptr<YoRigine::DeviceManager> deviceManager_;
		std::unique_ptr<YoRigine::CommandManager> commandManager_;
		std::unique_ptr<YoRigine::SwapChainManager> swapChainManager_;
		std::unique_ptr<YoRigine::DescriptorHeap> descriptorHeap_;

		YoRigine::SrvManager* srvManager_ = nullptr;
		std::unique_ptr<YoRigine::RtvManager> rtvManager_;
		std::unique_ptr<YoRigine::DsvManager> dsvManager_;


		// 描画設定
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};

		// シャドウマップ用
		D3D12_VIEWPORT shadowVP_{};
		D3D12_RECT shadowSC_{};

		D3D12_RESOURCE_BARRIER barrier_{};

		// シェーダー関連
		Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
		Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
		IDxcIncludeHandler* includeHandler_ = nullptr;

		// バリアの状態管理
		D3D12_RESOURCE_STATES depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		D3D12_RESOURCE_STATES shadowDepthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		D3D12_RESOURCE_STATES finalResultCurrentState_ = D3D12_RESOURCE_STATE_GENERIC_READ;

	};
}
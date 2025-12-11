#pragma once

// Engine
#include "FrameContext.h"

// C++
#include <d3d12.h>
#include <wrl.h>
#include <array>

class DeviceManager;

namespace YoRigine {
	/// <summary>
	/// コマンド管理クラス
	/// </summary>
	class CommandManager
	{
	public:

		// バックバッファ数（ダブルバッファだから2）
		static constexpr uint32_t kFrameCount = 2;

		///************************* 基本関数 *************************///

		/// <summary>
		/// 初期化
		/// </summary>
		/// <param name="deviceManager"></param>
		void Initialize(DeviceManager* deviceManager);

		/// <summary>
		/// 終了
		/// </summary>
		void Finalize();

		/// <summary>
		/// フレーム開始
		/// </summary>
		/// <param name="frameindex"></param>
		void BeginFrame(uint32_t frameindex);

		/// <summary>
		/// フレームの終了
		/// </summary>
		void EndFrame();

		/// <summary>
		/// 全フレームの完了待機
		/// </summary>
		void WaitForAllFrames();

		/// <summary>
		/// 現在のフレームの完了待機
		/// </summary>
		void WaitForCurrentFrame();

		/// <summary>
		/// コマンドリストのリセット
		/// </summary>
		void Reset(uint32_t frameindex);

	private:
		///************************* 内部処理 *************************///

		/// <summary>
		/// コマンドリストの生成
		/// </summary>
		void CreateCommands();

		/// <summary>
		/// フェンスの生成
		/// </summary>
		void CreateFecnce();

		/// <summary>
		/// フレームコンテキストの生成
		/// </summary>
		void InitializeFrameContexts();

	public:
		///************************* アクセッサ *************************///


		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() { return commandList_; }
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() { return commandQueue_; }
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetCurrentCommandAllocator()
		{
			return frameContexts_[currentFrameIndex_].commandAllocator;
		}

		Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() { return fence_; }
		uint64_t GetFenceValue() { return fenceValue_; }
		HANDLE GetFenceEvent() { return fenceEvent_; }
		uint32_t GetCurrentFrameIndex() const { return currentFrameIndex_; }

	private:
		///************************* メンバ変数 *************************///

		DeviceManager* deviceManager_ = nullptr;

		// コマンドキュー
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
		// コマンドリスト
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
		// フェンス
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		uint64_t fenceValue_ = 0;
		HANDLE fenceEvent_ = nullptr;

		// フレームコンテキストの配列
		std::array<FrameComtext, kFrameCount> frameContexts_;

		// 現在のフレームインデックス
		uint32_t currentFrameIndex_ = 0;

		// 初回フレームかどうか
		bool isFirstFrame_ = true;
	};
}

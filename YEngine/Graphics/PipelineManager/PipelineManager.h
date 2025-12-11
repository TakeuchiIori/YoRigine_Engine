#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <string>
#include <dxcapi.h>
#include <vector>
#include <unordered_map>
#include <array>
#include <functional>

// Engine
#include "Loaders/Json/EnumUtils.h"

class DirectXCommon;
/// <summary>
/// 全体のパイプライン管理クラス
/// </summary>
class PipelineManager
{
public:
	///************************* 基本関数 *************************///
	// コンストラクタとデストラクタ
	PipelineManager() = default;
	~PipelineManager() = default;
	static  PipelineManager* GetInstance();
	void Initialize();
	D3D12_BLEND_DESC GetBlendDesc(BlendMode _mode);
	ID3D12RootSignature* GetRootSignature(const std::string& key);
	ID3D12PipelineState* GetPipeLineStateObject(const std::string& key);
	void Finalize();

private:

	///************************* パイプライン生成 *************************///

	// スプライト用
	void CreatePSO_Sprite();

	// オブジェクト
	void CreatePSO_Object();
	void CreatePSO_ObjectInstance();

	// アニメーション
	void CreatePSO_SkinningCS();

	// ライン用
	void CreatePSO_Line();

	// パーティクル
	void CreatePSO_Particle();

	// キューブマップ
	void CreatePSO_CubeMap();
	///************************* ポストエフェクト *************************///

	void CreatePSO_BaseOffScreen(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_Smoothing(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_Edge(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_DepthOutLine(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_RadialBlur(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_ToneMapping(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_Dissolve(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_Chromatic(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_ColorAdjust(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);
	void CreatePSO_ShatterTransition(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);


	///************************* GPUパーティクル *************************///
	void CreatePSO_GPUParticleInit();


public:
	// パーティクル用のInputLayoutを取得
	const D3D12_INPUT_LAYOUT_DESC& GetParticleInputLayoutDesc() const {
		return particleInputLayoutDesc_;
	}

	ID3D12PipelineState* GetBlendModePSO(BlendMode blendMode);
	ID3D12PipelineState* GetBlendModeGPU(BlendMode blendMode);

	// 現在のパーティクル用 PSO を取得
	ID3D12PipelineState* GetCurrentParticlePSO() {
		return GetBlendModePSO(blendMode_);
	}

	// パーティクル用のブレンドモードを設定
	void SetParticleBlendMode(BlendMode mode) {
		if (blendMode_ != mode) {
			blendMode_ = mode;
		}
	}



private:
	// シングルトン
	PipelineManager(const  PipelineManager&) = delete;
	PipelineManager& operator=(const  PipelineManager&) = delete;
	PipelineManager(PipelineManager&&) = delete;
	PipelineManager& operator=(PipelineManager&&) = delete;
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;
	std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> blendModePipelineStates_;
	std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> blendModePipelineStatesGPU_;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
	BlendMode blendMode_{};


	// パーティクルで使用
	std::array<D3D12_INPUT_ELEMENT_DESC, 3> particleInputElements_;
	D3D12_RASTERIZER_DESC particleRasterrizerDesc_{};
	D3D12_DEPTH_STENCIL_DESC particleDepthStencilDesc_{};
	D3D12_INPUT_LAYOUT_DESC particleInputLayoutDesc_{};
	Microsoft::WRL::ComPtr<IDxcBlob> particleVertexShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> particlePixelShaderBlob_;

	// GPU
	BlendMode blendModeGPU_{};
	std::array<D3D12_INPUT_ELEMENT_DESC, 3> particleInputElementsGPU_;
	D3D12_RASTERIZER_DESC particleRasterrizerDescGPU_{};
	D3D12_DEPTH_STENCIL_DESC particleDepthStencilDescGPU_{};
	D3D12_INPUT_LAYOUT_DESC particleInputLayoutDescGPU_{};
	Microsoft::WRL::ComPtr<IDxcBlob> particleVertexShaderBlobGPU_;
	Microsoft::WRL::ComPtr<IDxcBlob> particlePixelShaderBlobGPU_;

};


#include "ShadowPipeline.h"
#include "Debugger/Logger.h"
#include "d3dx12.h"
#include <d3d12.h>
#include <wrl.h>

ShadowPipeline* ShadowPipeline::GetInstance()
{
	static ShadowPipeline instance;
	return &instance;
}

void ShadowPipeline::Initialize()
{
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();
	CreateShadowmapPSO();
}

void ShadowPipeline::Finalize()
{	// パイプラインステートオブジェクトの解放
	for (auto& pso : pipelineStates_) {
		pso.second.Reset();
	}
	pipelineStates_.clear();
	// ルートシグネチャの解放
	for (auto& rs : rootSignatures_) {
		rs.second.Reset();
	}
	rootSignatures_.clear();
}

ID3D12RootSignature* ShadowPipeline::GetRootSignature(const std::string& key)
{
	auto it = rootSignatures_.find(key);
	if (it != rootSignatures_.end()) {
		return rootSignatures_[key].Get();
	}
	else {
		return nullptr;
	}
}

ID3D12PipelineState* ShadowPipeline::GetPipeLineStateObject(const std::string& key)
{
	auto it = pipelineStates_.find(key);
	if (it != pipelineStates_.end()) {
		return pipelineStates_[key].Get();
	}
	else {
		return nullptr;
	}
}

void ShadowPipeline::CreateShadowmapPSO()
{
    auto device = dxCommon_->GetDevice();

    D3D12_ROOT_PARAMETER rootParams[2]{};

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams[0].Descriptor.ShaderRegister = 0;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams[1].Descriptor.ShaderRegister = 1;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
    rootDesc.NumParameters = _countof(rootParams);
    rootDesc.pParameters = rootParams;
    rootDesc.NumStaticSamplers = 0;
    rootDesc.pStaticSamplers = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &sigBlob,
        &errBlob
    );
    if (FAILED(hr)) {
        if (errBlob) {
            Logger((char*)errBlob->GetBufferPointer());
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignatures_["Shadowmap"])
    );
    assert(SUCCEEDED(hr));

    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Shadow/ShadowMap.VS.hlsl", L"vs_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignatures_["Shadowmap"].Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = {nullptr,0 };
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.NumRenderTargets = 0;

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStates_["Shadowmap"]));
    assert(SUCCEEDED(hr));
}

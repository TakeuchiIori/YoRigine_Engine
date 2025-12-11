#pragma once

#include <d3d12.h>
#include "DirectXCommon.h"

/// <summary>
/// シャドウマップ用パイプライン
/// </summary>
class ShadowPipeline
{
public:
    // コンストラクタとデストラクタ
    ShadowPipeline() = default;
    ~ShadowPipeline() = default;
    static  ShadowPipeline* GetInstance();

    /// <summary>
    /// 初期化
    /// </summary>
    /// <param name="dxCommon">DirectXCommonへのポインタ</param>
    void Initialize();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    /// <summary>
    /// ルートシグネチャ取得
    /// </summary>
    ID3D12RootSignature* GetRootSignature(const std::string& key);

    /// <summary>
    /// パイプラインステート取得
    /// </summary>
    ID3D12PipelineState* GetPipeLineStateObject(const std::string& key);


private:

    /// <summary>
	/// シャドウマップ用PSO生成
    /// </summary>
    void CreateShadowmapPSO();

private:
	// シングルトン
    ShadowPipeline(const  ShadowPipeline&) = delete;
    ShadowPipeline& operator=(const  ShadowPipeline&) = delete;
    ShadowPipeline(ShadowPipeline&&) = delete;
    ShadowPipeline& operator=(ShadowPipeline&&) = delete;

	///************************* メンバ変数 *************************///
    DirectXCommon* dxCommon_ = nullptr;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
};

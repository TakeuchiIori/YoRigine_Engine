#include "LineManager.h"
#include "DirectXCommon.h"
#include "PipelineManager/PipelineManager.h"

/// <summary>
/// LineManager のシングルトンインスタンスを取得
/// </summary>
LineManager* LineManager::GetInstance()
{
	static LineManager instance;
	return &instance;
}

/// <summary>
/// LineManager の初期化処理
/// ・DirectXCommon の取得
/// ・ライン描画用パイプライン（RootSignature / PSO）の読み込み
/// </summary>
void LineManager::Initialize()
{
	// ポインタを渡す
	dxCommon_ = DirectXCommon::GetInstance();

	// Line 用パイプラインリソース取得
	rootSignature_ = PipelineManager::GetInstance()->GetRootSignature("Line");
	graphicsPipelineState_ = PipelineManager::GetInstance()->GetPipeLineStateObject("Line");
}

#include "SpriteCommon.h"

void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
	// 引数を受け取る
	dxCommon_ = dxCommon;
	// グラフィックパイプラインの生成
	CreateGraphicsPipelineState();
}

void SpriteCommon::CommonDrawSetting()
{
    // コマンドリスト取得
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // 1. ルートシグネチャをセット
    commandList->SetGraphicsRootSignature(rootSignature_);

    // 2. グラフィックスパイプラインステート（PSO）をセット
    commandList->SetPipelineState(pipelineState_);

    // 3. プリミティブトポロジー設定（三角形リスト）
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}

void SpriteCommon::CreateRootSignature()
{
}

void SpriteCommon::CreateGraphicsPipelineState()
{
}

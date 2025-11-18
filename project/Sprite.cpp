#include "Sprite.h"
#include "SpriteCommon.h"
#include "WinApp.h"
#include "Math.h"

void Sprite::Initialize(SpriteCommon* spriteCommon, ID3D12Resource* directionalLightResource)
{
	// 引数をメンバ変数にセット
	spriteCommon_ = spriteCommon;
	// ライト情報リソースをセット
	directionalLightResource_ = directionalLightResource;

	// 頂点データ作成
	CreateVertexData();
	// マテリアルデータ作成
	CreateMaterialData();
	// 座標変換行列データ作成
	CreateTransformationMatrixData();

}

void Sprite::Update()
{
	// ============================
	// 頂点リソースにデータを書き込む（4点分）
	// インデックスリソースにデータを書き込む（6個分）
	// ============================

	// インデックス（2枚の三角形で四角形を描画）
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 1;
	indexData[4] = 3;
	indexData[5] = 2;

	// 左上
	vertexData[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexData[0].texcoord = { 0.0f, 1.0f };
	vertexData[0].normal = { 0.0f, 0.0f, -1.0f };

	// 左下
	vertexData[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexData[1].texcoord = { 0.0f, 0.0f };
	vertexData[1].normal = { 0.0f, 0.0f, -1.0f };

	// 右上
	vertexData[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexData[2].texcoord = { 1.0f, 1.0f };
	vertexData[2].normal = { 0.0f, 0.0f, -1.0f };

	// 右下
	vertexData[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexData[3].texcoord = { 1.0f, 0.0f };
	vertexData[3].normal = { 0.0f, 0.0f, -1.0f };

	// ============================
	// Transform情報を作る → 行列を作って ConstantBuffer に書き込む
	// ============================

	// ひとまず固定の Transform（必要になったらメンバにして外から変更）
	transform_.translate = { position_.x, position_.y, 0.0f };
	transform_.rotate = { 0.0f, 0.0f, rotation_ };
	transform_.scale = { size_.x, size_.y, 1.0f };

	// World 行列
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	// Sprite はビュー行列は単位行列でOK
	Matrix4x4 viewMatrix = MakeIdentity4x4();

	// 画面サイズに合わせた正射影行列
	Matrix4x4 projectionMatrix =
		MakeOrthographicMatrix(
			0.0f, 0.0f,
			static_cast<float>(WinApp::kClientWidth),
			static_cast<float>(WinApp::kClientHeight),
			0.0f, 100.0f);

	// WVP 行列
	Matrix4x4 wvpMatrix =
		Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

	// ConstantBuffer に書き込む
	transformationMatrixData->WVP = wvpMatrix;
	transformationMatrixData->World = worldMatrix;
}

void Sprite::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrv)
{
	// DirectXCommon & コマンドリスト取得
	DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	// スプライト共通描画設定
	// ルートシグネチャ・PSO・プリミティブトポロジの設定
	spriteCommon_->CommonDrawSetting();

	// ===== VertexBufferView / IndexBufferView を設定 =====
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);

	// ===== 定数バッファ（Material / TransformationMatrix）を設定 =====
	// b0 : Material
	commandList->SetGraphicsRootConstantBufferView(
		0, materialResource->GetGPUVirtualAddress());

	commandList->SetGraphicsRootConstantBufferView(
		1, directionalLightResource_->GetGPUVirtualAddress());

	// b2 : TransformationMatrix
	commandList->SetGraphicsRootConstantBufferView(
		2, transformationMatrixResource->GetGPUVirtualAddress());

	// ===== SRV の DescriptorTable を設定 =====
	// t0 を束ねているテーブルの 3 番目の RootParameter に設定（main と同じ）
	commandList->SetGraphicsRootDescriptorTable(3, textureSrv);

	// ===== 描画！（DrawCall） =====
	// インデックス 6 個で四角形 1 枚分
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateVertexData()
{
	// DirectXCommon を取り出す
	DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

	// VertexResource を作る
	vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * 4);

	// IndexResource を作る
	indexResource = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);

	// VertexBufferView を作成
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;

	// IndexBufferView を作成
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// 頂点データ用のアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// インデックスデータ用のアドレスを取得
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

	// （ここから先の position / texcoord / index の中身を書くのは
	//  次のスライドのタイミングでOK）
}


void Sprite::CreateMaterialData()
{
	// DirectXCommon を取得
	DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

	// ========================
	// マテリアルリソース（ConstantBuffer）を作る
	// ========================
	materialResource = dxCommon->CreateBufferResource(sizeof(Material));

	// ========================
	// マテリアルリソースを Map して materialData に割り当てる
	// ========================
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// ========================
	// マテリアル初期値を書き込む（資料そのまま）
	// ========================
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->lightingType = 0;
	materialData->uvTransform = MakeIdentity4x4();
}

void Sprite::CreateTransformationMatrixData()
{
	// DirectXCommon を SpriteCommon 経由で取得
	DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

	// 座標変換行列リソース（ConstantBuffer）を作る
	transformationMatrixResource =
		dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

	// 書き込むためのアドレスを取得して transformationMatrixData に割り当てる
	transformationMatrixResource->Map(
		0, nullptr,
		reinterpret_cast<void**>(&transformationMatrixData));

	// 単位行列を書きこんでおく（資料の通り）
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();
}
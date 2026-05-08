struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
    float pad;
};

struct VertexInfluence
{
    float4 weight;
    int4 index;
};

struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);

cbuffer SkinningInformation : register(b0)
{
    uint numVertices;
    uint3 padding;
};

[numthreads(256, 1, 1)]

void main(uint3 DTid : SV_DispatchThreadID)
{
    // 今回処理する頂点番号
    uint vertexIndex = DTid.x;

    // 頂点数を超えた thread は何もしない
    if (vertexIndex >= numVertices)
    {
        return;
    }

    // 入力頂点と influence を読む
    Vertex inputVertex = gInputVertices[vertexIndex];
    VertexInfluence influence = gInfluences[vertexIndex];

    // 変形後頂点を作る
    Vertex outputVertex;
    outputVertex.texcoord = inputVertex.texcoord;
    outputVertex.pad = inputVertex.pad;

    // position を 4 本のボーンでスキニングする
    outputVertex.position =
        mul(inputVertex.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix) * influence.weight.x;
    outputVertex.position +=
        mul(inputVertex.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix) * influence.weight.y;
    outputVertex.position +=
        mul(inputVertex.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix) * influence.weight.z;
    outputVertex.position +=
        mul(inputVertex.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix) * influence.weight.w;

    // position の w は 1 に戻しておく
    outputVertex.position.w = 1.0f;

    // normal も 4 本のボーンでスキニングする
    outputVertex.normal =
        mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.x].skeletonSpaceInverseTransposeMatrix) * influence.weight.x;
    outputVertex.normal +=
        mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.y].skeletonSpaceInverseTransposeMatrix) * influence.weight.y;
    outputVertex.normal +=
        mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.z].skeletonSpaceInverseTransposeMatrix) * influence.weight.z;
    outputVertex.normal +=
        mul(inputVertex.normal, (float3x3) gMatrixPalette[influence.index.w].skeletonSpaceInverseTransposeMatrix) * influence.weight.w;

    // 法線を正規化する
    outputVertex.normal = normalize(outputVertex.normal);

    // 結果を書き込む
    gOutputVertices[vertexIndex] = outputVertex;
}

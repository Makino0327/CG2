#include "CopyImage.hlsli"

static const float32_t4 kPositions[3] =
{
    float32_t4(-1.0f, -1.0f, 0.0f, 1.0f),
    float32_t4(-1.0f, 3.0f, 0.0f, 1.0f),
    float32_t4(3.0f, -1.0f, 0.0f, 1.0f),
};

static const float32_t2 kTexcoords[3] =
{
    float32_t2(0.0f, 1.0f),
    float32_t2(0.0f, -1.0f),
    float32_t2(2.0f, 1.0f),
};

VertexShaderOutput main(uint32_t vertexId : SV_VertexID)
{
    VertexShaderOutput output;
    output.position = kPositions[vertexId];
    output.texcoord = kTexcoords[vertexId];
    return output;
}

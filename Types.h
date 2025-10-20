#pragma once

enum class DisplayMode {
	Sprite,
	Sphere,
	Teapot,
	Bunny,
	MultiMesh,
	Count
};

enum class LightingType {
	None = 0,
	Lambert,
	HalfLambert
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	float pad;
};

struct MeshData {
	std::string name;
	std::vector<VertexData> vertices;
};

struct ModelData {
	std::vector<MeshData> meshes;
};

struct Material {
	Vector4 color;
	int32_t lightingType;     
	float padding[3];         
	Matrix4x4 uvTransform;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight {
	Vector4 color;        // ライトの色
	Vector3 direction;    // ライトの向き（単位ベクトル）
	float intensity;      // 強度
};
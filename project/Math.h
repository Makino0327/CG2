#pragma once
struct Vector4 {
	float x, y, z, w;
};

struct Vector3 {
	float x, y, z;
};

struct Vector2 {
	float x, y;
};

struct Matrix4x4 {
	float m[4][4];
};

struct Matrix3x3 {
	float m[3][3];
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

Matrix4x4 MakeIdentity4x4();
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearZ, float farZ);
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearZ, float farZ);
Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b);
Matrix4x4 Inverse(const Matrix4x4& m);
Vector3 Normalize(const Vector3& v);
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
Matrix4x4 MakeRotateZMatrix(float angle);
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);


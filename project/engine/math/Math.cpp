#include "Math.h"
#include <cmath>

Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result = {};

	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{
	Matrix4x4 matrix = {};
	float cosX = cosf(rotate.x);
	float sinX = sinf(rotate.x);
	float cosY = cosf(rotate.y);
	float sinY = sinf(rotate.y);
	float cosZ = cosf(rotate.z);
	float sinZ = sinf(rotate.z);
	matrix.m[0][0] = scale.x * (cosY * cosZ);
	matrix.m[0][1] = scale.x * (cosY * sinZ);
	matrix.m[0][2] = scale.x * (-sinY);
	matrix.m[0][3] = 0.0f;
	matrix.m[1][0] = scale.y * (sinX * sinY * cosZ - cosX * sinZ);
	matrix.m[1][1] = scale.y * (sinX * sinY * sinZ + cosX * cosZ);
	matrix.m[1][2] = scale.y * (sinX * cosY);
	matrix.m[1][3] = 0.0f;
	matrix.m[2][0] = scale.z * (cosX * sinY * cosZ + sinX * sinZ);
	matrix.m[2][1] = scale.z * (cosX * sinY * sinZ - sinX * cosZ);
	matrix.m[2][2] = scale.z * (cosX * cosY);
	matrix.m[2][3] = 0.0f;
	matrix.m[3][0] = translate.x;
	matrix.m[3][1] = translate.y;
	matrix.m[3][2] = translate.z;
	matrix.m[3][3] = 1.0f;
	return matrix;
}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearZ, float farZ) {
	Matrix4x4 m{};
	float yScale = 1.0f / tanf(fovY / 2.0f);
	float xScale = yScale / aspect;
	float range = farZ - nearZ;

	m.m[0][0] = xScale;
	m.m[1][1] = yScale;
	m.m[2][2] = farZ / range;
	m.m[2][3] = 1.0f;
	m.m[3][2] = -nearZ * farZ / range;

	return m;
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearZ, float farZ) {
	Matrix4x4 result{};

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farZ - nearZ);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearZ / (nearZ - farZ);
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
	Matrix4x4 r{};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			for (int k = 0; k < 4; ++k) {
				r.m[row][col] += a.m[row][k] * b.m[k][col];
			}
		}
	}
	return r;
}

Matrix4x4 Inverse(const Matrix4x4& m)
{
	Matrix4x4 result;
	float* inv = &result.m[0][0];
	const float* mat = &m.m[0][0];

	float invOut[16];

	invOut[0] = mat[5] * mat[10] * mat[15] -
		mat[5] * mat[11] * mat[14] -
		mat[9] * mat[6] * mat[15] +
		mat[9] * mat[7] * mat[14] +
		mat[13] * mat[6] * mat[11] -
		mat[13] * mat[7] * mat[10];

	invOut[1] = -mat[1] * mat[10] * mat[15] +
		mat[1] * mat[11] * mat[14] +
		mat[9] * mat[2] * mat[15] -
		mat[9] * mat[3] * mat[14] -
		mat[13] * mat[2] * mat[11] +
		mat[13] * mat[3] * mat[10];

	invOut[2] = mat[1] * mat[6] * mat[15] -
		mat[1] * mat[7] * mat[14] -
		mat[5] * mat[2] * mat[15] +
		mat[5] * mat[3] * mat[14] +
		mat[13] * mat[2] * mat[7] -
		mat[13] * mat[3] * mat[6];

	invOut[3] = -mat[1] * mat[6] * mat[11] +
		mat[1] * mat[7] * mat[10] +
		mat[5] * mat[2] * mat[11] -
		mat[5] * mat[3] * mat[10] -
		mat[9] * mat[2] * mat[7] +
		mat[9] * mat[3] * mat[6];

	invOut[4] = -mat[4] * mat[10] * mat[15] +
		mat[4] * mat[11] * mat[14] +
		mat[8] * mat[6] * mat[15] -
		mat[8] * mat[7] * mat[14] -
		mat[12] * mat[6] * mat[11] +
		mat[12] * mat[7] * mat[10];

	invOut[5] = mat[0] * mat[10] * mat[15] -
		mat[0] * mat[11] * mat[14] -
		mat[8] * mat[2] * mat[15] +
		mat[8] * mat[3] * mat[14] +
		mat[12] * mat[2] * mat[11] -
		mat[12] * mat[3] * mat[10];

	invOut[6] = -mat[0] * mat[6] * mat[15] +
		mat[0] * mat[7] * mat[14] +
		mat[4] * mat[2] * mat[15] -
		mat[4] * mat[3] * mat[14] -
		mat[12] * mat[2] * mat[7] +
		mat[12] * mat[3] * mat[6];

	invOut[7] = mat[0] * mat[6] * mat[11] -
		mat[0] * mat[7] * mat[10] -
		mat[4] * mat[2] * mat[11] +
		mat[4] * mat[3] * mat[10] +
		mat[8] * mat[2] * mat[7] -
		mat[8] * mat[3] * mat[6];

	invOut[8] = mat[4] * mat[9] * mat[15] -
		mat[4] * mat[11] * mat[13] -
		mat[8] * mat[5] * mat[15] +
		mat[8] * mat[7] * mat[13] +
		mat[12] * mat[5] * mat[11] -
		mat[12] * mat[7] * mat[9];

	invOut[9] = -mat[0] * mat[9] * mat[15] +
		mat[0] * mat[11] * mat[13] +
		mat[8] * mat[1] * mat[15] -
		mat[8] * mat[3] * mat[13] -
		mat[12] * mat[1] * mat[11] +
		mat[12] * mat[3] * mat[9];

	invOut[10] = mat[0] * mat[5] * mat[15] -
		mat[0] * mat[7] * mat[13] -
		mat[4] * mat[1] * mat[15] +
		mat[4] * mat[3] * mat[13] +
		mat[12] * mat[1] * mat[7] -
		mat[12] * mat[3] * mat[5];

	invOut[11] = -mat[0] * mat[5] * mat[11] +
		mat[0] * mat[7] * mat[9] +
		mat[4] * mat[1] * mat[11] -
		mat[4] * mat[3] * mat[9] -
		mat[8] * mat[1] * mat[7] +
		mat[8] * mat[3] * mat[5];

	invOut[12] = -mat[4] * mat[9] * mat[14] +
		mat[4] * mat[10] * mat[13] +
		mat[8] * mat[5] * mat[14] -
		mat[8] * mat[6] * mat[13] -
		mat[12] * mat[5] * mat[10] +
		mat[12] * mat[6] * mat[9];

	invOut[13] = mat[0] * mat[9] * mat[14] -
		mat[0] * mat[10] * mat[13] -
		mat[8] * mat[1] * mat[14] +
		mat[8] * mat[2] * mat[13] +
		mat[12] * mat[1] * mat[10] -
		mat[12] * mat[2] * mat[9];

	invOut[14] = -mat[0] * mat[5] * mat[14] +
		mat[0] * mat[6] * mat[13] +
		mat[4] * mat[1] * mat[14] -
		mat[4] * mat[2] * mat[13] -
		mat[12] * mat[1] * mat[6] +
		mat[12] * mat[2] * mat[5];

	invOut[15] = mat[0] * mat[5] * mat[10] -
		mat[0] * mat[6] * mat[9] -
		mat[4] * mat[1] * mat[10] +
		mat[4] * mat[2] * mat[9] +
		mat[8] * mat[1] * mat[6] -
		mat[8] * mat[2] * mat[5];

	float det = mat[0] * invOut[0] + mat[1] * invOut[4] + mat[2] * invOut[8] + mat[3] * invOut[12];
	if (det == 0.0f)
	{
		// 逆行列なし（特異行列）
		return MakeIdentity4x4(); // または assert, エラーログ等
	}

	float invDet = 1.0f / det;
	for (int i = 0; i < 16; ++i)
	{
		inv[i] = invOut[i] * invDet;
	}

	return result;
}

Vector3 Normalize(const Vector3& v) {
	float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	if (length == 0.0f) return { 0.0f, 0.0f, 0.0f };
	return { v.x / length, v.y / length, v.z / length };
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result = {};
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 MakeRotateZMatrix(float angle) {
	Matrix4x4 result = {};
	float c = cosf(angle);
	float s = sinf(angle);

	result.m[0][0] = c;
	result.m[0][1] = -s;
	result.m[1][0] = s;
	result.m[1][1] = c;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;

	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	return result;
}

Vector3 Add(const Vector3& a, const Vector3& b) {
	return {
		a.x + b.x,
		a.y + b.y,
		a.z + b.z
	};
}

Vector3 AddVector(const Vector3& v, float scalar) {
	return {
		v.x * scalar,
		v.y * scalar,
		v.z * scalar
	};
}

Matrix4x4 MakeRotateYMatrix(float radian)
{
	Matrix4x4 result = MakeIdentity4x4();

	float c = std::cos(radian);
	float s = std::sin(radian);

	// 左手座標系用のY回転
	result.m[0][0] = c;
	result.m[0][2] = s;
	result.m[2][0] = -s;
	result.m[2][2] = c;

	return result;
}

Quaternion Normalize(const Quaternion& q) {
	float length = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	if (length == 0.0f) {
		return { 0.0f, 0.0f, 0.0f, 1.0f };
	}
	return { q.x / length, q.y / length, q.z / length, q.w / length };
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t
	};
}

float Dot(const Quaternion& q0, const Quaternion& q1) {
	return q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;
}

Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t) {
	Quaternion qStart = Normalize(q0);
	Quaternion qEnd = Normalize(q1);

	float dot = Dot(qStart, qEnd);

	// 最短経路で補間するため、逆向きなら片方を反転する
	if (dot < 0.0f) {
		dot = -dot;
		qEnd.x = -qEnd.x;
		qEnd.y = -qEnd.y;
		qEnd.z = -qEnd.z;
		qEnd.w = -qEnd.w;
	}

	// ほぼ同じ向きなら線形補間で十分
	if (dot > 0.9995f) {
		Quaternion result = {
			qStart.x + (qEnd.x - qStart.x) * t,
			qStart.y + (qEnd.y - qStart.y) * t,
			qStart.z + (qEnd.z - qStart.z) * t,
			qStart.w + (qEnd.w - qStart.w) * t
		};
		return Normalize(result);
	}

	float theta = acosf(dot);
	float sinTheta = sinf(theta);

	float weight0 = sinf((1.0f - t) * theta) / sinTheta;
	float weight1 = sinf(t * theta) / sinTheta;

	Quaternion result = {
		qStart.x * weight0 + qEnd.x * weight1,
		qStart.y * weight0 + qEnd.y * weight1,
		qStart.z * weight0 + qEnd.z * weight1,
		qStart.w * weight0 + qEnd.w * weight1
	};
	return Normalize(result);
}

Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion)
{
	// 正規化した Quaternion から回転行列を作る
	Quaternion q = Normalize(quaternion);

	Matrix4x4 matrix = MakeIdentity4x4();

	matrix.m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	matrix.m[0][1] = 2.0f * (q.x * q.y + q.z * q.w);
	matrix.m[0][2] = 2.0f * (q.x * q.z - q.y * q.w);

	matrix.m[1][0] = 2.0f * (q.x * q.y - q.z * q.w);
	matrix.m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
	matrix.m[1][2] = 2.0f * (q.y * q.z + q.x * q.w);

	matrix.m[2][0] = 2.0f * (q.x * q.z + q.y * q.w);
	matrix.m[2][1] = 2.0f * (q.y * q.z - q.x * q.w);
	matrix.m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

	return matrix;
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate)
{
	// scale, rotate, translate からアフィン行列を作る
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
	Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

	return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
}

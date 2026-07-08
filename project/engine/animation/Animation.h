#pragma once

#include <map>
#include <string>
#include <vector>

#include "../math/Math.h"

template <typename TValue>
struct Keyframe {
	float time;
	TValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

template <typename TValue>
struct AnimationCurve {
	std::vector<Keyframe<TValue>> keyframes;
};

struct NodeAnimation {
	AnimationCurve<Vector3> translate;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vector3> scale;
};

struct Animation {
	float duration = 0.0f;
	std::map<std::string, NodeAnimation> nodeAnimations;
};

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename, uint32_t animationIndex = 0);
std::vector<std::string> GetAnimationNames(const std::string& directoryPath, const std::string& filename);
Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

#include "Animation.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <regex>

namespace {

	struct GltfAnimationSampler {
		uint32_t input = 0;
		uint32_t output = 0;
		std::string interpolation;
	};

	struct GltfAnimationChannel {
		uint32_t sampler = 0;
		uint32_t targetNode = 0;
		std::string path;
	};


	size_t FindMatchingBracket(const std::string& text, size_t openIndex, char openChar, char closeChar)
	{
		// 対応する括弧の終端位置を探す
		int depth = 0;
		for (size_t i = openIndex; i < text.size(); ++i) {
			if (text[i] == openChar) {
				++depth;
			} else if (text[i] == closeChar) {
				--depth;
				if (depth == 0) {
					return i;
				}
			}
		}
		assert(false);
		return std::string::npos;
	}
	size_t FindTopLevelKey(const std::string& jsonText, const std::string& key)
	{
		std::string target = "\"" + key + "\"";

		int objectDepth = 0;
		bool inString = false;

		for (size_t i = 0; i < jsonText.size(); ++i) {
			char c = jsonText[i];

			// 文字列の外でだけ深さを数える
			if (!inString) {
				if (c == '{') {
					++objectDepth;
				} else if (c == '}') {
					--objectDepth;
				}
			}

			// 最上位オブジェクト直下にあるキーだけ探す
			if (!inString && objectDepth == 1) {
				if (jsonText.compare(i, target.size(), target) == 0) {
					return i;
				}
			}

			// エスケープされていない " で文字列状態を切り替える
			if (c == '"' && (i == 0 || jsonText[i - 1] != '\\')) {
				inString = !inString;
			}
		}

		return std::string::npos;
	}

	std::string ExtractArrayBlock(const std::string& jsonText, const std::string& key)
	{
		// 最上位オブジェクト直下の key を探す
		size_t keyPos = FindTopLevelKey(jsonText, key);
		assert(keyPos != std::string::npos);

		// 対応する配列の開始位置を探す
		size_t arrayBegin = jsonText.find('[', keyPos);
		assert(arrayBegin != std::string::npos);

		// 対応する配列の終端まで切り出す
		size_t arrayEnd = FindMatchingBracket(jsonText, arrayBegin, '[', ']');
		return jsonText.substr(arrayBegin, arrayEnd - arrayBegin + 1);
	}


	std::vector<std::string> SplitTopLevelObjects(const std::string& arrayBlock)
	{
		// 配列の中にあるトップレベルの { ... } を1個ずつ分解する
		std::vector<std::string> objects;
		int depth = 0;
		size_t objectBegin = std::string::npos;

		for (size_t i = 0; i < arrayBlock.size(); ++i) {
			if (arrayBlock[i] == '{') {
				if (depth == 0) {
					objectBegin = i;
				}
				++depth;
			} else if (arrayBlock[i] == '}') {
				--depth;
				if (depth == 0 && objectBegin != std::string::npos) {
					objects.push_back(arrayBlock.substr(objectBegin, i - objectBegin + 1));
					objectBegin = std::string::npos;
				}
			}
		}

		return objects;
	}

	std::string FindStringValue(const std::string& objectText, const std::string& key)
	{
		// "key": "value" の文字列値を取得する
		std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
		std::smatch match;
		bool found = std::regex_search(objectText, match, pattern);
		assert(found);
		return match[1].str();
	}

	uint32_t FindUIntValue(const std::string& objectText, const std::string& key)
	{
		// "key": number の整数値を取得する
		std::regex pattern("\"" + key + "\"\\s*:\\s*(\\d+)");
		std::smatch match;
		bool found = std::regex_search(objectText, match, pattern);
		assert(found);
		return static_cast<uint32_t>(std::stoul(match[1].str()));
	}

	uint32_t FindUIntValueOrDefault(const std::string& objectText, const std::string& key, uint32_t defaultValue)
	{
		// 値があれば取得し、無ければ既定値を返す
		std::regex pattern("\"" + key + "\"\\s*:\\s*(\\d+)");
		std::smatch match;
		if (std::regex_search(objectText, match, pattern)) {
			return static_cast<uint32_t>(std::stoul(match[1].str()));
		}
		return defaultValue;
	}

	struct GltfAccessor {
		uint32_t bufferView = 0;
		uint32_t byteOffset = 0;
		uint32_t componentType = 0;
		uint32_t count = 0;
		std::string type;
	};

	struct GltfBufferView {
		uint32_t buffer = 0;
		uint32_t byteOffset = 0;
		uint32_t byteLength = 0;
	};

	std::vector<GltfAccessor> ParseAccessors(const std::string& jsonText)
	{
		// accessors 配列を構造体配列へ変換する
		std::vector<GltfAccessor> accessors;
		std::string block = ExtractArrayBlock(jsonText, "accessors");
		std::vector<std::string> objects = SplitTopLevelObjects(block);

		for (const std::string& objectText : objects) {
			GltfAccessor accessor;
			accessor.bufferView = FindUIntValue(objectText, "bufferView");
			accessor.byteOffset = FindUIntValueOrDefault(objectText, "byteOffset", 0);
			accessor.componentType = FindUIntValue(objectText, "componentType");
			accessor.count = FindUIntValue(objectText, "count");
			accessor.type = FindStringValue(objectText, "type");
			accessors.push_back(accessor);
		}

		return accessors;
	}

	std::vector<GltfBufferView> ParseBufferViews(const std::string& jsonText)
	{
		// bufferViews 配列を構造体配列へ変換する
		std::vector<GltfBufferView> bufferViews;
		std::string block = ExtractArrayBlock(jsonText, "bufferViews");
		std::vector<std::string> objects = SplitTopLevelObjects(block);

		for (const std::string& objectText : objects) {
			GltfBufferView bufferView;
			bufferView.buffer = FindUIntValue(objectText, "buffer");
			bufferView.byteOffset = FindUIntValueOrDefault(objectText, "byteOffset", 0);
			bufferView.byteLength = FindUIntValue(objectText, "byteLength");
			bufferViews.push_back(bufferView);
		}

		return bufferViews;
	}

	std::vector<uint8_t> ReadBinaryFile(const std::string& filePath)
	{
		// .bin を丸ごとメモリに読む
		std::ifstream file(filePath, std::ios::binary);
		assert(file.is_open());

		file.seekg(0, std::ios::end);
		size_t size = static_cast<size_t>(file.tellg());
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> data(size);
		file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
		return data;
	}

	std::vector<GltfAnimationSampler> ParseAnimationSamplers(const std::string& animationObject)
	{
		// samplers 配列を取り出して構造体配列にする
		std::vector<GltfAnimationSampler> samplers;

		std::string samplersBlock = ExtractArrayBlock(animationObject, "samplers");
		std::vector<std::string> samplerObjects = SplitTopLevelObjects(samplersBlock);

		for (const std::string& samplerObject : samplerObjects) {
			GltfAnimationSampler sampler;
			sampler.input = FindUIntValue(samplerObject, "input");
			sampler.output = FindUIntValue(samplerObject, "output");
			sampler.interpolation = FindStringValue(samplerObject, "interpolation");
			samplers.push_back(sampler);
		}

		return samplers;
	}

	std::vector<GltfAnimationChannel> ParseAnimationChannels(const std::string& animationObject)
	{
		// channels 配列を取り出して構造体配列にする
		std::vector<GltfAnimationChannel> channels;

		std::string channelsBlock = ExtractArrayBlock(animationObject, "channels");
		std::vector<std::string> channelObjects = SplitTopLevelObjects(channelsBlock);

		for (const std::string& channelObject : channelObjects) {
			GltfAnimationChannel channel;
			channel.sampler = FindUIntValue(channelObject, "sampler");
			channel.targetNode = FindUIntValue(channelObject, "node");
			channel.path = FindStringValue(channelObject, "path");
			channels.push_back(channel);
		}

		return channels;
	}

	std::string FindNodeName(const std::string& jsonText, uint32_t nodeIndex)
	{
		// nodes 配列から指定 index の node 名を取る
		std::string nodesBlock = ExtractArrayBlock(jsonText, "nodes");
		std::vector<std::string> nodeObjects = SplitTopLevelObjects(nodesBlock);

		assert(nodeIndex < nodeObjects.size());
		return FindStringValue(nodeObjects[nodeIndex], "name");
	}

}


Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename)
{
	Animation animation;

	// gltf本体をテキストとして読む
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string jsonText = buffer.str();

	// accessor と bufferView を先に配列化しておく
	std::vector<GltfAccessor> accessors = ParseAccessors(jsonText);
	std::vector<GltfBufferView> bufferViews = ParseBufferViews(jsonText);

	// buffers[0].uri を使って対応する .bin を読む
	std::string buffersBlock = ExtractArrayBlock(jsonText, "buffers");
	std::vector<std::string> bufferObjects = SplitTopLevelObjects(buffersBlock);
	assert(!bufferObjects.empty());

	std::string bufferUri = FindStringValue(bufferObjects[0], "uri");
	std::vector<uint8_t> binary = ReadBinaryFile(directoryPath + "/" + bufferUri);

	// animations 配列の先頭を今回使うアニメーションとして扱う
	std::string animationsBlock = ExtractArrayBlock(jsonText, "animations");
	std::vector<std::string> animationObjects = SplitTopLevelObjects(animationsBlock);
	assert(!animationObjects.empty());

	const std::string& animationObject = animationObjects[0];

	// samplers と channels を分けて読む
	std::vector<GltfAnimationSampler> samplers = ParseAnimationSamplers(animationObject);
	std::vector<GltfAnimationChannel> channels = ParseAnimationChannels(animationObject);

	assert(!channels.empty());

	for (const GltfAnimationChannel& channel : channels) {
		// channel が使う sampler を取得する
		assert(channel.sampler < samplers.size());
		const GltfAnimationSampler& sampler = samplers[channel.sampler];

		// 入力時刻 accessor を取得する
		const GltfAccessor& inputAccessor = accessors[sampler.input];
		const GltfBufferView& inputBufferView = bufferViews[inputAccessor.bufferView];

		// 入力時刻列へのポインタを取る
		const float* inputTimes = reinterpret_cast<const float*>(
			binary.data() + inputBufferView.byteOffset + inputAccessor.byteOffset);

		// duration は最も長いものを採用する
		animation.duration = std::max(animation.duration, inputTimes[inputAccessor.count - 1]);

		// channel の対象 node 名を取得する
		std::string nodeName = FindNodeName(jsonText, channel.targetNode);

		// 出力値 accessor を取得する
		const GltfAccessor& outputAccessor = accessors[sampler.output];
		const GltfBufferView& outputBufferView = bufferViews[outputAccessor.bufferView];

		// 出力値列へのポインタを取る
		const float* outputValues = reinterpret_cast<const float*>(
			binary.data() + outputBufferView.byteOffset + outputAccessor.byteOffset);

		// node 名に対応する animation を取得する
		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeName];

		// ここで path ごとの分岐をする

		if (channel.path == "rotation") {
			for (uint32_t index = 0; index < inputAccessor.count; ++index) {
				KeyframeQuaternion keyframe;

				// キーフレーム時刻を入れる
				keyframe.time = inputTimes[index];

				Quaternion value;
				value.x = outputValues[index * 4 + 0];
				value.y = outputValues[index * 4 + 1];
				value.z = outputValues[index * 4 + 2];
				value.w = outputValues[index * 4 + 3];

				// まずは回転をそのまま使って確認する
				keyframe.value = Normalize(value);


				nodeAnimation.rotate.keyframes.push_back(keyframe);
			}
		}
		else if (channel.path == "translation") {
			for (uint32_t index = 0; index < inputAccessor.count; ++index) {
				KeyframeVector3 keyframe;

				// キーフレーム時刻を入れる
				keyframe.time = inputTimes[index];

				Vector3 value;
				value.x = outputValues[index * 3 + 0];
				value.y = outputValues[index * 3 + 1];
				value.z = outputValues[index * 3 + 2];

				// glTF 右手系からエンジン左手系へ合わせる
				// まずは移動をそのまま使って確認する
				keyframe.value = value;


				nodeAnimation.translate.keyframes.push_back(keyframe);
			}
		}
		else if (channel.path == "scale") {
			for (uint32_t index = 0; index < inputAccessor.count; ++index) {
				KeyframeVector3 keyframe;

				// キーフレーム時刻を入れる
				keyframe.time = inputTimes[index];

				Vector3 value;
				value.x = outputValues[index * 3 + 0];
				value.y = outputValues[index * 3 + 1];
				value.z = outputValues[index * 3 + 2];

				// scale はそのまま使う
				keyframe.value = value;

				nodeAnimation.scale.keyframes.push_back(keyframe);
			}
		}

	}

	return animation;
}

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time)
{
	// キーがないと返しようがないので弾く
	assert(!keyframes.empty());

	// キーが1つだけ、または先頭より前なら先頭値を返す
	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;

		// 今のキーと次のキーの範囲に time が入っていれば補間する
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float t = (time - keyframes[index].time) /
				(keyframes[nextIndex].time - keyframes[index].time);

			return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
		}
	}

	// 最後のキーより後ろなら末尾値を返す
	return keyframes.back().value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time)
{
	// キーがないと返しようがないので弾く
	assert(!keyframes.empty());

	// キーが1つだけ、または先頭より前なら先頭値を返す
	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;

		// 今のキーと次のキーの範囲に time が入っていれば補間する
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float t = (time - keyframes[index].time) /
				(keyframes[nextIndex].time - keyframes[index].time);

			return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
		}
	}

	// 最後のキーより後ろなら末尾値を返す
	return keyframes.back().value;
}

#pragma once
#include <string>
#include <vector>
#include "Math.h"
#include "Types.h"
class ModelLoader
{
public:  
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
};


#pragma once

#include <vector>
#include "DXAPI/stdafx.h"
#include "Model.h"

class Vertex;

class ModelLoader
{
public:
	static void LoadModel(const char* path, std::vector< Vertex >& vertices, std::vector< UINT >& indices);

};


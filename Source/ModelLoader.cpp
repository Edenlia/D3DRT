#include "ModelLoader.h"
#include "./DXAPI/stdafx.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "Util/Utility.h"
#include <dxcapi.h>

using namespace DirectX;

void ModelLoader::LoadModel(const char* path, std::vector<Vertex>& vertices, std::vector<UINT>& indices)
{
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate |
													aiProcess_JoinIdenticalVertices |
													aiProcess_GenSmoothNormals |
													aiProcess_FlipUVs |
													aiProcess_MakeLeftHanded);

	ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode);

    for (int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* aMesh = scene->mMeshes[i];

        ASSERT(aMesh->HasNormals());

        XMFLOAT4 colors[] = { {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f} };

        for (int j = 0; j < aMesh->mNumVertices; j++) {
			aiVector3D vertex = aMesh->mVertices[j];
            aiVector3D normal = aMesh->mNormals[j];
            aiVector3D uv = aMesh->mTextureCoords[0][j];
			aiVector3D tangent = aMesh->mTangents[j];
			aiVector3D bitangent = aMesh->mBitangents[j];

			XMFLOAT4 p = XMFLOAT4(vertex.x, vertex.y, vertex.z, 1.0f);
			XMFLOAT4 c = colors[j % 3];
			XMFLOAT3 n = XMFLOAT3(normal.x, normal.y, normal.z);
			XMFLOAT2 u = XMFLOAT2(uv.x, uv.y);
			XMFLOAT3 t = XMFLOAT3(tangent.x, tangent.y, tangent.z);
			XMFLOAT3 b = XMFLOAT3(bitangent.x, bitangent.y, bitangent.z);

            vertices.push_back(Vertex(p, c, n, u, t, b));
		}

		for (int j = 0; j < aMesh->mNumFaces; j++) {
			aiFace face = aMesh->mFaces[j];
			for (int k = 0; k < face.mNumIndices; k++) {
				indices.push_back(face.mIndices[k]);
			}
		}
    }
	
}

void ModelLoader::CreatePlane(std::vector<Vertex>& vertices)
{
	// Define the geometry for a plane.
	vertices.push_back({ {-10.f, -1.3f, 010.f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.0f, 1.0f, 0.0f} }); // 0
	vertices.push_back({ {-10.f, -1.3f, -10.f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.0f, 1.0f, 0.0f} }); // 1
	vertices.push_back({ {010.f, -1.3f, 010.f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.0f, 1.0f, 0.0f} }); // 2
	vertices.push_back({ {010.f, -1.3f, 010.f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.0f, 1.0f, 0.0f} }); // 2
	vertices.push_back({ {-10.f, -1.3f, -10.f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.0f, 1.0f, 0.0f} }); // 1
	vertices.push_back({ {010.f, -1.3f, -10.f, 1.0f}, {1.0f, 0.7f, 0.3f, 1.0f}, {0.0f, 1.0f, 0.0f} }); // 4
}

void ModelLoader::CreateTetrahedron(std::vector<Vertex>& vertices, std::vector<UINT>& indices)
{
	// Define the geometry for a triangle.
	vertices.push_back({ {std::sqrtf(8.f / 9.f), 0.f, -1.f / 3.f}, {1.f, 0.f, 0.f, 1.f} });
	vertices.push_back({ {-std::sqrtf(2.f / 9.f), std::sqrtf(2.f / 3.f), -1.f / 3.f}, {0.f, 1.f, 0.f, 1.f} });
	vertices.push_back({ {-std::sqrtf(2.f / 9.f), -std::sqrtf(2.f / 3.f), -1.f / 3.f}, {0.f, 0.f, 1.f, 1.f} });
	vertices.push_back({ {0.f, 0.f, 1.f}, {1, 0, 1, 1} });

	indices = { 0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2 };
}

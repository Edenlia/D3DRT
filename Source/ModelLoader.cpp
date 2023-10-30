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

            vertices.push_back(Vertex(XMFLOAT4(vertex.x, vertex.y, vertex.z, 1.0f), colors[j % 3], XMFLOAT4(normal.x, normal.y, normal.z, 1.0), XMFLOAT2(uv.x, uv.y)));
		}

		for (int j = 0; j < aMesh->mNumFaces; j++) {
			aiFace face = aMesh->mFaces[j];
			for (int k = 0; k < face.mNumIndices; k++) {
				indices.push_back(face.mIndices[k]);
			}
		}
    }
	
}

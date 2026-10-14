#pragma once

#include "MikanRendererFwd.h"

#include <cstdint>
#include <string>

namespace ModelImportUtils
{
// Interleaved vertex data laid out in the material's vertex definition, plus a triangle list
// indexing it. Borrowed for the duration of a mesh-creation call: the meshes copy what they keep.
struct TriMeshSourceData
{
	std::string meshName;
	MkMaterialInstancePtr materialInstance;
	const IMkVertexAttribute* positionAttribute= nullptr;
	const uint8_t* vertexData= nullptr;
	size_t vertexSize= 0;
	uint32_t vertexCount= 0;
	const uint32_t* indexData= nullptr;
	uint32_t indexCount= 0;

	inline bool isValid() const { return vertexCount > 0 && indexCount > 0; }
};

IMkTriangulatedMeshPtr createTriangulatedMeshResource(IMkGraphicsContext* graphicsContext,
													  const TriMeshSourceData& triMeshData);
IMkWireframeMeshPtr createWireframeMeshResource(IMkGraphicsContext* graphicsContext,
												const TriMeshSourceData& triMeshData);
}; // namespace ModelImportUtils

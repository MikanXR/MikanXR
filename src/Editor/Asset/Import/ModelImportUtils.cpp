#include "ModelImportUtils.h"
#include "Colors.h"
#include "IMkTriangulatedMesh.h"
#include "IMkVertexDefinition.h"
#include "IMkWireframeMesh.h"
#include "MkMaterialInstance.h"

#include <glm/ext/vector_float4.hpp>

#include <cstring>

namespace ModelImportUtils
{
IMkTriangulatedMeshPtr createTriangulatedMeshResource(IMkGraphicsContext* graphicsContext,
													  const TriMeshSourceData& triMeshData)
{
	if (!triMeshData.isValid())
	{
		return IMkTriangulatedMeshPtr();
	}

	// Copy over the vertex data into a correctly size vertex buffer
	const size_t vertexCount= triMeshData.vertexCount;
	const size_t vertexSize= triMeshData.vertexSize;
	const size_t vertexBufferSize= vertexCount * vertexSize;
	uint8_t* vertexBuffer= new uint8_t[vertexBufferSize];
	memcpy(vertexBuffer, triMeshData.vertexData, vertexBufferSize);

	// Copy over the index data into a correctly size index buffer
	const size_t indexCount= triMeshData.indexCount;
	const size_t triangleCount= indexCount / 3;
	const size_t indexSize= sizeof(uint32_t);
	const size_t indexBufferSize= indexCount * indexSize;
	uint8_t* indexBuffer= new uint8_t[indexBufferSize];
	std::memcpy(indexBuffer, triMeshData.indexData, indexBufferSize);

	IMkTriangulatedMeshPtr triMesh=
		createMkTriangulatedMesh(graphicsContext, triMeshData.meshName, (const uint8_t*)vertexBuffer, vertexSize,
								 (uint32_t)vertexCount, (const uint8_t*)indexBuffer, indexSize, (uint32_t)triangleCount,
								 true); // <-- triangulated mesh owns vertex data, cleans up on delete

	// If the mesh fails to set the material instance or create resources
	// then clean the mesh up and return nullptr
	if (!triMesh->setMaterialInstance(triMeshData.materialInstance) || !triMesh->createResources())
	{
		triMesh= nullptr;
	}

	return triMesh;
}

IMkWireframeMeshPtr createWireframeMeshResource(IMkGraphicsContext* graphicsContext,
												const TriMeshSourceData& triMeshData)
{
	if (!triMeshData.isValid())
	{
		return IMkWireframeMeshPtr();
	}

	// Copy over the position data into the wireframe vertex buffer
	const IMkVertexAttribute* posAttribute= triMeshData.positionAttribute;
	const size_t posAttribSize= posAttribute->getAttributeSize();

	const size_t writeVertexSize= posAttribSize; // [x, y, z], [x, y, z], ...
	const size_t writeVertexCount= triMeshData.vertexCount;
	const size_t writeVertexBufferSize= writeVertexCount * writeVertexSize;
	uint8_t* writeVertexData= new uint8_t[writeVertexBufferSize];

	{
		const size_t readVertexSize= triMeshData.vertexSize;
		const uint8_t* readPtr= triMeshData.vertexData + posAttribute->getOffset();

		uint8_t* writePtr= writeVertexData + posAttribute->getOffset();

		for (size_t vertexIndex= 0; vertexIndex < writeVertexCount; ++vertexIndex)
		{
			memcpy(writePtr, readPtr, posAttribSize);
			readPtr+= readVertexSize;
			writePtr+= writeVertexSize;
		}
	}

	// Convert the triangle index list into a line index list
	const size_t lineCount= triMeshData.indexCount; // 3 indices per triangle, 3 lines per triangle
	const size_t indexCount= lineCount * 2;
	const size_t indexSize= sizeof(uint32_t);
	const size_t indexBufferSize= indexCount * indexSize;
	uint8_t* indexData= new uint8_t[indexBufferSize];

	{
		const size_t triangleCount= triMeshData.indexCount / 3;
		const uint32_t* readPtr= triMeshData.indexData;
		uint32_t* writePtr= (uint32_t*)indexData;

		for (size_t triangleIndex= 0; triangleIndex < triangleCount; triangleIndex++)
		{
			const uint32_t i0= readPtr[0];
			const uint32_t i1= readPtr[1];
			const uint32_t i2= readPtr[2];
			readPtr+= 3;

			writePtr[0]= i0;
			writePtr[1]= i1;
			writePtr[2]= i1;
			writePtr[3]= i2;
			writePtr[4]= i2;
			writePtr[5]= i0;
			writePtr+= 6;
		}
	}

	IMkWireframeMeshPtr wireMesh=
		CreateMkWireframeMesh(graphicsContext, triMeshData.meshName, (const uint8_t*)writeVertexData, writeVertexSize,
							  (uint32_t)writeVertexCount, (const uint8_t*)indexData, indexSize, (uint32_t)lineCount,
							  true); // <-- wireframe mesh owns vertex data, cleans up on delete

	if (wireMesh->createResources())
	{
		auto matInstance= wireMesh->getMaterialInstance();

		matInstance->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA, glm::vec4(Colors::Yellow, 1.f));
	}
	else
	{
		wireMesh= nullptr;
	}

	return wireMesh;
}
}; // namespace ModelImportUtils

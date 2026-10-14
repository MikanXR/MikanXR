#include "ObjModelImporter.h"
#include "Colors.h"
#include "IMkGraphicsContext.h"
#include "MikanRenderModelResource.h"
#include "MikanModelResourceManager.h"
#include "MkMaterialInstance.h"
#include "MkMaterial.h"
#include "IMkShader.h"
#include "Logger.h"
#include "MikanShaderCache.h"
#include "MikanTextureCache.h"
#include "IMkTriangulatedMesh.h"
#include "IMkWireframeMesh.h"
#include "ModelImportUtils.h"

#include "fast_obj.h"

#include <glm/ext/vector_float4.hpp>

namespace ObjUtils
{
class MaterialTriMeshData
{
public:
	MaterialTriMeshData(int materialId, const std::string& materialName, MkMaterialInstancePtr materialInst)
		: m_materialId(materialId)
		, m_materialName(materialName)
		, m_materialInstance(materialInst)
	{
		MkMaterialConstPtr material= m_materialInstance->getMaterial();
		IMkVertexDefinitionConstPtr vertexDefinition= material->getProgram()->getVertexDefinition();

		m_positionAttribute= vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::position);
		m_normalAttribute= vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::normal);
		m_texCoordAttribute= vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::texCoord);
		m_vertexSize= vertexDefinition->getVertexSize();

		assert(m_positionAttribute != nullptr && m_positionAttribute->getDataType() == eVertexDataType::datatype_vec3);
		assert(m_normalAttribute == nullptr || m_normalAttribute->getDataType() == eVertexDataType::datatype_vec3);
		assert(m_texCoordAttribute == nullptr || m_texCoordAttribute->getDataType() == eVertexDataType::datatype_vec2);
		assert(m_vertexSize > 0);
	}

	~MaterialTriMeshData()
	{
		// The mesh-creation path copies this data into its own buffers, so we retain ownership of these.
		delete[] m_vertexData;
		delete[] m_indexData;
		delete[] m_indexRemapData;
	}

	// Non-copyable: we own raw heap buffers and only ever hand these out via shared_ptr.
	MaterialTriMeshData(const MaterialTriMeshData&)= delete;
	MaterialTriMeshData& operator=(const MaterialTriMeshData&)= delete;

	inline bool isValid() const { return m_vertexCount > 0 && m_indexCount > 0; }

	inline const std::string& getMaterialName() const { return m_materialName; }
	inline MkMaterialInstancePtr getMaterialInstance() const { return m_materialInstance; }
	const IMkVertexAttribute* getPositionAttribute() const { return m_positionAttribute; }
	inline uint32_t getVertexCount() const { return m_vertexCount; }
	inline size_t getVertexSize() const { return m_vertexSize; }
	inline const uint8_t* getVertexData() const { return m_vertexData; }

	inline uint32_t getIndexCount() const { return m_indexCount; }
	inline const uint32_t* getIndexData() const { return m_indexData; }

	inline void incTriangleCount() { m_triangleCount++; }

	ModelImportUtils::TriMeshSourceData getSourceData() const
	{
		ModelImportUtils::TriMeshSourceData sourceData;
		sourceData.meshName= m_materialName;
		sourceData.materialInstance= m_materialInstance;
		sourceData.positionAttribute= m_positionAttribute;
		sourceData.vertexData= m_vertexData;
		sourceData.vertexSize= m_vertexSize;
		sourceData.vertexCount= m_vertexCount;
		sourceData.indexData= m_indexData;
		sourceData.indexCount= m_indexCount;
		return sourceData;
	}

	void allocateBuffers(size_t sourcePositionCount)
	{
		// Initially assume we need 3 vertices per triangle (max upper bound)
		// The final mesh will be compacted to the actual number of vertices
		const size_t estVertexCount= m_triangleCount * 3;
		m_vertexData= new uint8_t[estVertexCount * m_vertexSize];
		m_indexData= new uint32_t[estVertexCount];

		// Remap buffer maps indices from the obj file's (global) vertex list to the compacted vertex buffer.
		// It's indexed by the source position index (fastObjIndex::p), which spans the whole file's position
		// array (1..position_count) regardless of how many triangles this material owns, so it must be sized
		// to the source position count -- not the per-material triangle vertex count.
		m_indexRemapCount= sourcePositionCount + 1;
		m_indexRemapData= new int32_t[m_indexRemapCount];
		memset(m_indexRemapData, -1, sizeof(int32_t) * m_indexRemapCount);
	}

	void addTriangle(const fastObjMesh& objData, const fastObjIndex* elementIndices)
	{
		for (int i= 0; i < 3; i++)
		{
			const fastObjIndex elementIndex= elementIndices[i];
			fastObjUInt sourcePositionIndex= elementIndex.p;
			assert(sourcePositionIndex != 0);
			assert(sourcePositionIndex < m_indexRemapCount);

			uint32_t remappedIndex= m_indexRemapData[sourcePositionIndex];
			if (remappedIndex != -1)
			{
				// Re-use a vertex we already imported
				m_indexData[m_indexCount]= m_indexRemapData[sourcePositionIndex];
				m_indexCount++;
			}
			else
			{
				// Get the write pointer for the next vertex
				uint8_t* vertexWritePtr= &m_vertexData[m_vertexSize * m_vertexCount];

				// Copy in the position data
				glm::vec3 position(objData.positions[3 * elementIndex.p + 0], objData.positions[3 * elementIndex.p + 1],
								   objData.positions[3 * elementIndex.p + 2]);
				memcpy(vertexWritePtr + m_positionAttribute->getOffset(), &position, sizeof(glm::vec3));

				// Copy in the normal data (if vertex has a normal)
				if (m_normalAttribute != nullptr)
				{
					glm::vec3 normal(objData.normals[3 * elementIndex.n + 0], objData.normals[3 * elementIndex.n + 1],
									 objData.normals[3 * elementIndex.n + 2]);
					memcpy(vertexWritePtr + m_normalAttribute->getOffset(), &normal, sizeof(glm::vec3));
				}

				// Copy in the texel data (if vertex has a texel)
				if (m_texCoordAttribute != nullptr)
				{
					glm::vec2 texcoord(objData.texcoords[2 * elementIndex.t + 0],
									   objData.texcoords[2 * elementIndex.t + 1]);
					memcpy(vertexWritePtr + m_texCoordAttribute->getOffset(), &texcoord, sizeof(glm::vec2));
				}

				// Map the obj vertex index to the new vertex index
				m_indexRemapData[sourcePositionIndex]= m_vertexCount;

				// Add the new vertex index to the index buffer
				m_indexData[m_indexCount]= m_vertexCount;
				m_indexCount++;

				// Finally, increment the vertex count
				m_vertexCount++;
			}
		}
	}

private:
	int m_materialId= -1;
	std::string m_materialName;
	MkMaterialInstancePtr m_materialInstance;
	const IMkVertexAttribute* m_positionAttribute= nullptr;
	const IMkVertexAttribute* m_normalAttribute= nullptr;
	const IMkVertexAttribute* m_texCoordAttribute= nullptr;
	uint8_t* m_vertexData= 0;
	size_t m_vertexSize= 0;
	uint32_t m_vertexCount= 0;
	uint32_t* m_indexData= 0;
	int32_t* m_indexRemapData= 0;
	size_t m_indexRemapCount= 0;
	uint32_t m_indexCount= 0;
	uint32_t m_triangleCount= 0;
};
using MaterialTriMeshDataPtr= std::shared_ptr<MaterialTriMeshData>;
using MaterialTriMeshDataConstPtr= std::shared_ptr<const MaterialTriMeshData>;

MkMaterialInstancePtr createTriMeshMaterialInstance(IMkGraphicsContext* graphicsContext, MkMaterialConstPtr material,
													const fastObjMesh* objMesh, const fastObjMaterial& objMaterial);
}; // namespace ObjUtils

MikanRenderModelResourcePtr ObjModelImporter::importModelFromFile(const std::filesystem::path& modelPath,
																  MkMaterialConstPtr overrideMaterial)
{
	IMkGraphicsContext* graphicsContext= m_ownerManager->getGraphicsContext();
	IMkShaderCache* shaderCache= graphicsContext->getShaderCache();

	MikanRenderModelResourcePtr modelResource;

	if (modelPath.empty())
		return MikanRenderModelResourcePtr();

	MkMaterialConstPtr triMeshMaterial= overrideMaterial;
	if (!triMeshMaterial)
	{
		triMeshMaterial= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PNT_TEXTURED_LIT_COLORED);
	}

	if (!triMeshMaterial)
		return MikanRenderModelResourcePtr();

	std::vector<ObjUtils::MaterialTriMeshDataPtr> materialToTrimeshMap;

	// Load the raw obj data
	std::string modelPathString= modelPath.string();
	std::string modelNameString= modelPath.stem().string();
	fastObjMesh* objData= fast_obj_read(modelPathString.c_str());

	// Process the obj data into indexes triangles separated by material
	if (objData != nullptr)
	{
		// Create a new model resource
		modelResource= std::make_shared<MikanRenderModelResource>(m_ownerManager->getGraphicsContext());
		modelResource->setName(modelNameString);
		modelResource->setModelFilePath(modelPath);

		if (objData->material_count > 0)
		{
			// Create a material instance for each material in the obj file
			for (int materialIndex= 0; materialIndex < objData->material_count; materialIndex++)
			{
				const fastObjMaterial& objMaterial= objData->materials[materialIndex];
				const std::string materialName= objMaterial.name;

				MkMaterialInstancePtr materialInst=
					ObjUtils::createTriMeshMaterialInstance(graphicsContext, triMeshMaterial, objData, objMaterial);
				ObjUtils::MaterialTriMeshDataPtr triMeshData=
					std::make_shared<ObjUtils::MaterialTriMeshData>(materialIndex, materialName, materialInst);

				materialToTrimeshMap.push_back(triMeshData);
			}
		}
		else
		{
			// Create a default material
			const std::string materialName= "default-material";
			fastObjMaterial defaultObjMaterial;
			memset(&defaultObjMaterial, 0, sizeof(fastObjMaterial));
			defaultObjMaterial.Ka[0]= defaultObjMaterial.Ka[1]= defaultObjMaterial.Ka[2]= 1.f;
			defaultObjMaterial.Kd[0]= defaultObjMaterial.Kd[1]= defaultObjMaterial.Kd[2]= 0.8f;
			defaultObjMaterial.Ks[0]= defaultObjMaterial.Ks[1]= defaultObjMaterial.Ks[2]= 0.5f;
			defaultObjMaterial.Ns= 250.f;
			defaultObjMaterial.Ni= 1.45f;
			defaultObjMaterial.illum= 2;
			defaultObjMaterial.d= 1.0;

			MkMaterialInstancePtr materialInst=
				ObjUtils::createTriMeshMaterialInstance(graphicsContext, triMeshMaterial, objData, defaultObjMaterial);
			ObjUtils::MaterialTriMeshDataPtr triMeshData=
				std::make_shared<ObjUtils::MaterialTriMeshData>(0, materialName, materialInst);

			materialToTrimeshMap.push_back(triMeshData);
		}

		// Spin thru all the object faces counting up the number of triangles for each material
		size_t skippedFaceCount= 0;
		for (uint32_t objectIndex= 0; objectIndex < objData->object_count; objectIndex++)
		{
			const fastObjGroup& group= objData->objects[objectIndex];

			// Count up the number of triangles for each material
			for (uint32_t faceIndex= 0; faceIndex < group.face_count; faceIndex++)
			{
				unsigned int faceVertexCount= objData->face_vertices[group.face_offset + faceIndex];
				unsigned int faceMaterialId= objData->face_materials[group.face_offset + faceIndex];

				if (faceVertexCount == 3 && faceMaterialId < materialToTrimeshMap.size())
				{
					auto& triMeshData= materialToTrimeshMap[faceMaterialId];

					triMeshData->incTriangleCount();
				}
				else if (faceVertexCount != 3)
				{
					// Only triangles are supported. Non-triangle faces (e.g. quads/n-gons) are omitted rather
					// than triangulated, since we can't assume the faces are convex (no ear-clipping here).
					skippedFaceCount++;
				}
			}
		}

		if (skippedFaceCount > 0)
		{
			MIKAN_LOG_WARNING("ObjModelImporter::importModelFromFile")
				<< "Omitted " << skippedFaceCount << " non-triangle face(s) while importing " << modelPathString
				<< ". Re-export with triangulated faces to include them.";
		}

		// Allocate the vertex and index buffers for each material instance.
		// The remap table is indexed by the file's global position index, so size it to the position count.
		for (auto& triMeshData : materialToTrimeshMap)
		{
			triMeshData->allocateBuffers(objData->position_count);
		};

		// Spin back all the object faces and actually add triangles to each material instance
		for (uint32_t objectIndex= 0; objectIndex < objData->object_count; objectIndex++)
		{
			const fastObjGroup& group= objData->objects[objectIndex];
			uint32_t groupElementIndex= 0;

			// Count up the number of triangles for each material
			for (uint32_t faceIndex= 0; faceIndex < group.face_count; faceIndex++)
			{
				unsigned int faceVertexCount= objData->face_vertices[group.face_offset + faceIndex];
				unsigned int faceMaterialId= objData->face_materials[group.face_offset + faceIndex];

				if (faceVertexCount == 3 && faceMaterialId < materialToTrimeshMap.size())
				{
					auto& triMeshData= materialToTrimeshMap[faceMaterialId];
					const fastObjIndex* elementIndices= &objData->indices[group.index_offset + groupElementIndex];

					triMeshData->addTriangle(*objData, elementIndices);
				}

				groupElementIndex+= faceVertexCount;
			}
		}

		// Create the meshes for each material instance
		for (ObjUtils::MaterialTriMeshDataPtr triMeshData : materialToTrimeshMap)
		{
			const ModelImportUtils::TriMeshSourceData sourceData= triMeshData->getSourceData();
			IMkTriangulatedMeshPtr trimesh=
				ModelImportUtils::createTriangulatedMeshResource(graphicsContext, sourceData);
			IMkWireframeMeshPtr wiremesh= ModelImportUtils::createWireframeMeshResource(graphicsContext, sourceData);

			modelResource->addTriangulatedMesh(trimesh);
			modelResource->addWireframeMesh(wiremesh);
		}
	}

	if (objData != nullptr)
	{
		fast_obj_destroy(objData);
	}

	return modelResource;
}

namespace ObjUtils
{
bool addTextureToMaterialInstance(IMkTextureCache* textureCache, MkMaterialInstancePtr materialInstance,
								  const fastObjMesh* objMesh, unsigned int textureIndex,
								  const eUniformSemantic semantic)
{
	IMkShaderPtr program= materialInstance->getMaterial()->getProgram();

	// See if the shader has a uniform that wants to bind to a texture with the given semantic
	std::string uniformName;
	if (program->getFirstUniformNameOfSemantic(semantic, uniformName)
		&& getUniformSemanticDataType(semantic) == eUniformDataType::datatype_texture)
	{
		// Try loading the texture using the relative path
		IMkTexturePtr texture;
		// Texture index 0 is a dummy/invalid texture, valid indices start at 1
		if (textureIndex > 0 && textureIndex < objMesh->texture_count)
		{
			const fastObjTexture& objTexture= objMesh->textures[textureIndex];
			if (objTexture.path != nullptr && objTexture.path[0] != '\0')
			{
				texture= textureCache->loadTexturePath(objTexture.path);
			}
		}

		// If that fails, fallback to the default white texture
		if (texture == nullptr)
		{
			texture= textureCache->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGB);
		}

		if (texture)
		{
			// Bind the texture to the material instance
			return materialInstance->setTextureByUniformName(uniformName, texture);
		}
		else
		{
			// Failed to load any texture to bind to the uniform
			return false;
		}
	}
	else
	{
		// The shader doesn't have uniform that cares about this texture
		return true;
	}
}

MkMaterialInstancePtr createTriMeshMaterialInstance(IMkGraphicsContext* graphicsContext, MkMaterialConstPtr material,
													const fastObjMesh* objMesh, const fastObjMaterial& objMaterial)
{
	IMkTextureCache* textureCache= graphicsContext->getTextureCache();
	MkMaterialInstancePtr materialInstance= createMkMaterialInstance(material);

	materialInstance->setVec3BySemantic(eUniformSemantic::ambientColorRGB,
										glm::vec4(objMaterial.Ka[0], objMaterial.Ka[1], objMaterial.Ka[2], 1.f));
	materialInstance->setVec3BySemantic(eUniformSemantic::diffuseColorRGBA,
										glm::vec4(objMaterial.Kd[0], objMaterial.Kd[1], objMaterial.Kd[2], 1.f));
	materialInstance->setVec3BySemantic(eUniformSemantic::specularColorRGB,
										glm::vec4(objMaterial.Ks[0], objMaterial.Ks[1], objMaterial.Ks[2], 1.f));
	materialInstance->setFloatBySemantic(eUniformSemantic::specularHighlights, objMaterial.Ns);
	materialInstance->setFloatBySemantic(eUniformSemantic::opticalDensity, objMaterial.Ni);
	materialInstance->setFloatBySemantic(eUniformSemantic::dissolve, objMaterial.d);
	materialInstance->setFloatBySemantic(eUniformSemantic::ambientStrength, 0.3f);

	// Ambient Texture Map
	addTextureToMaterialInstance(textureCache, materialInstance, objMesh, objMaterial.map_Ka,
								 eUniformSemantic::ambientTexture);
	// Diffuse Texture Map
	addTextureToMaterialInstance(textureCache, materialInstance, objMesh, objMaterial.map_Kd,
								 eUniformSemantic::diffuseTexture);
	// Specular Texture Map
	addTextureToMaterialInstance(textureCache, materialInstance, objMesh, objMaterial.map_Ks,
								 eUniformSemantic::specularTexture);
	// Specular Hightlight Map
	addTextureToMaterialInstance(textureCache, materialInstance, objMesh, objMaterial.map_Ns,
								 eUniformSemantic::specularHightlightTexture);
	// Alpha Texture Map
	addTextureToMaterialInstance(textureCache, materialInstance, objMesh, objMaterial.map_d,
								 eUniformSemantic::alphaTexture);
	// Bump Map
	addTextureToMaterialInstance(textureCache, materialInstance, objMesh, objMaterial.map_bump,
								 eUniformSemantic::bumpTexture);

	return materialInstance;
}
}; // namespace ObjUtils
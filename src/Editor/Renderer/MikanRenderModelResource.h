#pragma once

#include "MikanRendererFwd.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class MikanRenderModelResource
{
public:
	MikanRenderModelResource(class IMkGraphicsContext* ownerGraphicsContext);
	virtual ~MikanRenderModelResource();

	inline const std::string& getName() const { return m_name; }
	inline void setName(const std::string& inName) { m_name= inName; }

	inline const std::filesystem::path& getModelFilePath() const { return m_renderModelFilepath; }
	inline void setModelFilePath(const std::filesystem::path& inModelFilePath)
	{
		m_renderModelFilepath= inModelFilePath;
	}

	void addTriangulatedMesh(IMkTriangulatedMeshPtr mesh);
	void addWireframeMesh(IMkWireframeMeshPtr mesh);

	int getTriangulatedMeshCount() const { return (int)m_triangulatedMeshes.size(); }
	IMkTriangulatedMeshPtr getTriangulatedMesh(int meshIndex) const { return m_triangulatedMeshes[meshIndex]; }

	size_t getWireframeMeshCount() const { return m_wireframeMeshes.size(); }
	IMkWireframeMeshPtr getWireframeMesh(int meshIndex) const { return m_wireframeMeshes[meshIndex]; }

	// A file the client geometry is built from, as recorded when the model loaded
	struct GeometrySourceFile
	{
		std::filesystem::path path;
		uint64_t fileSize= 0;
		int64_t lastWriteTime= 0;
	};

	// FNV-1a 64 over each file's path, size and last-write time, as 16 hex digits
	static std::string computeGeometryETag(const std::vector<GeometrySourceFile>& sourceFiles);

	// Records the model file and each mesh's diffuse texture file and derives the geometry etag from
	// them. Called once the importer has bound the mesh materials. The resource manager never
	// reloads a loaded model, so the etag stays valid for the life of the resource.
	void finalizeGeometryETag();
	inline const std::string& getGeometryETag() const { return m_geometryETag; }

	// The client geometry as GLB bytes: every triangulated mesh, each with its diffuse texture
	// embedded as an unlit base colour image. Built on the first call and shared after, so a
	// connection thread can send it while the editor goes on. Main thread only.
	std::shared_ptr<const std::vector<uint8_t>> getOrBuildGlbPayload();

protected:
	void disposeMeshRenderResources();

	class IMkGraphicsContext* m_ownerGraphicsContext= nullptr;

	std::string m_name;
	std::filesystem::path m_renderModelFilepath;

	std::vector<IMkTriangulatedMeshPtr> m_triangulatedMeshes;
	std::vector<IMkWireframeMeshPtr> m_wireframeMeshes;

	// The distinct diffuse texture files, and per triangulated mesh an index into them or -1
	std::vector<std::filesystem::path> m_clientTexturePaths;
	std::vector<int> m_clientTextureIndexPerMesh;
	std::string m_geometryETag;
	std::shared_ptr<const std::vector<uint8_t>> m_glbPayload;
};

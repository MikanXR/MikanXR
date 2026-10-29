#pragma once

#include "Constants_OcclusionMeshBuild.h"
#include "MikanRendererFwd.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

#include <memory>
#include <string>

struct OcclusionMeshBuildResult;
class MkScene;

/// The built occlusion mesh drawn solid in the build stage's viewport, in
/// one of the preview views. Every view is the same geometry through the
/// same lit textured material: the textured view samples the atlas, and an
/// overlay writes its measure into the texcoord and samples a colour ramp.
/// Switching views rebuilds the vertex buffer, not the geometry.
class OcclusionMeshPreview
{
public:
	OcclusionMeshPreview();
	~OcclusionMeshPreview();

	/// Upload the result's atlas and draw its mesh in the given view. Needs
	/// the GL context. result must stay unchanged until release().
	bool build(IMkGraphicsContext* graphicsContext, const OcclusionMeshBuildResult& result,
			   eOcclusionMeshPreviewView view);
	void release();

	bool hasMesh() const { return m_result != nullptr; }
	bool isViewAvailable(eOcclusionMeshPreviewView view) const;
	eOcclusionMeshPreviewView getView() const { return m_view; }
	bool setView(eOcclusionMeshPreviewView view);

	/// Where the mesh's own space (the take's ARKit world) lands in the world
	void setModelMatrix(const glm::mat4& meshToWorld);

	/// The mesh's axis-aligned bounds in its own space
	const glm::vec3& getBoundsLo() const { return m_boundsLo; }
	const glm::vec3& getBoundsHi() const { return m_boundsHi; }

	void render(IMkCameraConstPtr camera, IMkGraphicsContext* graphicsContext) const;

	/// The measure's values at the two ends of an overlay's colour ramp, for
	/// the legend. False for a view with no ramp.
	static bool getRampEnds(eOcclusionMeshPreviewView view, std::string& outLow, std::string& outHigh);

	/// The best default view for a result: coverage when it was measured,
	/// textured when it was textured, shaded otherwise
	static eOcclusionMeshPreviewView getDefaultView(const OcclusionMeshBuildResult& result);

private:
	bool rebuildMesh();

	IMkGraphicsContext* m_graphicsContext= nullptr;
	const OcclusionMeshBuildResult* m_result= nullptr;
	eOcclusionMeshPreviewView m_view= eOcclusionMeshPreviewView::shaded;
	glm::mat4 m_meshToWorld= glm::mat4(1.f);
	glm::vec3 m_boundsLo= glm::vec3(0.f);
	glm::vec3 m_boundsHi= glm::vec3(0.f);

	std::shared_ptr<MkScene> m_scene;
	IMkTriangulatedMeshPtr m_mesh;
	IMkStaticMeshInstancePtr m_meshInstance;
	IMkTexturePtr m_atlasTexture;
	IMkTexturePtr m_rampTexture;
	IMkTexturePtr m_classTexture;
};

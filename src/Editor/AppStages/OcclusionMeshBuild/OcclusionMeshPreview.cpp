#include "OcclusionMeshBuild/OcclusionMeshPreview.h"

#include "IMkGraphicsContext.h"
#include "IMkShader.h"
#include "IMkShaderCache.h"
#include "IMkStaticMeshInstance.h"
#include "IMkTexture.h"
#include "IMkTextureCache.h"
#include "IMkTriangulatedMesh.h"
#include "IMkVertexDefinition.h"
#include "Logger.h"
#include "MikanTextureCache.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScene.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "ModelImportUtils.h"
#include "OcclusionMeshBuilder.h"

#include "glm/common.hpp"
#include "glm/geometric.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace OcclusionMeshPreviewInternal
{
// Texel 0 of the ramp is a flat grey for a vertex the measure has no value
// for; the ramp itself runs from red (poor) through yellow to green (good)
// over the remaining texels, the same three colours as the coverage classes
const int k_rampWidth= 256;
const glm::vec3 k_noDataColor(0.35f, 0.35f, 0.35f);
const glm::vec3 k_poorColor(0.85f, 0.15f, 0.12f);
const glm::vec3 k_fairColor(0.95f, 0.8f, 0.15f);
const glm::vec3 k_goodColor(0.2f, 0.75f, 0.25f);
const glm::vec3 k_shadedColor(0.7f, 0.7f, 0.7f);

// The ends of the sequential overlays: a count reaching this many supporting
// frames reads full green, and a supporting camera this far away full red
const float k_observationsFull= 16.f;
const float k_directionsFull= 9.f;
const float k_distanceFull= 3.f;

// u for a measure normalised to [0, 1], 1 good, landing on the ramp past
// the no-data texel
float rampU(float goodness)
{
	const float first= 1.5f / (float)k_rampWidth;
	const float last= 1.f - 0.5f / (float)k_rampWidth;
	return first + std::clamp(goodness, 0.f, 1.f) * (last - first);
}

float noDataU() { return 0.5f / (float)k_rampWidth; }

glm::vec3 rampColor(float t)
{
	return t < 0.5f ? glm::mix(k_poorColor, k_fairColor, t * 2.f) : glm::mix(k_fairColor, k_goodColor, t * 2.f - 1.f);
}

// An RGBA texture from rows of pixels, uploaded and released, top row first
IMkTexturePtr createRgbaTexture(const std::string& name, int width, int height, const uint8_t* rgba)
{
	IMkTexturePtr texture= CreateMkTexture((uint16_t)width, (uint16_t)height, rgba, MK_RGBA, MK_RGBA);
	texture->setName(name);
	texture->setTextureWrapMode(IMkTexture::TextureWrapMode::ClampToEdge);
	if (!texture->createTexture())
	{
		MIKAN_LOG_WARNING("OcclusionMeshPreview") << "Failed to create the texture " << name;
		return IMkTexturePtr();
	}
	texture->setTextureMapData(nullptr);
	return texture;
}

IMkTexturePtr createColorTable(const std::string& name, const std::vector<glm::vec3>& colors)
{
	std::vector<uint8_t> rgba(colors.size() * 4);
	for (size_t index= 0; index < colors.size(); ++index)
	{
		for (int channel= 0; channel < 3; ++channel)
			rgba[index * 4 + channel]= (uint8_t)std::lround(std::clamp(colors[index][channel], 0.f, 1.f) * 255.f);
		rgba[index * 4 + 3]= 255;
	}
	return createRgbaTexture(name, (int)colors.size(), 1, rgba.data());
}

// The atlas comes from OpenCV as BGR with its top row first, and the mesh's
// texcoords are V-up. The lit shader samples the texcoord as given, so the
// rows go up bottom first and the image's bottom row lands at v = 0.
IMkTexturePtr createAtlasTexture(const cv::Mat& atlas)
{
	if (atlas.empty() || atlas.type() != CV_8UC3 || atlas.cols > UINT16_MAX || atlas.rows > UINT16_MAX)
		return IMkTexturePtr();

	cv::Mat rgba, flipped;
	cv::cvtColor(atlas, rgba, cv::COLOR_BGR2RGBA);
	cv::flip(rgba, flipped, 0);
	return createRgbaTexture("OcclusionMeshPreviewAtlas", flipped.cols, flipped.rows, flipped.data);
}
} // namespace OcclusionMeshPreviewInternal

OcclusionMeshPreview::OcclusionMeshPreview()
	: m_scene(std::make_shared<MkScene>())
{
	// A key light from above and slightly behind the default orbit
	m_scene->setLightDirection(glm::normalize(glm::vec3(0.3f, -1.f, -0.5f)));
}

OcclusionMeshPreview::~OcclusionMeshPreview() { release(); }

bool OcclusionMeshPreview::build(IMkGraphicsContext* graphicsContext, const OcclusionMeshBuildResult& result,
								 eOcclusionMeshPreviewView view)
{
	using namespace OcclusionMeshPreviewInternal;

	release();
	if (graphicsContext == nullptr || result.mesh.vertices.empty() || result.mesh.indices.empty())
		return false;

	m_graphicsContext= graphicsContext;
	m_result= &result;

	m_boundsLo= glm::vec3(std::numeric_limits<float>::max());
	m_boundsHi= glm::vec3(-std::numeric_limits<float>::max());
	for (const glm::vec3& vertex : result.mesh.vertices)
	{
		m_boundsLo= glm::min(m_boundsLo, vertex);
		m_boundsHi= glm::max(m_boundsHi, vertex);
	}

	if (result.mesh.hasTexCoords())
		m_atlasTexture= createAtlasTexture(result.atlas);

	std::vector<glm::vec3> ramp(k_rampWidth);
	ramp[0]= k_noDataColor;
	for (int texel= 1; texel < k_rampWidth; ++texel)
		ramp[texel]= rampColor((float)(texel - 1) / (float)(k_rampWidth - 2));
	m_rampTexture= createColorTable("OcclusionMeshPreviewRamp", ramp);
	m_classTexture= createColorTable("OcclusionMeshPreviewClasses", {k_poorColor, k_fairColor, k_goodColor});

	m_view= isViewAvailable(view) ? view : eOcclusionMeshPreviewView::shaded;
	if (!rebuildMesh())
	{
		release();
		return false;
	}
	return true;
}

void OcclusionMeshPreview::release()
{
	m_scene->removeAllInstances();
	m_meshInstance= nullptr;
	m_mesh= nullptr;
	m_atlasTexture= nullptr;
	m_rampTexture= nullptr;
	m_classTexture= nullptr;
	m_result= nullptr;
	m_graphicsContext= nullptr;
}

bool OcclusionMeshPreview::isViewAvailable(eOcclusionMeshPreviewView view) const
{
	if (m_result == nullptr)
		return false;

	switch (view)
	{
	case eOcclusionMeshPreviewView::textured:
		return m_atlasTexture != nullptr && m_result->mesh.hasTexCoords();
	case eOcclusionMeshPreviewView::shaded:
		return true;
	case eOcclusionMeshPreviewView::coverage:
	case eOcclusionMeshPreviewView::observations:
	case eOcclusionMeshPreviewView::directions:
	case eOcclusionMeshPreviewView::angle:
	case eOcclusionMeshPreviewView::contradiction:
	case eOcclusionMeshPreviewView::distance:
		return m_result->coverage.getVertexCount() == m_result->mesh.vertices.size();
	default:
		return false;
	}
}

bool OcclusionMeshPreview::setView(eOcclusionMeshPreviewView view)
{
	if (!isViewAvailable(view))
		return false;
	if (view == m_view && m_mesh)
		return true;

	m_view= view;
	return rebuildMesh();
}

void OcclusionMeshPreview::setModelMatrix(const glm::mat4& meshToWorld)
{
	m_meshToWorld= meshToWorld;
	if (m_meshInstance)
		m_meshInstance->setModelMatrix(meshToWorld);
}

bool OcclusionMeshPreview::rebuildMesh()
{
	using namespace OcclusionMeshPreviewInternal;

	m_scene->removeAllInstances();
	m_meshInstance= nullptr;
	m_mesh= nullptr;

	IMkShaderCache* shaderCache= m_graphicsContext->getShaderCache();
	MkMaterialConstPtr material= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PNT_TEXTURED_LIT_COLORED);
	if (!material)
		return false;

	IMkVertexDefinitionConstPtr vertexDefinition= material->getProgram()->getVertexDefinition();
	const IMkVertexAttribute* positionAttribute=
		vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::position);
	const IMkVertexAttribute* normalAttribute= vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::normal);
	const IMkVertexAttribute* texCoordAttribute=
		vertexDefinition->getFirstAttributeBySemantic(eVertexSemantic::texCoord);
	const size_t vertexSize= vertexDefinition->getVertexSize();
	if (positionAttribute == nullptr)
		return false;

	const TriangleMesh& mesh= m_result->mesh;
	const MeshCoverageResult& coverage= m_result->coverage;
	const size_t vertexCount= mesh.vertices.size();
	const bool bHasNormals= mesh.normals.size() == vertexCount;

	// The texcoord a vertex samples in the current view
	auto texCoordOf= [&](size_t vertex) -> glm::vec2
	{
		switch (m_view)
		{
		case eOcclusionMeshPreviewView::textured:
			return mesh.texCoords[vertex];
		case eOcclusionMeshPreviewView::coverage:
			return glm::vec2(((float)coverage.coverageClass[vertex] + 0.5f) / 3.f, 0.5f);
		case eOcclusionMeshPreviewView::observations:
			return glm::vec2(rampU((float)coverage.support[vertex] / k_observationsFull), 0.5f);
		case eOcclusionMeshPreviewView::directions:
			return glm::vec2(rampU((float)coverage.directions[vertex] / k_directionsFull), 0.5f);
		case eOcclusionMeshPreviewView::angle:
			return glm::vec2(coverage.support[vertex] > 0 ? rampU(coverage.bestCos[vertex]) : noDataU(), 0.5f);
		case eOcclusionMeshPreviewView::contradiction:
		{
			const int observed= (int)coverage.support[vertex] + (int)coverage.carve[vertex];
			const float share= observed > 0 ? (float)coverage.carve[vertex] / (float)observed : 0.f;
			return glm::vec2(observed > 0 ? rampU(1.f - share) : noDataU(), 0.5f);
		}
		case eOcclusionMeshPreviewView::distance:
			return glm::vec2(coverage.support[vertex] > 0 ? rampU(1.f - coverage.nearest[vertex] / k_distanceFull)
														  : noDataU(),
							 0.5f);
		default:
			return glm::vec2(0.5f);
		}
	};

	std::vector<uint8_t> vertexData(vertexCount * vertexSize, 0);
	for (size_t vertex= 0; vertex < vertexCount; ++vertex)
	{
		uint8_t* vertexWritePtr= &vertexData[vertex * vertexSize];
		memcpy(vertexWritePtr + positionAttribute->getOffset(), &mesh.vertices[vertex], sizeof(glm::vec3));
		if (normalAttribute != nullptr)
		{
			const glm::vec3 normal= bHasNormals ? mesh.normals[vertex] : glm::vec3(0.f, 1.f, 0.f);
			memcpy(vertexWritePtr + normalAttribute->getOffset(), &normal, sizeof(glm::vec3));
		}
		if (texCoordAttribute != nullptr)
		{
			const glm::vec2 texCoord= texCoordOf(vertex);
			memcpy(vertexWritePtr + texCoordAttribute->getOffset(), &texCoord, sizeof(glm::vec2));
		}
	}

	IMkTexturePtr diffuseTexture;
	glm::vec4 modelColor(1.f);
	switch (m_view)
	{
	case eOcclusionMeshPreviewView::textured:
		diffuseTexture= m_atlasTexture;
		break;
	case eOcclusionMeshPreviewView::shaded:
		diffuseTexture= m_graphicsContext->getTextureCache()->tryGetTextureByName(INTERNAL_TEXTURE_WHITE_RGB);
		modelColor= glm::vec4(k_shadedColor, 1.f);
		break;
	case eOcclusionMeshPreviewView::coverage:
		diffuseTexture= m_classTexture;
		break;
	default:
		diffuseTexture= m_rampTexture;
		break;
	}

	MkMaterialInstancePtr materialInstance= createMkMaterialInstance(material);
	materialInstance->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA, modelColor);
	materialInstance->setVec3BySemantic(eUniformSemantic::specularColorRGB, glm::vec3(0.1f));
	materialInstance->setFloatBySemantic(eUniformSemantic::specularHighlights, 32.f);
	materialInstance->setFloatBySemantic(eUniformSemantic::ambientStrength, 0.45f);
	if (diffuseTexture)
		materialInstance->setTextureBySemantic(eUniformSemantic::diffuseTexture, diffuseTexture);

	ModelImportUtils::TriMeshSourceData sourceData;
	sourceData.meshName= "OcclusionMeshPreview";
	sourceData.materialInstance= materialInstance;
	sourceData.positionAttribute= positionAttribute;
	sourceData.vertexData= vertexData.data();
	sourceData.vertexSize= vertexSize;
	sourceData.vertexCount= (uint32_t)vertexCount;
	sourceData.indexData= mesh.indices.data();
	sourceData.indexCount= (uint32_t)mesh.indices.size();
	m_mesh= ModelImportUtils::createTriangulatedMeshResource(m_graphicsContext, sourceData);
	if (!m_mesh)
		return false;

	m_meshInstance= createMkStaticMeshInstance("OcclusionMeshPreview", m_mesh);
	m_meshInstance->setModelMatrix(m_meshToWorld);
	m_meshInstance->setVisible(true);
	m_scene->addInstance(m_meshInstance);
	return true;
}

void OcclusionMeshPreview::render(IMkCameraConstPtr camera, IMkGraphicsContext* graphicsContext) const
{
	if (!m_meshInstance || !camera || graphicsContext == nullptr)
		return;

	mkStateClearBuffer(graphicsContext->getMkStateStack().getCurrentState(), eMkClearFlags::depth);
	m_scene->render(camera, graphicsContext->getMkStateStack());
}

bool OcclusionMeshPreview::getRampEnds(eOcclusionMeshPreviewView view, std::string& outLow, std::string& outHigh)
{
	using namespace OcclusionMeshPreviewInternal;

	char text[32];
	switch (view)
	{
	case eOcclusionMeshPreviewView::observations:
		outLow= "0";
		snprintf(text, sizeof(text), "%.0f+", k_observationsFull);
		outHigh= text;
		return true;
	case eOcclusionMeshPreviewView::directions:
		outLow= "0";
		snprintf(text, sizeof(text), "%.0f", k_directionsFull);
		outHigh= text;
		return true;
	case eOcclusionMeshPreviewView::angle:
		outLow= "0.0";
		outHigh= "1.0";
		return true;
	case eOcclusionMeshPreviewView::contradiction:
		outLow= "100%";
		outHigh= "0%";
		return true;
	case eOcclusionMeshPreviewView::distance:
		snprintf(text, sizeof(text), "%.0f m+", k_distanceFull);
		outLow= text;
		outHigh= "0 m";
		return true;
	default:
		return false;
	}
}

eOcclusionMeshPreviewView OcclusionMeshPreview::getDefaultView(const OcclusionMeshBuildResult& result)
{
	if (result.coverage.getVertexCount() == result.mesh.vertices.size() && !result.mesh.vertices.empty())
		return eOcclusionMeshPreviewView::coverage;
	if (result.mesh.hasTexCoords() && !result.atlas.empty())
		return eOcclusionMeshPreviewView::textured;
	return eOcclusionMeshPreviewView::shaded;
}

#include "MarkerComponent.h"
#include "App.h"
#include "CalibrationPatternFinder.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "Logger.h"
#include "MarkerObjectSystem.h"
#include "MikanAPITypes.h"
#include "MikanMarkerTypes.h"
#include "MikanMathTypes.h"
#include "MikanObject.h"
#include "MkMaterialInstance.h"
#include "OSUtils.h"
#include "ProjectConfig.h"
#include "SelectionComponent.h"
#include "StringUtils.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <hpdf.h>

#include <filesystem>

// -- MarkerDefinition -----
const std::string MarkerDefinition::k_arucoIdPropertyId= "aruco_id";
const std::string MarkerDefinition::k_lengthMMPropertyId= "length_mm";

MarkerDefinition::MarkerDefinition()
	: MikanComponentDefinition()
	, m_arucoId(DEFAULT_ORIGIN_MARKER_ID)
	, m_lengthMM(DEFAULT_MARKER_SIZE_MM)
{
}

MarkerDefinition::MarkerDefinition(MikanMarkerID markerId)
	: MikanComponentDefinition(markerId, "")
	, m_arucoId(DEFAULT_ORIGIN_MARKER_ID)
	, m_lengthMM(DEFAULT_MARKER_SIZE_MM)
{
}

configuru::Config MarkerDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[MarkerDefinition::k_arucoIdPropertyId]= m_arucoId;
	pt[MarkerDefinition::k_lengthMMPropertyId]= m_lengthMM;

	return pt;
}

void MarkerDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_arucoId= pt.get_or<int>(MarkerDefinition::k_arucoIdPropertyId, m_arucoId);
	m_lengthMM= pt.get_or<float>(MarkerDefinition::k_lengthMMPropertyId, m_lengthMM);
}

bool MarkerDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
										  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanMarkerComponentValues>();
	if (componentValues)
	{
		m_arucoId= componentValues->aruco_id;
		m_lengthMM= componentValues->length_mm;
	}

	return true;
}

void MarkerDefinition::setArucoId(int arucoId)
{
	if (arucoId != m_arucoId)
	{
		m_arucoId= arucoId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_arucoIdPropertyId));
	}
}

void MarkerDefinition::setLengthMM(float lengthMM)
{
	if (lengthMM != m_lengthMM)
	{
		m_lengthMM= lengthMM;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_lengthMMPropertyId));
	}
}

// -- MarkerComponent -----
const std::string MarkerComponent::k_printMarkerFunctionId= "print_marker";

MarkerComponent::MarkerComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle MarkerComponent::getClientAPIValuesStructType() const
{
	return &MikanMarkerComponentValues::staticGetArchetype();
}

MarkerObjectSystemPtr MarkerComponent::getOwnerMarkerSystem() const
{
	return std::static_pointer_cast<MarkerObjectSystem>(getOwnerObject()->getOwnerSystem());
}

// -- IPropertyInterface ----
void MarkerComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(MarkerDefinition::k_arucoIdPropertyId, MikanVariantType::INT)
			->setDefaultValue(DEFAULT_ORIGIN_MARKER_ID));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(MarkerDefinition::k_lengthMMPropertyId, MikanVariantType::FLOAT)
			->setDefaultValue(DEFAULT_MARKER_SIZE_MM));
}

bool MarkerComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == MarkerDefinition::k_arucoIdPropertyId)
	{
		outValue= getMarkerDefinition()->getArucoId();
		return true;
	}
	else if (propertyName == MarkerDefinition::k_lengthMMPropertyId)
	{
		outValue= getMarkerDefinition()->getLengthMM();
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool MarkerComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == MarkerDefinition::k_arucoIdPropertyId)
	{
		getMarkerDefinition()->setArucoId(inValue.getIntValue());
		return true;
	}
	else if (propertyName == MarkerDefinition::k_lengthMMPropertyId)
	{
		getMarkerDefinition()->setLengthMM(inValue.getFloatValue());
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
void MarkerComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_printMarkerFunctionId, "Print Marker"));
}

bool MarkerComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_printMarkerFunctionId)
	{
		printMarker();
		return true;
	}

	return MikanComponent::invokeFunction(functionName);
}

void MarkerComponent::printMarker()
{
	MarkerObjectSystemPtr markerSystem= getOwnerMarkerSystem();
	ArucoDictionaryPtr dictionary=
		CalibrationPatternFinder::getArucoDictionary(markerSystem->getTypedDefinitionConst()->getArucoDictionaryType());

	MarkerDefinitionPtr markerDefinition= getMarkerDefinition();
	const int arucoId= markerDefinition->getArucoId();
	const float lengthMM= markerDefinition->getLengthMM();

	// Generate ArUco marker image using OpenCV
	cv::Mat markerImage;
	const int markerSizePixels= 600; // High resolution for printing
	cv::aruco::generateImageMarker(*dictionary.get(), arucoId, markerSizePixels, markerImage, 1);

	// Create PDF using libharu
	HPDF_Doc pdf= HPDF_New(NULL, NULL);
	if (!pdf)
	{
		MIKAN_LOG_ERROR("MarkerComponent::printMarker") << "Failed to create PDF document";
		return;
	}

	// Add a page (A4 size)
	HPDF_Page page= HPDF_AddPage(pdf);
	HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);

	// Get page dimensions
	const float pageWidth= HPDF_Page_GetWidth(page);
	const float pageHeight= HPDF_Page_GetHeight(page);

	// Calculate marker size in points (1mm = 2.83465 points)
	const float mmToPoints= 2.83465f;
	const float markerSizePoints= lengthMM * mmToPoints;

	// Center the marker on the page
	const float xPos= (pageWidth - markerSizePoints) / 2.0f;
	const float yPos= (pageHeight - markerSizePoints) / 2.0f;

	// Convert OpenCV image to PNG format in memory
	std::vector<uchar> pngBuffer;
	cv::imencode(".png", markerImage, pngBuffer);

	// Load image from memory into libharu
	HPDF_Image image= HPDF_LoadPngImageFromMem(pdf, pngBuffer.data(), (HPDF_UINT)pngBuffer.size());
	if (!image)
	{
		MIKAN_LOG_ERROR("MarkerComponent::printMarker") << "Failed to load marker image into PDF";
		HPDF_Free(pdf);
		return;
	}

	// Draw the marker image on the page
	HPDF_Page_DrawImage(page, image, xPos, yPos, markerSizePoints, markerSizePoints);

	// Add text label below the marker
	HPDF_Font font= HPDF_GetFont(pdf, "Helvetica", NULL);
	HPDF_Page_SetFontAndSize(page, font, 12);

	std::string labelText=
		"ArUco Marker ID: " + std::to_string(arucoId) + " | Size: " + std::to_string((int)lengthMM) + "mm";
	const float textWidth= HPDF_Page_TextWidth(page, labelText.c_str());
	const float textXPos= (pageWidth - textWidth) / 2.0f;
	const float textYPos= yPos - 30.0f;

	HPDF_Page_BeginText(page);
	HPDF_Page_TextOut(page, textXPos, textYPos, labelText.c_str());
	HPDF_Page_EndText(page);

	// Save PDF to temp file
	std::filesystem::path pdfFilePath=
		std::filesystem::temp_directory_path() / ("MikanMarker_" + std::to_string(arucoId) + ".pdf");

	HPDF_STATUS status= HPDF_SaveToFile(pdf, pdfFilePath.string().c_str());
	HPDF_Free(pdf);

	if (status != HPDF_OK)
	{
		MIKAN_LOG_ERROR("MarkerComponent::printMarker") << "Failed to save PDF to: " << pdfFilePath.string();
		return;
	}

	MIKAN_LOG_INFO("MarkerComponent::printMarker") << "PDF saved to: " << pdfFilePath.string();

	// Open PDF in default viewer (which will have print functionality)
	if (!OSUtils::openFileWithDefaultApplication(pdfFilePath))
	{
		MIKAN_LOG_ERROR("MarkerComponent::printMarker") << "Failed to open PDF viewer";
	}
}

// -- Rendering ----
void MarkerComponent::ensureMarkerResources(IMkGraphicsContext* graphicsContext)
{
	MarkerDefinitionPtr markerDef= getMarkerDefinition();
	const int currentArucoId= markerDef->getArucoId();
	const float currentLengthMM= markerDef->getLengthMM();

	if (currentArucoId == m_cachedArucoId && currentLengthMM == m_cachedLengthMM && m_markerMesh != nullptr)
	{
		return;
	}

	m_cachedArucoId= currentArucoId;
	m_cachedLengthMM= currentLengthMM;
	m_markerTexture.reset();
	m_markerMesh.reset();

	if (currentArucoId < 0)
		return;

	// Generate ArUco texture
	MarkerObjectSystemPtr markerSystem= getOwnerMarkerSystem();
	ArucoDictionaryPtr dictionary=
		CalibrationPatternFinder::getArucoDictionary(markerSystem->getTypedDefinitionConst()->getArucoDictionaryType());
	if (!dictionary)
		return;

	const int imageSize= 128;
	const int maxMarkerId= dictionary->bytesList.rows - 1;
	const int clampedId= std::max(0, std::min(currentArucoId, maxMarkerId));

	cv::Mat markerImage;
	cv::aruco::generateImageMarker(*dictionary, clampedId, imageSize, markerImage, 1);

	cv::Mat rgbImage;
	cv::cvtColor(markerImage, rgbImage, cv::COLOR_GRAY2RGB);

	IMkTexturePtr tex= CreateMkTexture((uint16_t)imageSize, (uint16_t)imageSize, rgbImage.data, MK_RGB, MK_RGB);
	if (!tex->createTexture())
		return;
	m_markerTexture= tex;

	// Build world-space quad mesh (XY plane, facing +Z)
	const float halfSize= currentLengthMM / 2000.f; // mm -> m, halved

	struct QuadVertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
	};
	static const uint16_t k_indices[]= {0, 1, 2, 0, 2, 3};
	QuadVertex verts[4]= {
		{glm::vec3(-halfSize, 0.f, -halfSize), glm::vec2(0.f, 0.f)},
		{glm::vec3(-halfSize, 0.f, halfSize), glm::vec2(0.f, 1.f)},
		{glm::vec3(halfSize, 0.f, halfSize), glm::vec2(1.f, 1.f)},
		{glm::vec3(halfSize, 0.f, -halfSize), glm::vec2(1.f, 0.f)},
	};

	MkMaterialConstPtr material= graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PT_TEXTURED);
	if (!material)
		return;

	IMkTriangulatedMeshPtr mesh=
		createMkTriangulatedMesh(graphicsContext, "aruco_marker_quad", (const uint8_t*)verts, sizeof(QuadVertex), 4,
								 (const uint8_t*)k_indices, sizeof(uint16_t), 2, false);

	if (!mesh->setMaterial(material) || !mesh->createResources())
		return;

	mesh->getMaterialInstance()->setTextureBySemantic(eUniformSemantic::rgbTexture, m_markerTexture);

	m_markerMesh= mesh;
}

IMkTexturePtr MarkerComponent::getMarkerTexture(IMkGraphicsContext* graphicsContext)
{
	ensureMarkerResources(graphicsContext);
	return m_markerTexture;
}

void MarkerComponent::renderArucoMarker(IMkGraphicsContext* graphicsContext, IMkCameraConstPtr camera,
										const glm::mat4& transform)
{
	ensureMarkerResources(graphicsContext);

	if (m_markerMesh)
	{
		drawTransformedTriangulatedMesh(camera, transform, m_markerMesh);
	}
}

// -- Lua Binding ----
void MarkerComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<MarkerComponent, MikanComponent>(MarkerComponent::k_componentClassName.c_str())
		.addProperty(
			"arucoId", [](MarkerComponent* c) -> int { return c->getMarkerDefinition()->getArucoId(); },
			[](MarkerComponent* c, int v) { c->getMarkerDefinition()->setArucoId(v); })
		.addProperty(
			"lengthMM", [](MarkerComponent* c) -> float { return c->getMarkerDefinition()->getLengthMM(); },
			[](MarkerComponent* c, float v) { c->getMarkerDefinition()->setLengthMM(v); })
		.addFunction("printMarker", [](MarkerComponent* c) { c->printMarker(); })
		.endClass();
}
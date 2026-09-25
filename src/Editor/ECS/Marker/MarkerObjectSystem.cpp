#include "MarkerObjectSystem.h"
#include "App.h"
#include "CalibrationPatternFinder.h"
#include "Logger.h"
#include "MarkerComponent.h"
#include "MikanAPITypes.h"
#include "MikanMarkerTypes.h"
#include "MikanMathTypes.h"
#include "MikanObject.h"
#include "MikanPropertyDatabase.h"
#include "ProjectManager.h"
#include "OSUtils.h"
#include "ProjectConfig.h"
#include "SelectionComponent.h"
#include "StringUtils.h"

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <hpdf.h>

#include <filesystem>

// -- MarkerObjectSystemDefinition -----
const std::string MarkerObjectSystemDefinition::k_arucoIdListPropertyId= "aruco_id_list";
const std::string MarkerObjectSystemDefinition::k_arucoDictionaryTypePropertyId= "aruco_dictionary_type";
const std::string MarkerObjectSystemDefinition::k_charucoRowsPropertyId= "charuco_rows";
const std::string MarkerObjectSystemDefinition::k_charucoColsPropertyId= "charuco_cols";
const std::string MarkerObjectSystemDefinition::k_charucoSquareLengthMMPropertyId= "charuco_square_length_mm";
const std::string MarkerObjectSystemDefinition::k_charucoMarkerLengthMMPropertyId= "charuco_marker_length_mm";
const std::string MarkerObjectSystemDefinition::k_charucoDictionaryTypePropertyId= "charuco_dictionary_type";

MarkerObjectSystemDefinition::MarkerObjectSystemDefinition(const std::string& configName,
														   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config MarkerObjectSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	// ArUco settings
	const std::string& arucoDictionaryType= k_charucoDictionaryStrings[(int)m_arucoDictionaryType];
	pt[k_arucoDictionaryTypePropertyId]= arucoDictionaryType;

	// ChArUco settings
	pt[k_charucoRowsPropertyId]= m_charucoRows;
	pt[k_charucoColsPropertyId]= m_charucoCols;
	pt[k_charucoSquareLengthMMPropertyId]= m_charucoSquareLengthMM;
	pt[k_charucoMarkerLengthMMPropertyId]= m_charucoMarkerLengthMM;

	const std::string& charucoDictionaryType= k_charucoDictionaryStrings[(int)m_charucoDictionaryType];
	pt[k_charucoDictionaryTypePropertyId]= charucoDictionaryType;

	return pt;
}

void MarkerObjectSystemDefinition::readFromJSON(const configuru::Config& pt)
{
	Super::readFromJSON(pt);

	// Read in the ArUco settings
	const std::string charcuoDictionaryString= pt.get_or<std::string>(
		k_arucoDictionaryTypePropertyId, k_charucoDictionaryStrings[(int)eCharucoDictionaryType::DICT_6X6]);
	m_arucoDictionaryType=
		StringUtils::FindEnumValue<eCharucoDictionaryType>(charcuoDictionaryString, k_charucoDictionaryStrings);

	// Read ChArUco settings
	m_charucoRows= pt.get_or<int>(k_charucoRowsPropertyId, m_charucoRows);
	m_charucoCols= pt.get_or<int>(k_charucoColsPropertyId, m_charucoCols);
	m_charucoSquareLengthMM= pt.get_or<float>(k_charucoSquareLengthMMPropertyId, m_charucoSquareLengthMM);
	m_charucoMarkerLengthMM= pt.get_or<float>(k_charucoMarkerLengthMMPropertyId, m_charucoMarkerLengthMM);

	const std::string charucoDictionaryString= pt.get_or<std::string>(
		k_charucoDictionaryTypePropertyId, k_charucoDictionaryStrings[(int)eCharucoDictionaryType::DICT_6X6]);
	m_charucoDictionaryType=
		StringUtils::FindEnumValue<eCharucoDictionaryType>(charucoDictionaryString, k_charucoDictionaryStrings);
}

void MarkerObjectSystemDefinition::setArucoDictionaryType(eCharucoDictionaryType dictionaryType)
{
	if (dictionaryType != m_arucoDictionaryType && (int)dictionaryType >= 0
		&& (int)dictionaryType < (int)eCharucoDictionaryType::COUNT)
	{
		m_arucoDictionaryType= dictionaryType;
		notifyPropertyChanged(ConfigPropertyChangeSet()
								  .addPropertyName(k_arucoDictionaryTypePropertyId)
								  .addPropertyName(k_arucoIdListPropertyId));
	}
}

void MarkerObjectSystemDefinition::getArucoIdList(std::vector<int>& outMarkerIdList) const
{
	outMarkerIdList.clear();

	int arucoMarkerCount= 0;
	switch (m_arucoDictionaryType)
	{
	case eCharucoDictionaryType::DICT_4X4:
		arucoMarkerCount= 4 * 4;
		break;
	case eCharucoDictionaryType::DICT_5X5:
		arucoMarkerCount= 5 * 5;
		break;
	case eCharucoDictionaryType::DICT_6X6:
		arucoMarkerCount= 6 * 6;
		break;
	case eCharucoDictionaryType::DICT_7X7:
		arucoMarkerCount= 7 * 7;
		break;
	}

	for (int i= 0; i < arucoMarkerCount; i++)
	{
		outMarkerIdList.push_back(i);
	}
}

void MarkerObjectSystemDefinition::setCharucoRows(int charucoRows)
{
	if (charucoRows != m_charucoRows && charucoRows >= MIN_CHARUCO_CORNER_COUNT
		&& charucoRows <= MAX_CHARUCO_CORNER_COUNT)
	{
		m_charucoRows= charucoRows;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_charucoRowsPropertyId));
	}
}

void MarkerObjectSystemDefinition::setCharucoCols(int charucoCols)
{
	if (charucoCols != m_charucoCols && charucoCols >= MIN_CHARUCO_CORNER_COUNT
		&& charucoCols <= MAX_CHARUCO_CORNER_COUNT)
	{
		m_charucoCols= charucoCols;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_charucoColsPropertyId));
	}
}

void MarkerObjectSystemDefinition::setCharucoSquareLengthMM(float charucoSquareLengthMM)
{
	if (charucoSquareLengthMM != m_charucoSquareLengthMM)
	{
		m_charucoSquareLengthMM= charucoSquareLengthMM;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_charucoSquareLengthMMPropertyId));
	}
}

void MarkerObjectSystemDefinition::setCharucoMarkerLengthMM(float charucoMarkerLengthMM)
{
	if (charucoMarkerLengthMM != m_charucoMarkerLengthMM)
	{
		m_charucoMarkerLengthMM= charucoMarkerLengthMM;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_charucoMarkerLengthMMPropertyId));
	}
}

void MarkerObjectSystemDefinition::setCharucoDictionaryType(eCharucoDictionaryType charucoDictionaryType)
{
	if (charucoDictionaryType != m_charucoDictionaryType && (int)charucoDictionaryType >= 0
		&& (int)charucoDictionaryType < (int)eCharucoDictionaryType::COUNT)
	{
		m_charucoDictionaryType= charucoDictionaryType;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_charucoDictionaryTypePropertyId));
	}
}

// -- MarkerObjectSystem -----
MarkerObjectSystem::MarkerObjectSystem(ProjectManagerPtr ownerObjectSystemManager)
	: Super::MikanTypedObjectSystem(ownerObjectSystemManager)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle MarkerObjectSystem::getClientAPIValuesStructType() const
{
	return &MikanMarkerSystemValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void MarkerObjectSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(MarkerObjectSystemDefinition::k_arucoIdListPropertyId,
																  MikanVariantType::INT_ARRAY)
								 ->setReadOnly()
								 ->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 MarkerObjectSystemDefinition::k_arucoDictionaryTypePropertyId, MikanVariantType::INT)
								 ->setDefaultValue((int)DEFAULT_ARUCO_DICTIONARY_TYPE));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 MarkerObjectSystemDefinition::k_charucoDictionaryTypePropertyId, MikanVariantType::INT)
								 ->setDefaultValue((int)DEFAULT_CHARUCO_DICTIONARY_TYPE));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(MarkerObjectSystemDefinition::k_charucoRowsPropertyId,
																  MikanVariantType::INT)
								 ->setDefaultValue(CHARUCO_PATTERN_H));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(MarkerObjectSystemDefinition::k_charucoColsPropertyId,
																  MikanVariantType::INT)
								 ->setDefaultValue(CHARUCO_PATTERN_W));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(MarkerObjectSystemDefinition::k_charucoSquareLengthMMPropertyId,
											 MikanVariantType::FLOAT)
			->setDefaultValue(DEFAULT_CHARUCO_SQUARE_LEN_MM));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(MarkerObjectSystemDefinition::k_charucoMarkerLengthMMPropertyId,
											 MikanVariantType::FLOAT)
			->setDefaultValue(DEFAULT_CHARUCO_MARKER_LEN_MM));
}

bool MarkerObjectSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	MarkerObjectSystemDefinitionConstPtr markerSystemDefinition= getTypedDefinitionConst();

	if (propertyName == MarkerObjectSystemDefinition::k_arucoIdListPropertyId)
	{
		std::vector<int> arucoIdList;
		markerSystemDefinition->getArucoIdList(arucoIdList);
		outValue= arucoIdList;
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_arucoDictionaryTypePropertyId)
	{
		int enumIntValue= (int)markerSystemDefinition->getArucoDictionaryType();
		outValue= enumIntValue;
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoDictionaryTypePropertyId)
	{
		int enumIntValue= (int)markerSystemDefinition->getCharucoDictionaryType();
		outValue= enumIntValue;
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoRowsPropertyId)
	{
		outValue= markerSystemDefinition->getCharucoRows();
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoColsPropertyId)
	{
		outValue= markerSystemDefinition->getCharucoCols();
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoSquareLengthMMPropertyId)
	{
		outValue= markerSystemDefinition->getCharucoSquareLengthMM();
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoMarkerLengthMMPropertyId)
	{
		outValue= markerSystemDefinition->getCharucoMarkerLengthMM();
		return true;
	}

	return Super::getPropertyValue(propertyName, outValue);
}

bool MarkerObjectSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	MarkerObjectSystemDefinitionPtr markerSystemDefinition= getTypedDefinition();

	if (propertyName == MarkerObjectSystemDefinition::k_arucoDictionaryTypePropertyId)
	{
		const auto dictionaryType= (eCharucoDictionaryType)inValue.getIntValue();
		markerSystemDefinition->setArucoDictionaryType(dictionaryType);
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoDictionaryTypePropertyId)
	{
		const auto dictionaryType= (eCharucoDictionaryType)inValue.getIntValue();
		markerSystemDefinition->setCharucoDictionaryType(dictionaryType);
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoRowsPropertyId)
	{
		int charucoRows= inValue.getIntValue();
		markerSystemDefinition->setCharucoRows(charucoRows);
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoColsPropertyId)
	{
		int charucoCols= inValue.getIntValue();
		markerSystemDefinition->setCharucoCols(charucoCols);
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoSquareLengthMMPropertyId)
	{
		float charucoSquareLengthMM= inValue.getFloatValue();
		markerSystemDefinition->setCharucoSquareLengthMM(charucoSquareLengthMM);
		return true;
	}
	else if (propertyName == MarkerObjectSystemDefinition::k_charucoMarkerLengthMMPropertyId)
	{
		float charucoMarkerLengthMM= inValue.getFloatValue();
		markerSystemDefinition->setCharucoMarkerLengthMM(charucoMarkerLengthMM);
		return true;
	}

	return Super::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string MarkerObjectSystem::k_printCharucoMarkerFunctionId= "print_checkerboard";

void MarkerObjectSystem::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	Super::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_printCharucoMarkerFunctionId, "Print Marker"));
}

bool MarkerObjectSystem::invokeFunction(const std::string& functionName)
{
	if (functionName == k_printCharucoMarkerFunctionId)
	{
		printMarker();
		return true;
	}

	return Super::invokeFunction(functionName);
}

void MarkerObjectSystem::printMarker()
{
	MarkerObjectSystemDefinitionPtr markerSystemDefinition= getTypedDefinition();
	ArucoDictionaryPtr dictionary=
		CalibrationPatternFinder::getArucoDictionary(markerSystemDefinition->getArucoDictionaryType());

	// Get ChArUco board parameters from config
	const int charucoRows= markerSystemDefinition->getCharucoRows();
	const int charucoCols= markerSystemDefinition->getCharucoCols();
	const float squareLengthMM= markerSystemDefinition->getCharucoSquareLengthMM();
	const float markerLengthMM= markerSystemDefinition->getCharucoMarkerLengthMM();
	const eCharucoDictionaryType charucoDictionaryType= markerSystemDefinition->getCharucoDictionaryType();

	// Create ChArUco board
	cv::aruco::CharucoBoard charucoBoard(cv::Size(charucoCols, charucoRows), squareLengthMM, markerLengthMM,
										 *dictionary.get());

	// Generate ChArUco board image
	cv::Mat boardImage;
	const int boardSizePixels= 2400; // High resolution for printing
	charucoBoard.generateImage(cv::Size(boardSizePixels, boardSizePixels), boardImage, 10, 1);

	// Create PDF using libharu
	HPDF_Doc pdf= HPDF_New(NULL, NULL);
	if (!pdf)
	{
		MIKAN_LOG_ERROR("MarkerObjectSystem::printMarker") << "Failed to create PDF document";
		return;
	}

	// Add a page (A4 size, landscape to fit board better)
	HPDF_Page page= HPDF_AddPage(pdf);
	HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_LANDSCAPE);

	// Get page dimensions
	const float pageWidth= HPDF_Page_GetWidth(page);
	const float pageHeight= HPDF_Page_GetHeight(page);

	// Calculate board size in points (1mm = 2.83465 points)
	const float mmToPoints= 2.83465f;
	const float boardWidthMM= charucoCols * squareLengthMM;
	const float boardHeightMM= charucoRows * squareLengthMM;
	const float boardWidthPoints= boardWidthMM * mmToPoints;
	const float boardHeightPoints= boardHeightMM * mmToPoints;

	// Center the board on the page
	const float xPos= (pageWidth - boardWidthPoints) / 2.0f;
	const float yPos= (pageHeight - boardHeightPoints) / 2.0f;

	// Convert OpenCV image to PNG format in memory
	std::vector<uchar> pngBuffer;
	cv::imencode(".png", boardImage, pngBuffer);

	// Load image from memory into libharu
	HPDF_Image image= HPDF_LoadPngImageFromMem(pdf, pngBuffer.data(), (HPDF_UINT)pngBuffer.size());
	if (!image)
	{
		MIKAN_LOG_ERROR("MarkerObjectSystem::printMarker") << "Failed to load board image into PDF";
		HPDF_Free(pdf);
		return;
	}

	// Draw the board image on the page
	HPDF_Page_DrawImage(page, image, xPos, yPos, boardWidthPoints, boardHeightPoints);

	// Add text label below the board
	HPDF_Font font= HPDF_GetFont(pdf, "Helvetica", NULL);
	HPDF_Page_SetFontAndSize(page, font, 12);

	std::string labelText= "ChArUco Board: " + std::to_string(charucoRows) + "x" + std::to_string(charucoCols)
						   + " | Square: " + std::to_string((int)squareLengthMM) + "mm"
						   + " | Marker: " + std::to_string((int)markerLengthMM) + "mm";
	const float textWidth= HPDF_Page_TextWidth(page, labelText.c_str());
	const float textXPos= (pageWidth - textWidth) / 2.0f;
	const float textYPos= yPos - 30.0f;

	HPDF_Page_BeginText(page);
	HPDF_Page_TextOut(page, textXPos, textYPos, labelText.c_str());
	HPDF_Page_EndText(page);

	// Save PDF to temp file
	std::filesystem::path pdfFilePath=
		std::filesystem::temp_directory_path()
		/ ("MikanCharucoBoard_" + std::to_string(charucoRows) + "x" + std::to_string(charucoCols) + ".pdf");

	HPDF_STATUS status= HPDF_SaveToFile(pdf, pdfFilePath.string().c_str());
	HPDF_Free(pdf);

	if (status != HPDF_OK)
	{
		MIKAN_LOG_ERROR("MarkerObjectSystem::printMarker") << "Failed to save PDF to: " << pdfFilePath.string();
		return;
	}

	MIKAN_LOG_INFO("MarkerObjectSystem::printMarker") << "PDF saved to: " << pdfFilePath.string();

	// Open PDF in default viewer (which will have print functionality)
	if (!OSUtils::openFileWithDefaultApplication(pdfFilePath))
	{
		MIKAN_LOG_ERROR("MarkerObjectSystem::printMarker") << "Failed to open PDF viewer";
	}
}

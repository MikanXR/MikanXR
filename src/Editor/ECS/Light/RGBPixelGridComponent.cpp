#include "RGBPixelGridComponent.h"
#include "EditorObjectSystem.h"
#include "BoxColliderComponent.h"
#include "Colors.h"
#include "IDMXManager.h"
#include "DMXObjectSystem.h"
#include "EnumPropertyMetaData.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkTriangulatedMesh.h"
#include "MathTypeConversion.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanObject.h"
#include "MikanShaderCache.h"
#include "MikanTextRenderer.h"
#include "MikanLightTypes.h"
#include "MikanVariantTypes.h"
#include "MkMaterialInstance.h"
#include "SelectionComponent.h"
#include "StringUtils.h"
#include "TextStyle.h"

#include "glm/gtc/matrix_transform.hpp"

#include <algorithm>
#include <cstring>

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

namespace
{
const std::string g_pixelGridOriginStrings[(int)eDMXPixelGridOrigin::COUNT]= {
	"upperLeft",
	"upperRight",
	"lowerLeft",
	"lowerRight",
};

// Localization keys, not display text (see EnumPropertyMetaData)
const std::string k_pixelGridOriginLocKeys[]= {
	"propertyValues.pixel_origin_upper_left",
	"propertyValues.pixel_origin_upper_right",
	"propertyValues.pixel_origin_lower_left",
	"propertyValues.pixel_origin_lower_right",
};
} // namespace

const std::string* k_pixelGridOriginStrings= g_pixelGridOriginStrings;

// -- RGBPixelGridDefinition -----
const std::string RGBPixelGridDefinition::k_gridColumnsPropertyId= "grid_columns";
const std::string RGBPixelGridDefinition::k_gridRowsPropertyId= "grid_rows";
const std::string RGBPixelGridDefinition::k_pixelSizeMMPropertyId= "pixel_size_mm";
const std::string RGBPixelGridDefinition::k_pixelSeparationMMPropertyId= "pixel_separation_mm";
const std::string RGBPixelGridDefinition::k_originPixelPropertyId= "origin_pixel";
const std::string RGBPixelGridDefinition::k_zigZagPropertyId= "zig_zag";

RGBPixelGridDefinition::RGBPixelGridDefinition()
	: DMXFixtureComponentDefinition()
{
	resizeGrid(m_columns, m_rows);
}

RGBPixelGridDefinition::RGBPixelGridDefinition(MikanLightID gridId)
	: DMXFixtureComponentDefinition(gridId)
{
	resizeGrid(m_columns, m_rows);
}

configuru::Config RGBPixelGridDefinition::writeToJSON()
{
	configuru::Config pt= DMXFixtureComponentDefinition::writeToJSON();

	pt[k_gridColumnsPropertyId]= m_columns;
	pt[k_gridRowsPropertyId]= m_rows;
	writeVector3f(pt, k_pixelSizeMMPropertyId.c_str(), m_pixelSizeMM);
	writeVector2f(pt, k_pixelSeparationMMPropertyId.c_str(), m_pixelSeparationMM);
	pt[k_originPixelPropertyId]= k_pixelGridOriginStrings[(int)m_originPixel];
	pt[k_zigZagPropertyId]= m_bZigZag;

	return pt;
}

void RGBPixelGridDefinition::readFromJSON(const configuru::Config& pt)
{
	DMXFixtureComponentDefinition::readFromJSON(pt);

	const int cols= pt.get_or<int>(k_gridColumnsPropertyId, 8);
	const int rows= pt.get_or<int>(k_gridRowsPropertyId, 8);
	resizeGrid(cols, rows);

	// readVector*f zeroes its output when the key is missing, so a project
	// saved before these properties existed would load a grid of zero size.
	// Read only what is present and leave the rest at its default.
	if (pt.has_key(k_pixelSizeMMPropertyId))
		readVector3f(pt, k_pixelSizeMMPropertyId.c_str(), m_pixelSizeMM);
	if (pt.has_key(k_pixelSeparationMMPropertyId))
		readVector2f(pt, k_pixelSeparationMMPropertyId.c_str(), m_pixelSeparationMM);

	const std::string originName=
		pt.get_or<std::string>(k_originPixelPropertyId, k_pixelGridOriginStrings[(int)eDMXPixelGridOrigin::upperLeft]);
	const eDMXPixelGridOrigin origin=
		StringUtils::FindEnumValue<eDMXPixelGridOrigin>(originName, k_pixelGridOriginStrings);
	m_originPixel= (origin != eDMXPixelGridOrigin::INVALID) ? origin : eDMXPixelGridOrigin::upperLeft;

	m_bZigZag= pt.get_or<bool>(k_zigZagPropertyId, m_bZigZag);
}

bool RGBPixelGridDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!DMXFixtureComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* values= initParams.getTypedPointer<MikanRGBPixelGridComponentValues>();
	if (values)
	{
		resizeGrid(values->grid_columns, values->grid_rows);
		m_pixelSizeMM= values->pixel_size_mm;
		m_pixelSeparationMM= values->pixel_separation_mm;
		m_originPixel= (eDMXPixelGridOrigin)values->origin_pixel;
		m_bZigZag= values->zig_zag;
	}

	return true;
}

void RGBPixelGridDefinition::setPixelSizeMM(const MikanVector3f& sizeMM)
{
	if (m_pixelSizeMM.x != sizeMM.x || m_pixelSizeMM.y != sizeMM.y || m_pixelSizeMM.z != sizeMM.z)
	{
		m_pixelSizeMM= sizeMM;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_pixelSizeMMPropertyId));
	}
}

void RGBPixelGridDefinition::setPixelSeparationMM(const MikanVector2f& separationMM)
{
	if (m_pixelSeparationMM.x != separationMM.x || m_pixelSeparationMM.y != separationMM.y)
	{
		m_pixelSeparationMM= separationMM;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_pixelSeparationMMPropertyId));
	}
}

void RGBPixelGridDefinition::setOriginPixel(eDMXPixelGridOrigin originPixel)
{
	if (m_originPixel != originPixel && originPixel != eDMXPixelGridOrigin::INVALID)
	{
		m_originPixel= originPixel;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_originPixelPropertyId));
	}
}

void RGBPixelGridDefinition::setZigZag(bool bZigZag)
{
	if (m_bZigZag != bZigZag)
	{
		m_bZigZag= bZigZag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_zigZagPropertyId));
	}
}

int RGBPixelGridDefinition::getPixelWireIndex(int col, int row) const
{
	if (col < 0 || col >= m_columns || row < 0 || row >= m_rows)
		return -1;

	// The origin corner fixes which end of each axis the scan starts from
	const bool bScanFromTop=
		m_originPixel == eDMXPixelGridOrigin::upperLeft || m_originPixel == eDMXPixelGridOrigin::upperRight;
	const bool bScanFromLeft=
		m_originPixel == eDMXPixelGridOrigin::upperLeft || m_originPixel == eDMXPixelGridOrigin::lowerLeft;

	const int scanRow= bScanFromTop ? row : (m_rows - 1 - row);
	int scanCol= bScanFromLeft ? col : (m_columns - 1 - col);

	// Serpentine wiring runs every other row backwards
	if (m_bZigZag && (scanRow % 2) == 1)
		scanCol= m_columns - 1 - scanCol;

	return scanRow * m_columns + scanCol;
}

bool RGBPixelGridDefinition::getPixelGridPosition(int wireIndex, int& outCol, int& outRow) const
{
	if (wireIndex < 0 || wireIndex >= getPixelCount() || m_columns <= 0)
		return false;

	const int scanRow= wireIndex / m_columns;
	int scanCol= wireIndex % m_columns;

	if (m_bZigZag && (scanRow % 2) == 1)
		scanCol= m_columns - 1 - scanCol;

	const bool bScanFromTop=
		m_originPixel == eDMXPixelGridOrigin::upperLeft || m_originPixel == eDMXPixelGridOrigin::upperRight;
	const bool bScanFromLeft=
		m_originPixel == eDMXPixelGridOrigin::upperLeft || m_originPixel == eDMXPixelGridOrigin::lowerLeft;

	outRow= bScanFromTop ? scanRow : (m_rows - 1 - scanRow);
	outCol= bScanFromLeft ? scanCol : (m_columns - 1 - scanCol);

	return true;
}

glm::vec3 RGBPixelGridDefinition::getPixelLocalCenter(int col, int row) const
{
	const float separationX= m_pixelSeparationMM.x * k_millimeters_to_meters;
	const float separationY= m_pixelSeparationMM.y * k_millimeters_to_meters;

	// Columns run along +X and rows along -Y, centered on the component origin
	const float x= ((float)col - (float)(m_columns - 1) * 0.5f) * separationX;
	const float y= -((float)row - (float)(m_rows - 1) * 0.5f) * separationY;

	return glm::vec3(x, y, 0.f);
}

glm::vec3 RGBPixelGridDefinition::getGridLocalHalfExtents() const
{
	const float separationX= m_pixelSeparationMM.x * k_millimeters_to_meters;
	const float separationY= m_pixelSeparationMM.y * k_millimeters_to_meters;
	const float sizeX= m_pixelSizeMM.x * k_millimeters_to_meters;
	const float sizeY= m_pixelSizeMM.y * k_millimeters_to_meters;
	const float sizeZ= m_pixelSizeMM.z * k_millimeters_to_meters;

	// A zero separation degrades to one pixel's own size, so this never collapses
	return glm::vec3(((float)(m_columns - 1) * separationX + sizeX) * 0.5f,
					 ((float)(m_rows - 1) * separationY + sizeY) * 0.5f, sizeZ * 0.5f);
}

void RGBPixelGridDefinition::resizeGrid(int columns, int rows)
{
	m_columns= std::max(1, columns);
	m_rows= std::max(1, rows);
	setDMXChannelCount(static_cast<uint16_t>(getTotalChannels()));

	notifyPropertyChanged(
		ConfigPropertyChangeSet().addPropertyName(k_gridColumnsPropertyId).addPropertyName(k_gridRowsPropertyId));
}

void RGBPixelGridDefinition::setColumns(int columns)
{
	if (m_columns != columns)
		resizeGrid(columns, m_rows);
}

void RGBPixelGridDefinition::setRows(int rows)
{
	if (m_rows != rows)
		resizeGrid(m_columns, rows);
}

// -- RGBPixelGridComponent -----
RGBPixelGridComponent::RGBPixelGridComponent(MikanObjectWeakPtr owner)
	: DMXFixtureComponent(owner)
{
}

void RGBPixelGridComponent::init()
{
	DMXFixtureComponent::init();

	SelectionComponentPtr selectionComponentPtr= getOwnerObject()->getComponentOfType<SelectionComponent>();
	if (selectionComponentPtr)
	{
		selectionComponentPtr->OnInteractionRayOverlapEnter+=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionRayOverlapEnter);
		selectionComponentPtr->OnInteractionRayOverlapExit+=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionRayOverlapExit);
		selectionComponentPtr->OnInteractionSelected+=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionSelected);
		selectionComponentPtr->OnInteractionUnselected+=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionUnselected);

		m_selectionComponent= selectionComponentPtr;
	}

	m_boxCollider= getOwnerObject()->getComponentOfType<BoxColliderComponent>();

	// Ensure pixel data buffer is sized correctly based on definition.
	resizePixelDataBuffer();
	rebuildCollisionVolume();
	rebuildPixelMesh();

	propogateWorldTransformChange(eTransformChangeType::propogateWorldTransform);
}

void RGBPixelGridComponent::dispose()
{
	SelectionComponentPtr selectionComponentPtr= m_selectionComponent.lock();
	if (selectionComponentPtr)
	{
		selectionComponentPtr->OnInteractionRayOverlapEnter-=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionRayOverlapEnter);
		selectionComponentPtr->OnInteractionRayOverlapExit-=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionRayOverlapExit);
		selectionComponentPtr->OnInteractionSelected-=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionSelected);
		selectionComponentPtr->OnInteractionUnselected-=
			MakeDelegate(this, &RGBPixelGridComponent::onInteractionUnselected);
	}

	m_pixelMesh= nullptr;

	DMXFixtureComponent::dispose();
}

void RGBPixelGridComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
													const ConfigPropertyChangeSet& changedPropertySet)
{
	DMXFixtureComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	const bool bGridResized= changedPropertySet.hasPropertyName(RGBPixelGridDefinition::k_gridColumnsPropertyId)
							 || changedPropertySet.hasPropertyName(RGBPixelGridDefinition::k_gridRowsPropertyId);
	if (bGridResized)
	{
		resizePixelDataBuffer();
	}

	// Any layout change moves the pixels, so the selection box follows
	if (bGridResized || changedPropertySet.hasPropertyName(RGBPixelGridDefinition::k_pixelSizeMMPropertyId)
		|| changedPropertySet.hasPropertyName(RGBPixelGridDefinition::k_pixelSeparationMMPropertyId))
	{
		rebuildCollisionVolume();
	}
}

void RGBPixelGridComponent::resizePixelDataBuffer()
{
	RGBPixelGridDefinitionConstPtr def= getRGBPixelGridDefinitionConst();
	const size_t requiredSize= static_cast<size_t>(def->getTotalChannels());

	if (m_pixelData.size() != requiredSize)
		m_pixelData.assign(requiredSize, 0);
}

void RGBPixelGridComponent::rebuildCollisionVolume()
{
	BoxColliderComponentPtr boxColliderPtr= m_boxCollider.lock();
	if (!boxColliderPtr)
		return;

	// The grid is centered on the component origin, so the box is too
	boxColliderPtr->setHalfExtents(getRGBPixelGridDefinitionConst()->getGridLocalHalfExtents());
	boxColliderPtr->setRelativeTransform(GlmTransform());
}

void RGBPixelGridComponent::rebuildPixelMesh()
{
	m_pixelMesh= nullptr;

	IEditorWindow* ownerWindow= getOwnerEditorWindow();
	if (!ownerWindow)
		return;

	IMkGraphicsContextPtr graphicsContext= ownerWindow->getGraphicsContext();

	// One unit cube centered on the origin, scaled per pixel at draw time
	struct PosVert
	{
		float x, y, z;
	};
	const PosVert verts[8]= {
		{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
		{-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f},
	};
	const uint32_t indices[36]= {
		0, 2, 1, 0, 3, 2, // -Z
		4, 5, 6, 4, 6, 7, // +Z
		0, 1, 5, 0, 5, 4, // -Y
		3, 7, 6, 3, 6, 2, // +Y
		0, 4, 7, 0, 7, 3, // -X
		1, 2, 6, 1, 6, 5, // +X
	};

	m_pixelMesh=
		createMkTriangulatedMesh(graphicsContext.get(), "pixelGridPixel", reinterpret_cast<const uint8_t*>(verts),
								 sizeof(PosVert), 8, reinterpret_cast<const uint8_t*>(indices), sizeof(uint32_t), 12,
								 false); // data uploaded to GPU in createResources(), no need for mesh to own CPU copy

	if (m_pixelMesh)
	{
		MkMaterialConstPtr material=
			graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_SOLID_COLOR);
		m_pixelMesh->setMaterial(material);
		m_pixelMesh->createResources();
	}
}

void RGBPixelGridComponent::onInteractionRayOverlapEnter(const ColliderRaycastHitResult& hitResult)
{
	m_bIsHovered= true;
}

void RGBPixelGridComponent::onInteractionRayOverlapExit(const ColliderRaycastHitResult& hitResult)
{
	m_bIsHovered= false;
}

void RGBPixelGridComponent::onInteractionSelected() { m_bIsSelected= true; }

void RGBPixelGridComponent::onInteractionUnselected() { m_bIsSelected= false; }

void RGBPixelGridComponent::setPixel(int col, int row, uint8_t r, uint8_t g, uint8_t b)
{
	const int wireIndex= getRGBPixelGridDefinitionConst()->getPixelWireIndex(col, row);
	if (wireIndex < 0)
		return;

	const size_t idx= static_cast<size_t>(wireIndex) * 3;
	if (idx + 2 >= m_pixelData.size())
		return;

	m_pixelData[idx + 0]= r;
	m_pixelData[idx + 1]= g;
	m_pixelData[idx + 2]= b;
}

bool RGBPixelGridComponent::getPixel(int col, int row, uint8_t& outR, uint8_t& outG, uint8_t& outB) const
{
	const int wireIndex= getRGBPixelGridDefinitionConst()->getPixelWireIndex(col, row);
	if (wireIndex < 0)
		return false;

	const size_t idx= static_cast<size_t>(wireIndex) * 3;
	if (idx + 2 >= m_pixelData.size())
		return false;

	outR= m_pixelData[idx + 0];
	outG= m_pixelData[idx + 1];
	outB= m_pixelData[idx + 2];
	return true;
}

void RGBPixelGridComponent::setAllPixels(const uint8_t* rgbData, int count)
{
	const int copyCount= std::min(count, static_cast<int>(m_pixelData.size()));
	std::memcpy(m_pixelData.data(), rgbData, copyCount);
}

void RGBPixelGridComponent::fillPixels(uint8_t r, uint8_t g, uint8_t b)
{
	for (int i= 0; i < static_cast<int>(m_pixelData.size()); i+= 3)
	{
		m_pixelData[i + 0]= r;
		m_pixelData[i + 1]= g;
		m_pixelData[i + 2]= b;
	}
}

void RGBPixelGridComponent::getChannelValues(std::vector<uint8_t>& outValues) const { outValues= m_pixelData; }

void RGBPixelGridComponent::setChannelValues(const std::vector<uint8_t>& values)
{
	std::fill(m_pixelData.begin(), m_pixelData.end(), 0);
	setAllPixels(values.data(), static_cast<int>(values.size()));
	sendDMXData();
}

void RGBPixelGridComponent::sendDMXData() const
{
	DMXObjectSystemPtr dmxObjectSystem= getDMXObjectSystem();
	RGBPixelGridDefinitionPtr def= getRGBPixelGridDefinition();
	if (!def || def->getIsDisabled() || !dmxObjectSystem)
		return;

	const std::vector<uint8_t>& pixelData= getPixelData();
	const int totalChannels= def->getTotalChannels();
	if (totalChannels == 0)
		return;

	const uint16_t startUniverse= def->getDMXUniverse();
	const uint16_t startChannel= def->getDMXStartChannel(); // 1-based
	const int startOffset= startChannel - 1;                // 0-based absolute offset

	// Split pixel data across E1.31 universes (512 slots per universe).
	int channelsRemaining= totalChannels;
	int pixelDataOffset= 0;

	while (channelsRemaining > 0)
	{
		const int absoluteOffset= startOffset + pixelDataOffset;
		const uint16_t targetUniverse= static_cast<uint16_t>(startUniverse + (absoluteOffset / 512));
		const uint16_t slotInUniverse= static_cast<uint16_t>((absoluteOffset % 512) + 1); // 1-based

		const int slotsAvailableInUniverse= 512 - (slotInUniverse - 1);
		const int chunkSize= std::min(channelsRemaining, slotsAvailableInUniverse);

		dmxObjectSystem->writeUniverseData(targetUniverse, slotInUniverse, pixelData.data() + pixelDataOffset,
										   static_cast<uint16_t>(chunkSize));

		pixelDataOffset+= chunkSize;
		channelsRemaining-= chunkSize;
	}
}

// -- IEntityAccessor --
Serialization::StructTypeHandle RGBPixelGridComponent::getClientAPIValuesStructType() const
{
	return &MikanRGBPixelGridComponentValues::staticGetArchetype();
}

// -- IPropertyInterface --
void RGBPixelGridComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	DMXFixtureComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RGBPixelGridDefinition::k_gridColumnsPropertyId, MikanVariantType::INT));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RGBPixelGridDefinition::k_gridRowsPropertyId, MikanVariantType::INT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(RGBPixelGridDefinition::k_pixelSizeMMPropertyId,
																  MikanVariantType::VECTOR3F)
								 ->setDefaultValue(MikanVector3f(30.f, 30.f, 10.f)));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(RGBPixelGridDefinition::k_pixelSeparationMMPropertyId,
																  MikanVariantType::VECTOR2F)
								 ->setDefaultValue(MikanVector2f(40.f, 40.f)));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RGBPixelGridDefinition::k_originPixelPropertyId, MikanVariantType::INT)
			->setDefaultValue((int)eDMXPixelGridOrigin::upperLeft)
			->addMetaData(
				std::make_shared<EnumPropertyMetaData>(k_pixelGridOriginLocKeys, (int)eDMXPixelGridOrigin::COUNT)));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RGBPixelGridDefinition::k_zigZagPropertyId, MikanVariantType::BOOL));
}

bool RGBPixelGridComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	RGBPixelGridDefinitionPtr def= getRGBPixelGridDefinition();

	if (propertyName == RGBPixelGridDefinition::k_gridColumnsPropertyId)
	{
		outValue= def->getColumns();
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_gridRowsPropertyId)
	{
		outValue= def->getRows();
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_pixelSizeMMPropertyId)
	{
		outValue= def->getPixelSizeMM();
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_pixelSeparationMMPropertyId)
	{
		outValue= def->getPixelSeparationMM();
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_originPixelPropertyId)
	{
		outValue= (int)def->getOriginPixel();
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_zigZagPropertyId)
	{
		outValue= def->getZigZag();
		return true;
	}

	return DMXFixtureComponent::getPropertyValue(propertyName, outValue);
}

bool RGBPixelGridComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	RGBPixelGridDefinitionPtr def= getRGBPixelGridDefinition();

	if (propertyName == RGBPixelGridDefinition::k_gridColumnsPropertyId)
	{
		def->setColumns(inValue.getIntValue());
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_gridRowsPropertyId)
	{
		def->setRows(inValue.getIntValue());
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_pixelSizeMMPropertyId)
	{
		def->setPixelSizeMM(inValue.getVector3fValue());
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_pixelSeparationMMPropertyId)
	{
		def->setPixelSeparationMM(inValue.getVector2fValue());
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_originPixelPropertyId)
	{
		def->setOriginPixel((eDMXPixelGridOrigin)inValue.getIntValue());
		return true;
	}
	else if (propertyName == RGBPixelGridDefinition::k_zigZagPropertyId)
	{
		def->setZigZag(inValue.getBoolValue());
		return true;
	}

	return DMXFixtureComponent::setPropertyValue(propertyName, inValue);
}

// -- customRender --
void RGBPixelGridComponent::customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const
{
	RGBPixelGridDefinitionPtr def= getRGBPixelGridDefinition();
	if (!def || def->getIsDisabled())
		return;

	const glm::mat4 xform= getWorldTransform();
	const glm::vec3 position= glm::vec3(xform[3]);
	const int columns= def->getColumns();
	const int rows= def->getRows();

	// One solid box per pixel, in that pixel's current color
	if (m_pixelMesh)
	{
		const MikanVector3f& sizeMM= def->getPixelSizeMM();
		glm::mat4 pixelScale(1.f);
		pixelScale[0][0]= sizeMM.x * k_millimeters_to_meters;
		pixelScale[1][1]= sizeMM.y * k_millimeters_to_meters;
		pixelScale[2][2]= sizeMM.z * k_millimeters_to_meters;

		MkMaterialInstancePtr materialInstance= m_pixelMesh->getMaterialInstance();
		for (int row= 0; row < rows; ++row)
		{
			for (int col= 0; col < columns; ++col)
			{
				uint8_t r= 0, g= 0, b= 0;
				if (!getPixel(col, row, r, g, b))
					continue;

				materialInstance->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA,
													glm::vec4(r / 255.f, g / 255.f, b / 255.f, 1.f));

				const glm::mat4 pixelXform=
					xform * glm::translate(glm::mat4(1.f), def->getPixelLocalCenter(col, row)) * pixelScale;
				drawTransformedTriangulatedMesh(viewportCamera, pixelXform, m_pixelMesh);
			}
		}
	}

	// The wiring order is only worth the line clutter while the grid is picked
	if (m_bIsSelected || m_bIsHovered)
	{
		const glm::vec3 outlineColor= m_bIsSelected ? Colors::Yellow : Colors::LightGray;
		drawTransformedBox(graphicsContext, xform, def->getGridLocalHalfExtents(), outlineColor);

		const int pixelCount= def->getPixelCount();
		int prevCol= 0, prevRow= 0;
		for (int wireIndex= 0; wireIndex < pixelCount; ++wireIndex)
		{
			int col= 0, row= 0;
			if (!def->getPixelGridPosition(wireIndex, col, row))
				continue;

			if (wireIndex > 0)
			{
				drawSegment(graphicsContext, xform, def->getPixelLocalCenter(prevCol, prevRow),
							def->getPixelLocalCenter(col, row), Colors::Yellow);
			}

			prevCol= col;
			prevRow= row;
		}
	}

	drawTransformedAxes(graphicsContext, xform, 0.05f, 0.05f, 0.05f);

	if (getObjectSystemOfType<EditorObjectSystem>()->getEditorSettings().bRenderComponentNames)
	{
		TextStyle style= getDefaultTextStyle();
		wchar_t wszName[256];
		StringUtils::convertMbsToWcs(def->getComponentName().c_str(), wszName, sizeof(wszName));
		drawTextAtWorldPosition(graphicsContext, style, position, L"%s [%dx%d]", wszName, columns, rows);
	}
}

// -- Lua Binding --
void RGBPixelGridComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<RGBPixelGridComponent, DMXFixtureComponent>(RGBPixelGridComponent::k_componentClassName.c_str())
		.addProperty(
			"columns", [](RGBPixelGridComponent* c) -> int { return c->getRGBPixelGridDefinition()->getColumns(); },
			[](RGBPixelGridComponent* c, int v) { c->getRGBPixelGridDefinition()->setColumns(v); })
		.addProperty(
			"rows", [](RGBPixelGridComponent* c) -> int { return c->getRGBPixelGridDefinition()->getRows(); },
			[](RGBPixelGridComponent* c, int v) { c->getRGBPixelGridDefinition()->setRows(v); })
		.addProperty(
			"originPixel", [](RGBPixelGridComponent* c) -> int
			{ return (int)c->getRGBPixelGridDefinition()->getOriginPixel(); }, [](RGBPixelGridComponent* c, int v)
			{ c->getRGBPixelGridDefinition()->setOriginPixel((eDMXPixelGridOrigin)v); })
		.addProperty(
			"zigZag", [](RGBPixelGridComponent* c) -> bool { return c->getRGBPixelGridDefinition()->getZigZag(); },
			[](RGBPixelGridComponent* c, bool v) { c->getRGBPixelGridDefinition()->setZigZag(v); })
		.addFunction(
			"setPixel", [](RGBPixelGridComponent* c, int col, int row, int r, int g, int b)
			{ c->setPixel(col, row, static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)); })
		.addFunction("getPixelWireIndex", [](RGBPixelGridComponent* c, int col, int row) -> int
					 { return c->getRGBPixelGridDefinition()->getPixelWireIndex(col, row); })
		.addFunction("fillPixels", [](RGBPixelGridComponent* c, int r, int g, int b)
					 { c->fillPixels(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)); })
		.endClass();
}

#pragma once

#include "ColliderQuery.h"
#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "DMXFixtureComponent.h"
#include "LightSystemFwd.h"
#include "MikanLightTypes.h"
#include "MikanRendererFwd.h"

#include "glm/ext/vector_float3.hpp"

#include <cstdint>
#include <string>
#include <vector>

// The corner the wiring starts at, which also fixes both scan directions:
// an upper corner scans rows top to bottom, a left corner scans columns left
// to right, and each opposite corner reverses that axis.
enum class eDMXPixelGridOrigin : int
{
	INVALID= -1,

	upperLeft= 0,
	upperRight,
	lowerLeft,
	lowerRight,

	COUNT
};
extern const std::string* k_pixelGridOriginStrings;

// -- RGBPixelGridDefinition -----
class RGBPixelGridDefinition : public DMXFixtureComponentDefinition
{
public:
	RGBPixelGridDefinition();
	RGBPixelGridDefinition(MikanLightID gridId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_gridColumnsPropertyId;
	int getColumns() const { return m_columns; }
	void setColumns(int columns);

	static const std::string k_gridRowsPropertyId;
	int getRows() const { return m_rows; }
	void setRows(int rows);

	void resizeGrid(int columns, int rows);

	int getPixelCount() const { return m_columns * m_rows; }
	int getTotalChannels() const { return m_columns * m_rows * 3; }

	// -- Physical layout --
	// The size of one pixel's box. Z is the panel's depth.
	static const std::string k_pixelSizeMMPropertyId;
	const MikanVector3f& getPixelSizeMM() const { return m_pixelSizeMM; }
	void setPixelSizeMM(const MikanVector3f& sizeMM);

	// Center to center spacing between neighboring pixels
	static const std::string k_pixelSeparationMMPropertyId;
	const MikanVector2f& getPixelSeparationMM() const { return m_pixelSeparationMM; }
	void setPixelSeparationMM(const MikanVector2f& separationMM);

	static const std::string k_originPixelPropertyId;
	eDMXPixelGridOrigin getOriginPixel() const { return m_originPixel; }
	void setOriginPixel(eDMXPixelGridOrigin originPixel);

	// LED strips wire to the nearest pixel on the next row, so alternating
	// rows run backwards
	static const std::string k_zigZagPropertyId;
	bool getZigZag() const { return m_bZigZag; }
	void setZigZag(bool bZigZag);

	// -- Layout math --
	// Where a grid cell falls in the DMX stream, or -1 when out of range.
	// Upper left with no zig-zag is row * columns + col, the mapping that was
	// hardcoded before the layout properties existed.
	int getPixelWireIndex(int col, int row) const;
	// The grid cell a stream position addresses; false when out of range
	bool getPixelGridPosition(int wireIndex, int& outCol, int& outRow) const;
	// The pixel's center in component local space (meters), columns along +X
	// and rows along -Y, with the whole grid centered on the component origin
	glm::vec3 getPixelLocalCenter(int col, int row) const;
	// Half the extent of the whole grid including the outer pixels' own size (meters)
	glm::vec3 getGridLocalHalfExtents() const;

private:
	int m_columns= 8;
	int m_rows= 8;
	MikanVector3f m_pixelSizeMM= {30.f, 30.f, 10.f};
	MikanVector2f m_pixelSeparationMM= {40.f, 40.f};
	eDMXPixelGridOrigin m_originPixel= eDMXPixelGridOrigin::upperLeft;
	bool m_bZigZag= false;
};

// -- RGBPixelGridComponent -----
class RGBPixelGridComponent : public DMXFixtureComponent
{
public:
	RGBPixelGridComponent(MikanObjectWeakPtr owner);

	virtual void init() override;
	virtual void dispose() override;
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;

	inline static const std::string k_componentClassName= "RGBPixelGridComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline RGBPixelGridDefinitionConstPtr getRGBPixelGridDefinitionConst() const
	{
		return std::static_pointer_cast<const RGBPixelGridDefinition>(m_definition);
	}
	inline RGBPixelGridDefinitionPtr getRGBPixelGridDefinition() const
	{
		return std::static_pointer_cast<RGBPixelGridDefinition>(m_definition);
	}

	/// Read-only view of the flat R,G,B pixel data, in DMX wire order.
	const std::vector<uint8_t>& getPixelData() const { return m_pixelData; }

	/// Set a single pixel by grid position, routed through the layout mapping.
	void setPixel(int col, int row, uint8_t r, uint8_t g, uint8_t b);

	/// Read a single pixel by grid position; false when out of range.
	bool getPixel(int col, int row, uint8_t& outR, uint8_t& outG, uint8_t& outB) const;

	/// Set all pixels at once, in wire order.
	void setAllPixels(const uint8_t* rgbData, int count);

	/// Set all pixels to the same color.
	void fillPixels(uint8_t r, uint8_t g, uint8_t b);

	/// Size the selection box to the whole grid
	void rebuildCollisionVolume();

	// Selection event handlers
	void onInteractionRayOverlapEnter(const ColliderRaycastHitResult& hitResult);
	void onInteractionRayOverlapExit(const ColliderRaycastHitResult& hitResult);
	void onInteractionSelected();
	void onInteractionUnselected();

	// -- Channel data --
	virtual void getChannelValues(std::vector<uint8_t>& outValues) const override;
	// Replaces the pixel buffer and sends it; setPixel and fillPixels do not send
	virtual void setChannelValues(const std::vector<uint8_t>& values) override;
	// Broadcast pixel data to DMX Listeners
	virtual void sendDMXData() const override;

	// -- IEntityAccessor --
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface --
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface --
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		DMXFixtureComponent::getFunctionDescriptors(outDescriptors);
	}

	// -- Lua Binding --
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;
	void resizePixelDataBuffer();
	// One unit cube drawn once per pixel with a per-pixel color
	void rebuildPixelMesh();

protected:
	std::vector<uint8_t> m_pixelData; // flat R,G,B triples, DMX wire order; size = columns * rows * 3
	SelectionComponentWeakPtr m_selectionComponent;
	BoxColliderComponentWeakPtr m_boxCollider;
	IMkTriangulatedMeshPtr m_pixelMesh;
	bool m_bIsHovered= false;
	bool m_bIsSelected= false;
};

#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "MarkerComponent.h"
#include "MikanObjectSystem.h"
#include "MikanTypedObjectSystemDefinition.h"
#include "MikanTypedObjectSystem.h"
#include "MikanTypeFwd.h"
#include "MulticastDelegate.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "ProjectConfigConstants.h"

#include <map>
#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <glm/ext/matrix_float4x4.hpp>

class MarkerObjectSystemDefinition
	: public MikanTypedObjectSystemDefinition<MarkerComponent, MarkerDefinition, MikanMarkerID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<MarkerComponent, MarkerDefinition, MikanMarkerID>;

	MarkerObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	static const std::string k_arucoDictionaryTypePropertyId;
	inline eCharucoDictionaryType getArucoDictionaryType() const { return m_arucoDictionaryType; }
	void setArucoDictionaryType(eCharucoDictionaryType dictionaryType);

	static const std::string k_arucoIdListPropertyId;
	void getArucoIdList(std::vector<int>& outMarkerIdList) const;

	// ChArUco Settings
	static const std::string k_charucoRowsPropertyId;
	inline int getCharucoRows() const { return m_charucoRows; }
	void setCharucoRows(int charucoRows);

	static const std::string k_charucoColsPropertyId;
	inline int getCharucoCols() const { return m_charucoCols; }
	void setCharucoCols(int charucoCols);

	static const std::string k_charucoSquareLengthMMPropertyId;
	inline float getCharucoSquareLengthMM() const { return m_charucoSquareLengthMM; }
	void setCharucoSquareLengthMM(float charucoSquareLengthMM);

	static const std::string k_charucoMarkerLengthMMPropertyId;
	inline float getCharucoMarkerLengthMM() const { return m_charucoMarkerLengthMM; }
	void setCharucoMarkerLengthMM(float charucoMarkerLengthMM);

	static const std::string k_charucoDictionaryTypePropertyId;
	inline eCharucoDictionaryType getCharucoDictionaryType() const { return m_charucoDictionaryType; }
	void setCharucoDictionaryType(eCharucoDictionaryType charucoDictionaryType);

protected:
	// ArUco Common settings
	eCharucoDictionaryType m_arucoDictionaryType= DEFAULT_ARUCO_DICTIONARY_TYPE;

	// ChAruco Common settings
	int m_charucoRows= CHARUCO_PATTERN_H;
	int m_charucoCols= CHARUCO_PATTERN_W;
	float m_charucoSquareLengthMM= DEFAULT_CHARUCO_SQUARE_LEN_MM;
	float m_charucoMarkerLengthMM= DEFAULT_CHARUCO_MARKER_LEN_MM;
	eCharucoDictionaryType m_charucoDictionaryType= DEFAULT_CHARUCO_DICTIONARY_TYPE;
};

class MarkerObjectSystem : public MikanTypedObjectSystem<MarkerComponent, MarkerDefinition, MikanMarkerID,
														 MarkerObjectSystem, MarkerObjectSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<MarkerComponent, MarkerDefinition, MikanMarkerID, MarkerObjectSystem,
										MarkerObjectSystemDefinition>;

	MarkerObjectSystem(ProjectManagerPtr ownerObjectSystemManager);

	inline static const std::string k_objectSystemClassName= "MarkerObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline MarkerComponentPtr getMarkerById(MikanMarkerID markerId) const
	{
		return Super::getTypedComponentById(markerId);
	}
	inline MarkerComponentPtr getMarkerByName(const std::string& markerName) const
	{
		return Super::getTypedComponentByName(markerName);
	}

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_printCharucoMarkerFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

protected:
	void printMarker();
};
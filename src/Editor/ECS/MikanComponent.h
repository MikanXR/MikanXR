#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "CommonConfigFwd.h"
#include "IEntityAccessor.h"
#include "IMkGraphicsContext.h"
#include "ObjectFwd.h"
#include "MikanRendererFwd.h"
#include "MulticastDelegate.h"
#include "ObjectSystemConfigFwd.h"
#include "ProjectManager.h"

#include <memory>
#include <typeinfo>

class MikanComponentDefinition : public CommonConfig
{
public:
	MikanComponentDefinition();
	MikanComponentDefinition(int componentId, const std::string& componentName);

	MikanComponentPtr getOwnerComponent() const { return m_ownerComponent.lock(); }
	void setOwnerComponent(MikanComponentPtr ownerComponent) { m_ownerComponent= ownerComponent; }

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams);

	static const std::string k_componentIdPropertyId;
	int getComponentId() const { return m_componentId; }

	static const std::string k_componentNamePropertyId;
	const std::string& getComponentName() const { return m_componentName; }
	void setComponentName(const std::string& stencilName);

protected:
	MikanComponentWeakPtr m_ownerComponent;
	int m_componentId;
	std::string m_componentName;
};

class MikanComponent : public std::enable_shared_from_this<MikanComponent>, public IEntityAccessor
{
public:
	MikanComponent(MikanObjectWeakPtr owner);

	inline bool getWasInitialized() const { return m_bWasInitialized; }
	inline bool getWasDisposed() const { return m_bWasDisposed; }

	virtual void setDefinition(MikanComponentDefinitionPtr config);
	virtual MikanComponentDefinitionPtr getDefinition() const { return m_definition; }

	int getComponentId() const;

	inline static const std::string k_componentClassName= "MikanComponent";
	virtual std::string getComponentClassName() const { return k_componentClassName; }

	void setName(const std::string& name);
	const std::string& getName() const { return m_name; }

	class IEditorWindow* getOwnerEditorWindow() const;
	class IMkGraphicsContext* getGraphicsContext() const;
	MikanObjectPtr getOwnerObject() const { return m_ownerObject.lock(); }
	ProjectManagerPtr getOwnerProjectManager() const;
	bool destroyOwnerObject();

	template <class t_object_system_type>
	std::shared_ptr<t_object_system_type> getObjectSystemOfType() const
	{
		return getOwnerProjectManager()->getSystemOfType<t_object_system_type>();
	}

	template <class t_derived_type>
	std::shared_ptr<t_derived_type> getSelfPtr()
	{
		return std::dynamic_pointer_cast<t_derived_type>(shared_from_this());
	}

	template <class t_derived_type>
	std::shared_ptr<t_derived_type> getSelfPtr() const
	{
		return std::dynamic_pointer_cast<t_derived_type>(shared_from_this());
	}

	template <class t_derived_type>
	std::weak_ptr<t_derived_type> getSelfWeakPtr()
	{
		return getSelfPtr<t_derived_type>();
	}

	template <class t_derived_type>
	std::weak_ptr<t_derived_type> getSelfWeakPtr() const
	{
		return getSelfPtr<t_derived_type>();
	}

	virtual void init();
	virtual void postInit();
	virtual void dispose();

	// set m_bWantsUpdate to true in constructor to make this function be called
	virtual void update(float deltaSeconds) {}

	// Invoked manually from MikanTypedObjectSystem
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const {}

	// -- IEntityAccessor ----
	virtual std::string makePropertyUIIdentifier(const std::string& propName) const override;
	virtual CommonConfigPtr getEntityConfig() override { return m_definition; }
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);

protected:
	bool m_bWasInitialized= false;
	bool m_bWasDisposed= false;
	bool m_bWantsUpdate= false;
	std::string m_name;
	MikanObjectWeakPtr m_ownerObject;
	MikanComponentDefinitionPtr m_definition;
};

template <class t_derived_type>
std::shared_ptr<t_derived_type> ComponentCast(MikanComponentPtr component)
{
	return std::dynamic_pointer_cast<t_derived_type>(component);
}

template <class t_component_type>
const char* ComponentTypeName(MikanComponentPtr component)
{
	return typeid(*component.get()).name();
}

template <class t_component_type>
const char* ComponentTypeName()
{
	return typeid(t_component_type).name();
}

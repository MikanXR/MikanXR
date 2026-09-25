#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "IEntityAccessor.h"
#include "MulticastDelegate.h"
#include "ObjectFwd.h"
#include "ObjectSystemFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ProjectManager.h"

#include <vector>

using MikanObjectList= std::vector<MikanObjectPtr>;
using MikanPropertyDatabasePtr= std::shared_ptr<class MikanPropertyDatabase>;
using MikanFunctionDatabasePtr= std::shared_ptr<class MikanFunctionDatabase>;

class MikanObjectSystemDefinition : public CommonConfig
{
public:
	MikanObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
		: CommonConfig(configName)
		, m_idAllocator(idAllocator)
	{
	}

	MikanObjectSystemPtr getOwnerSystem() const { return m_ownerSystem.lock(); }
	void setOwnerSystem(MikanObjectSystemPtr ownerSystem) { m_ownerSystem= ownerSystem; }

protected:
	MikanObjectSystemWeakPtr m_ownerSystem;
	IEntityIDAllocatorWeakPtr m_idAllocator;
};

class MikanObjectSystem : public std::enable_shared_from_this<MikanObjectSystem>, public IEntityAccessor
{
public:
	MikanObjectSystem(ProjectManagerPtr ownerObjectSystem);
	virtual ~MikanObjectSystem();

	virtual bool init(MikanObjectSystemDefinitionPtr definitionPtr);
	virtual void postInit();
	virtual void dispose();
	virtual void update(float deltaSeconds);
	virtual bool isLoading() const { return false; }

	inline static const std::string k_objectSystemClassName= "MikanObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	inline ProjectManagerPtr getOwnerProjectManager() const { return m_ownerObjectSystemManager.lock(); }
	ProjectConfigPtr getProjectConfig() const;
	class IEditorWindow* getOwnerWindow() const;
	class IMkGraphicsContext* getGraphicsContext() const;

	template <class t_object_system_type>
	std::shared_ptr<t_object_system_type> getObjectSystemOfType() const
	{
		return getOwnerProjectManager()->getSystemOfType<t_object_system_type>();
	}

	inline MikanObjectSystemDefinitionConstPtr getDefinitionConst() const { return m_definitionWeakPtr.lock(); }
	inline MikanObjectSystemDefinitionPtr getDefinition() { return m_definitionWeakPtr.lock(); }

	using VisitFunction= std::function<void(MikanObjectPtr)>;
	void visitAllObjects(VisitFunction visitFunc) const
	{
		for (const auto& objectPtr : m_objects)
		{
			if (objectPtr)
			{
				visitFunc(objectPtr);
			}
		}
	}

	virtual MikanComponentPtr getComponentById(int componentId) const= 0;
	virtual bool getComponentList(const std::string& componentClassName,
								  std::vector<MikanComponentPtr>& outComponentList) const= 0;
	virtual bool getComponentIdList(const std::string& componentClassName,
									std::vector<int>& outComponentIdList) const= 0;

	MikanObjectPtr newEmptyObject();
	virtual MikanComponentPtr addNewObjectByUntypedDefinition(const std::string& primaryComponentClass,
															  Serialization::PolymorphicObjectPtr initParams);

	// Create an object of the system's primary component type with a default
	// initialized definition. Returns null when the class does not match.
	virtual MikanComponentPtr addNewObjectWithDefaultDefinition(const std::string& primaryComponentClass)
	{
		return MikanComponentPtr();
	}

	// Recreate an object from a saved definition JSON under its original
	// component id (undo of a destroy). Returns null when the id is still
	// live or the system has no typed pool.
	virtual MikanComponentPtr recreateObjectFromDefinitionJson(const configuru::Config& definitionConfig)
	{
		return MikanComponentPtr();
	}

	virtual bool deleteObject(MikanObjectPtr objectPtr);
	void deleteAllObjects();
	inline const MikanObjectList& getObjectList() const { return m_objects; }

	MulticastDelegate<void(MikanObjectSystemPtr, MikanObjectPtr)> OnNewObjectFinalized;
	// Fired before teardown begins, while the primary component's definition
	// still holds its pre-destroy state (dispose rewrites parenting)
	MulticastDelegate<void(MikanObjectSystemPtr, MikanComponentPtr)> OnObjectWillBeDestroyed;
	MulticastDelegate<void(MikanObjectSystemPtr, MikanObjectConstPtr)> OnObjectDisposed;
	MulticastDelegate<void(MikanObjectSystemPtr, MikanComponentPtr)> OnComponentInitialized;
	MulticastDelegate<void(MikanObjectSystemPtr, MikanComponentConstPtr)> OnComponentDisposed;

	// -- IEntityAccessor ----
	virtual std::string makePropertyUIIdentifier(const std::string& propName) const override;
	virtual CommonConfigPtr getEntityConfig() override { return getDefinition(); }
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const;

	// -- IPropertyInterface ----
	virtual void registerPropertyDescriptors(MikanPropertyDatabasePtr propertyDatabase);
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors) {}
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override
	{
		return false;
	}
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override
	{
		return false;
	}

	// -- IFunctionInterface ----
	virtual void registerFunctionDescriptors(MikanFunctionDatabasePtr functionDatabase);
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors) {}
	virtual bool invokeFunction(const std::string& functionName) override { return false; }

protected:
	bool disposeObjectInternal(MikanObjectPtr objectPtr);

protected:
	ProjectManagerWeakPtr m_ownerObjectSystemManager;
	MikanObjectSystemDefinitionWeakPtr m_definitionWeakPtr;

	MikanObjectList m_objects;

	MulticastDelegate<void(float deltaSeconds)> onUpdate;
	MulticastDelegate<void()> onCustomRender;

	friend class MikanComponent;
};
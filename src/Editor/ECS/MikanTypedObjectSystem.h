#pragma once

#include "ComponentNaming.h"
#include "MikanObjectSystem.h"
#include "MikanTypedComponentPool.h"
#include "MikanTypedObjectSystemDefinition.h"
#include "MikanPropertyDatabase.h"
#include "MikanFunctionDatabase.h"
#include "TransformComponent.h"

#include "Logger.h"

#include <chrono>
#include <memory>
#include <string>

// Template base class for object systems that manage a single pool of components
// TComponent: The component type (e.g., SceneComponent)
// TDefinition: The component definition type (e.g., SceneComponentDefinition)
// TID: The ID type used to identify components (e.g., MikanSceneID)
// TSystem: The system type (e.g., SceneObjectSystem)
// TSystemDefinition: The system definition type (e.g., SceneObjectSystemDefinition)
template <class TComponent, class TDefinition, typename TID, class TSystem, class TSystemDefinition>
class MikanTypedObjectSystem : public MikanObjectSystem
{
public:
	using ComponentPtr= std::shared_ptr<TComponent>;
	using ComponentConstPtr= std::shared_ptr<const TComponent>;
	using ComponentDefinitionPtr= std::shared_ptr<TDefinition>;
	using SystemDefinitionPtr= std::shared_ptr<TSystemDefinition>;
	using SystemDefinitionConstPtr= std::shared_ptr<const TSystemDefinition>;
	using Pool= MikanTypedComponentPool<TComponent, TDefinition, TID>;
	using DefinitionInitFunction= std::function<bool(ComponentDefinitionPtr)>;

	MikanTypedObjectSystem(ProjectManagerPtr ownerObjectSystem)
		: MikanObjectSystem(ownerObjectSystem)
		, m_componentPool(this, [this](auto def) { return this->objectFactory(def); })
	{
	}

	// -- MikanObjectSystem ----
	virtual bool init(MikanObjectSystemDefinitionPtr definitionPtr) override
	{
		MikanObjectSystem::init(definitionPtr);

		// Create pool objects from definitions
		SystemDefinitionConstPtr systemDefinition= getTypedDefinitionConst();
		m_componentPool.initializeFromDefinitions(systemDefinition->getAllDefinitions());

		// Mark the system as initialized
		m_bIsInitialzed= true;

		return true;
	}

	virtual void dispose() override
	{
		// Dispose all objects in the system first, which notifies components of deletion
		MikanObjectSystem::dispose();

		// Dispose all components in the pool
		m_componentPool.disposeAll();

		// Clear the initialized flag
		m_bIsInitialzed= false;
	}

	virtual void registerPropertyDescriptors(MikanPropertyDatabasePtr propertyDatabase) override
	{
		propertyDatabase->template registerPropertiesForSystem<TSystem>();
		propertyDatabase->template registerPropertiesForComponent<TSystem, TComponent>();
	}

	virtual void registerFunctionDescriptors(MikanFunctionDatabasePtr functionDatabase) override
	{
		functionDatabase->template registerFunctionsForSystem<TSystem>();
		functionDatabase->template registerFunctionsForComponent<TSystem, TComponent>();
	}

	virtual bool deleteObject(MikanObjectPtr objectPtr) override
	{
		auto primaryComponent= objectPtr->getComponentOfType<TComponent>();
		if (primaryComponent)
		{
			return removeObjectByPrimaryComponentId(primaryComponent->getComponentId());
		}

		return false;
	}

	// System Definition Accessors
	std::shared_ptr<const TSystemDefinition> getTypedDefinitionConst() const
	{
		return std::static_pointer_cast<const TSystemDefinition>(getDefinitionConst());
	}

	std::shared_ptr<TSystemDefinition> getTypedDefinition()
	{
		return std::static_pointer_cast<TSystemDefinition>(getDefinition());
	}

	// Component Pool Accessors
	virtual MikanComponentPtr getComponentById(int componentId) const override
	{
		return std::static_pointer_cast<MikanComponent>(m_componentPool.getById(static_cast<TID>(componentId)));
	}

	virtual bool getComponentList(const std::string& componentClassName,
								  std::vector<MikanComponentPtr>& outComponentList) const
	{
		if (componentClassName == TComponent::k_componentClassName)
		{
			getTypedComponentList(outComponentList);
			return true;
		}
		return false;
	}

	virtual bool getComponentIdList(const std::string& componentClassName, std::vector<int>& outComponentIdList) const
	{
		if (componentClassName == TComponent::k_componentClassName)
		{
			getTypedComponentIdList(outComponentIdList);
			return true;
		}
		return false;
	}

	void getTypedComponentList(std::vector<MikanComponentPtr>& outComponentList) const
	{
		const typename Pool::ComponentMap& componentMap= m_componentPool.getAll();
		for (const auto& kvpair : componentMap)
		{
			outComponentList.push_back(kvpair.second.lock());
		}
	}

	void getTypedComponentIdList(std::vector<int>& outComponentIdList) const
	{
		const typename Pool::ComponentMap& componentMap= m_componentPool.getAll();
		for (const auto& kvpair : componentMap)
		{
			outComponentIdList.push_back(static_cast<int>(kvpair.first));
		}
	}

	ComponentPtr getTypedComponentById(TID id) const { return m_componentPool.getById(id); }

	ComponentPtr getTypedComponentByName(const std::string& name) const { return m_componentPool.getByName(name); }

	TID getFirstComponentId() const { return m_componentPool.getFirstComponentId(); }

	using PredFunction= std::function<bool(ComponentConstPtr)>;
	ComponentPtr getTypedComponentByPredicate(PredFunction predicate) const
	{
		return m_componentPool.findByPredicate(predicate);
	}

	const typename Pool::ComponentMap& getComponentMap() const { return m_componentPool.getAll(); }

	using VisitFunction= std::function<void(ComponentPtr)>;
	using FilterFunction= std::function<bool(ComponentPtr)>;
	void visitComponents(VisitFunction visitFunc, FilterFunction filterFunc= {}) const
	{
		for (const auto& kvpair : getComponentMap())
		{
			ComponentPtr componentPtr= kvpair.second.lock();

			if (componentPtr && (!filterFunc || filterFunc(componentPtr)))
			{
				visitFunc(componentPtr);
			}
		}
	}

	// Component Pool Mutators
	virtual MikanComponentPtr addNewObjectByUntypedDefinition(const std::string& primaryComponentClass,
															  Serialization::PolymorphicObjectPtr initParams) override
	{
		// Only support creating objects from definitions that match the primary component type of this system
		if (primaryComponentClass == TComponent::k_componentClassName)
		{
			MikanObjectSystem* ownerObjectSystem= this;

			return addNewObjectByTypedDefinition(
				[ownerObjectSystem, &initParams](ComponentDefinitionPtr definition)
				{
					// Initialize the definition from the polymorphic object init params
					return definition->readFromInitParams(ownerObjectSystem, initParams);
				});
		}
		return MikanComponentPtr();
	}

	virtual MikanComponentPtr addNewObjectWithDefaultDefinition(const std::string& primaryComponentClass) override
	{
		if (primaryComponentClass == TComponent::k_componentClassName)
		{
			return addNewObjectByTypedDefinition();
		}
		return MikanComponentPtr();
	}

	virtual MikanComponentPtr recreateObjectFromDefinitionJson(const configuru::Config& definitionConfig) override
	{
		SystemDefinitionPtr systemDefinition= getTypedDefinition();
		assert(systemDefinition);

		// Always a fresh definition instance built from the JSON, carrying
		// its original component id
		ComponentDefinitionPtr componentDefinition= systemDefinition->allocateDefinitionFromJSON(definitionConfig);
		if (!componentDefinition)
			return MikanComponentPtr();

		const TID componentId= (TID)componentDefinition->getComponentId();
		if (getComponentById(componentId) != nullptr || systemDefinition->getDefinitionById(componentId) != nullptr)
		{
			// The id is still live; refusing beats silently leaking a duplicate
			return MikanComponentPtr();
		}

		// Runs the full object factory: init, postInit (which reattaches by
		// parent_transform_id), and addDefinition (which fires the pool change)
		ComponentPtr component= createObjectFromDefinition(componentDefinition);
		if (component)
		{
			MikanObjectPtr objectPtr= component->getOwnerObject();
			assert(objectPtr);

			if (OnNewObjectFinalized)
				OnNewObjectFinalized(shared_from_this(), objectPtr);
		}

		return component;
	}

	ComponentPtr addNewObjectByTypedDefinition(DefinitionInitFunction definitionInit= {})
	{
		SystemDefinitionPtr systemDefinition= getTypedDefinition();

		// Allocate new component definition (doesn't add to pool yet)
		ComponentDefinitionPtr componentDefinition= systemDefinition->allocateNewDefinition();
		componentDefinition->setComponentName(
			resolveDefaultComponentName(TComponent::k_componentClassName, componentDefinition->getComponentId()));

		// Allow caller to initialize definition before creating object
		if (definitionInit)
		{
			if (!definitionInit(componentDefinition))
			{
				// If definition initialization fails, deallocate the definition and return null
				systemDefinition->removeDefinition(componentDefinition->getComponentId());

				return ComponentPtr();
			}
		}

		// Create the component object using the pool (will add definition to pool after object is built)
		ComponentPtr component= m_componentPool.create(componentDefinition);

		// Notify external listeners that the newly created object is fully setup
		if (component)
		{
			MikanObjectPtr objectPtr= component->getOwnerObject();
			assert(objectPtr);

			if (OnNewObjectFinalized)
				OnNewObjectFinalized(shared_from_this(), objectPtr);
		}

		return component;
	}

	bool removeObjectByPrimaryComponentId(TID componentId)
	{
		ComponentPtr component= getTypedComponentById(componentId);
		if (component)
		{
			SystemDefinitionPtr systemDefinition= getTypedDefinition();
			assert(systemDefinition);
			MikanObjectPtr objectPtr= component->getOwnerObject();
			assert(objectPtr);

			// Notify listeners while the definition still holds pre-destroy
			// state (dispose rewrites parenting on it and its children)
			if (OnObjectWillBeDestroyed)
				OnObjectWillBeDestroyed(shared_from_this(), component);

			return
				// 1. Remove the object from the system first (fires object/component disposed events)
				MikanObjectSystem::disposeObjectInternal(objectPtr) &&
				// 2. Remove the component reference from the typed component pool
				m_componentPool.dispose(componentId) &&
				// 3. Remove the component definition from the system definition
				// (fires system property change event, which is now safe to do now that the object is cleaned up)
				systemDefinition->removeDefinition(componentId);
		}

		return false;
	}

	// -- Rendering -----
	void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera, FilterFunction filterFunc= {})
	{
		visitComponents([graphicsContext, viewportCamera](ComponentPtr component)
						{ component->customRender(graphicsContext, viewportCamera); }, filterFunc);
	}

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
	{
		MikanObjectSystem::getPropertyDescriptors(outDescriptors);

		outDescriptors.push_back(
			std::make_shared<PropertyDescriptor>(TSystemDefinition::k_componentIdListPropertyId,
												 MikanVariantType::INT_ARRAY)
				->setReadOnly()
				->setUIHidden()
				->setClientAPIHidden()); // Exposed via GetComponentListRequest, not PropertyGetValueRequest
	}
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override
	{
		if (propertyName == TSystemDefinition::k_componentIdListPropertyId)
		{
			std::vector<int> componentIdList;
			getTypedDefinitionConst()->getAllComponentIds(componentIdList);
			outValue= componentIdList;
			return true;
		}

		return MikanObjectSystem::getPropertyValue(propertyName, outValue);
	}

protected:
	ComponentPtr createObjectFromDefinition(ComponentDefinitionPtr componentDefinition)
	{
		// Create the scene object using the pool (will add definition to pool after object is built)
		return m_componentPool.create(componentDefinition);
	}

	virtual void additionalComponentFactory(MikanObjectPtr ownerComponentObject,
											ComponentDefinitionPtr componentDefinition)
	{
		// override in derived classes to add additional components to the object
	}

private:
	ComponentPtr objectFactory(ComponentDefinitionPtr componentDefinition)
	{
		SystemDefinitionPtr systemDefinition= getTypedDefinition();
		const std::string systemName= getObjectSystemClassName();
		const std::string compName= componentDefinition->getComponentName();

#define MIKAN_FACTORY_TIMED(label, expr)                                                                               \
	do                                                                                                                 \
	{                                                                                                                  \
		auto _t0= std::chrono::high_resolution_clock::now();                                                           \
		expr;                                                                                                          \
		auto _ms=                                                                                                      \
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _t0)     \
				.count();                                                                                              \
		if (_ms >= 10)                                                                                                 \
		{                                                                                                              \
			MIKAN_LOG_INFO("MikanTypedObjectSystem::objectFactory")                                                    \
				<< systemName << "[" << compName << "] " label ": " << _ms << "ms";                                    \
		}                                                                                                              \
	} while (0)

		MikanObjectPtr mikanObject= newEmptyObject();
		mikanObject->setName(compName);

		// Add the primary component to the object
		ComponentPtr componentPtr= mikanObject->template addComponent<TComponent>();
		componentPtr->setDefinition(componentDefinition);

		// If this is a TransformComponent, set it as the root component
		auto rootComponent= std::dynamic_pointer_cast<TransformComponent>(componentPtr);
		if (rootComponent)
		{
			mikanObject->setRootComponent(rootComponent);
		}

		// Allow derived systems to add additional components
		MIKAN_FACTORY_TIMED("additionalComponentFactory", additionalComponentFactory(mikanObject, componentDefinition));

		// Init the object once all components are added
		MIKAN_FACTORY_TIMED("init", mikanObject->init());

		// If the system is already initialized then run post-init right away
		if (m_bIsInitialzed)
		{
			MIKAN_FACTORY_TIMED("postInit", mikanObject->postInit());
		}

		// Add definition to pool (fires property change event now that object is fully built)
		MIKAN_FACTORY_TIMED("addDefinition", systemDefinition->addDefinition(componentDefinition));

#undef MIKAN_FACTORY_TIMED

		return componentPtr;
	}

private:
	bool m_bIsInitialzed= false;
	Pool m_componentPool;
};
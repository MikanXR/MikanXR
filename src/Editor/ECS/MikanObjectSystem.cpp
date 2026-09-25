#include "MikanObjectSystem.h"
#include "MikanObject.h"
#include "MikanPropertyDatabase.h"
#include "MikanFunctionDatabase.h"
#include "MikanVariantTypes.h"
#include "MikanPropertyTypes.h"
#include "IEditorWindow.h"
#include "StringUtils.h"

#include "assert.h"

MikanObjectSystem::MikanObjectSystem(ProjectManagerPtr ownerObjectSystem)
	: m_ownerObjectSystemManager(ownerObjectSystem)
{
}

MikanObjectSystem::~MikanObjectSystem() { assert(m_objects.empty()); }

bool MikanObjectSystem::init(MikanObjectSystemDefinitionPtr definitionPtr)
{
	m_definitionWeakPtr= definitionPtr;
	if (definitionPtr)
	{
		definitionPtr->setOwnerSystem(shared_from_this());
	}

	return true;
}

void MikanObjectSystem::postInit()
{
	for (MikanObjectPtr objectPtr : m_objects)
	{
		objectPtr->postInit();
	}
}

void MikanObjectSystem::dispose()
{
	deleteAllObjects();

	if (onDisposed)
	{
		onDisposed(this);
	}
}

void MikanObjectSystem::update(float deltaSeconds)
{
	if (onUpdate)
		onUpdate(deltaSeconds);
}

ProjectConfigPtr MikanObjectSystem::getProjectConfig() const { return getOwnerProjectManager()->getProjectConfig(); }

IEditorWindow* MikanObjectSystem::getOwnerWindow() const { return getOwnerProjectManager()->getOwnerWindow(); }

IMkGraphicsContext* MikanObjectSystem::getGraphicsContext() const
{
	IEditorWindow* ownerWindow= getOwnerWindow();
	if (ownerWindow)
	{
		return ownerWindow->getGraphicsContext().get();
	}

	return nullptr;
}

MikanObjectPtr MikanObjectSystem::newEmptyObject()
{
	MikanObjectPtr objectPtr= std::make_shared<MikanObject>(shared_from_this());
	m_objects.push_back(objectPtr);

	return objectPtr;
}

MikanComponentPtr MikanObjectSystem::addNewObjectByUntypedDefinition(const std::string& primaryComponentClass,
																	 Serialization::PolymorphicObjectPtr initParams)
{
	return MikanComponentPtr();
}

bool MikanObjectSystem::deleteObject(MikanObjectPtr objectPtr) { return disposeObjectInternal(objectPtr); }

bool MikanObjectSystem::disposeObjectInternal(MikanObjectPtr objectPtr)
{
	// Only tear down this object if it belongs to this system
	if (objectPtr)
	{
		auto it= std::find(m_objects.begin(), m_objects.end(), objectPtr);
		if (it != m_objects.end())
		{
			// 1. Call dispose() on all components
			// 2. Call OnObjectDisposed on owning system
			// 3. Remove all component reference from object
			objectPtr->dispose();

			// 4. Remove object from system list (should deallocate it if no other references)
			m_objects.erase(it);

			return true;
		}
	}

	return false;
}

void MikanObjectSystem::deleteAllObjects()
{
	for (MikanObjectPtr objectPtr : m_objects)
	{
		objectPtr->dispose();
	}
	m_objects.clear();
}

void MikanObjectSystem::registerPropertyDescriptors(MikanPropertyDatabasePtr propertyDatabase)
{
	propertyDatabase->registerPropertiesForSystem<MikanObjectSystem>();
}

void MikanObjectSystem::registerFunctionDescriptors(MikanFunctionDatabasePtr functionDatabase)
{
	functionDatabase->registerFunctionsForSystem<MikanObjectSystem>();
}

// -- IEntityAccessor ----
std::string MikanObjectSystem::makePropertyUIIdentifier(const std::string& propName) const
{
	return StringUtils::stringify(getObjectSystemClassName(), "_", propName);
}

Serialization::StructTypeHandle MikanObjectSystem::getClientAPIValuesStructType() const
{
	return &MikanSystemValues::staticGetArchetype();
}
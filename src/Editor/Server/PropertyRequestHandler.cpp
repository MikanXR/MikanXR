#include "App.h"
#include "AppStage.h"
#include "MainWindow.h"
#include "MikanComponent.h"
#include "MikanClientConnectionState.h"
#include "MikanObject.h"
#include "MikanObjectSystem.h"
#include "MikanServer.h"
#include "MikanPropertyDatabase.h"
#include "PropertyDatabaseEnumerator.h"
#include "PropertyRequestHandler.h"
#include "ProjectConfig.h"
#include "MikanPropertyEvents.h"
#include "MikanPropertyRequests.h"
#include "MulticastDelegate.h"
#include "ServerResponseHelpers.h"
#include "ServerEntitySerializer.h"

#include <functional>

using namespace std::placeholders;

// -- PropertyRequestHandler -- //
bool PropertyRequestHandler::startup(MainWindow* mainWindow)
{
	IInterprocessMessageServer* messageServer= m_owner->getMessageServer();

	// Property Change Callbacks.
	// Bind to the current project config and follow project loads, since
	// loading a project replaces the ProjectConfig instance.
	bindProjectConfig(getProjectConfig());
	ProjectManagerPtr projectManager= getProjectManager();
	projectManager->OnProjectLoaded+= MakeDelegate(this, &PropertyRequestHandler::onProjectLoaded);
	projectManager->OnProjectPreUnload+= MakeDelegate(this, &PropertyRequestHandler::onProjectPreUnload);

	// Property Requests
	messageServer->setRequestHandler(PropertySetValueRequest::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::setPropertyValueHandler, this, _1, _2));
	messageServer->setRequestHandler(PropertyGetValueRequest::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::getPropertyValueHandler, this, _1, _2));
	messageServer->setRequestHandler(SetPropertyNotifyMode::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::setPropertyNotifyModeHandler, this, _1, _2));
	messageServer->setRequestHandler(GetPropertyDescriptors::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::getPropertyDescriptorsHandler, this, _1, _2));
	messageServer->setRequestHandler(ComponentGetValuesRequest::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::getComponentValuesHandler, this, _1, _2));
	messageServer->setRequestHandler(GetComponentListRequest::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::getComponentListHandler, this, _1, _2));
	messageServer->setRequestHandler(SystemGetValuesRequest::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::getSystemValuesHandler, this, _1, _2));
	messageServer->setRequestHandler(SystemCreateObjectRequest::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::createSystemObjectHandler, this, _1, _2));
	messageServer->setRequestHandler(SystemDestroyObjectRequest::staticGetArchetype().getName(),
									 std::bind(&PropertyRequestHandler::destroySystemObjectHandler, this, _1, _2));

	return true;
}

void PropertyRequestHandler::shutdown()
{
	unbindProjectConfig();

	ProjectManagerPtr projectManager= getProjectManager();
	if (projectManager)
	{
		projectManager->OnProjectLoaded-= MakeDelegate(this, &PropertyRequestHandler::onProjectLoaded);
		projectManager->OnProjectPreUnload-= MakeDelegate(this, &PropertyRequestHandler::onProjectPreUnload);
	}
}

void PropertyRequestHandler::onProjectLoaded(ProjectManagerPtr projectManager)
{
	bindProjectConfig(projectManager->getProjectConfig());
}

void PropertyRequestHandler::onProjectPreUnload(ProjectManagerPtr projectManager) { unbindProjectConfig(); }

void PropertyRequestHandler::bindProjectConfig(ProjectConfigPtr projectConfig)
{
	if (projectConfig)
	{
		projectConfig->OnPropertyChanged+= MakeDelegate(this, &PropertyRequestHandler::onProjectConfigChanged);
		m_projectConfig= projectConfig;
	}
}

void PropertyRequestHandler::unbindProjectConfig()
{
	ProjectConfigPtr projectConfig= m_projectConfig.lock();
	if (projectConfig)
	{
		projectConfig->OnPropertyChanged-= MakeDelegate(this, &PropertyRequestHandler::onProjectConfigChanged);
	}
	m_projectConfig.reset();
}

// Project Config Change Callbacks
void PropertyRequestHandler::onProjectConfigChanged(CommonConfigPtr configPtr,
													const ConfigPropertyChangeSet& changedPropertySet)
{
	if (auto systemDefinition= std::dynamic_pointer_cast<MikanObjectSystemDefinition>(configPtr))
	{
		for (const std::string& propertyName : changedPropertySet.getSet())
		{
			onObjectSystemDefinitionChanged(systemDefinition, propertyName);
		}
	}
	else if (auto componentDefition= std::dynamic_pointer_cast<MikanComponentDefinition>(configPtr))
	{
		for (const std::string& propertyName : changedPropertySet.getSet())
		{
			onComponentDefinitionChanged(componentDefition, propertyName);
		}
	}
}

void PropertyRequestHandler::onObjectSystemDefinitionChanged(MikanObjectSystemDefinitionPtr systemDefinition,
															 const std::string& propertyName)
{
	const MikanClientConnectionStateMap& clientConnections= m_owner->getConnectedClientStateMap();
	if (clientConnections.empty())
		return;

	MikanObjectSystemPtr ownerSystem= systemDefinition->getOwnerSystem();
	if (!ownerSystem)
		return;

	MikanPropertyValue propertyValue= {};
	propertyValue.ownerSystem.setUtf8Value(ownerSystem->getObjectSystemClassName().c_str());
	propertyValue.componentId= -1; // System properties use -1 for componentId
	propertyValue.fieldName.setUtf8Value(propertyName.c_str());
	if (!ownerSystem->getPropertyValue(propertyName, propertyValue.fieldValue))
		return;

	// Broadcast to all clients (depending on subscription settings)
	for (auto& connection_it : clientConnections)
	{
		connection_it.second->publishPropertyChangedEvent(propertyValue);
	}
}

void PropertyRequestHandler::onComponentDefinitionChanged(MikanComponentDefinitionPtr componentDefinition,
														  const std::string& propertyName)
{
	const MikanClientConnectionStateMap& clientConnections= m_owner->getConnectedClientStateMap();
	if (clientConnections.empty())
		return;

	MikanComponentPtr ownerComponent= componentDefinition->getOwnerComponent();
	if (!ownerComponent)
		return;

	MikanObjectPtr ownerObject= ownerComponent->getOwnerObject();
	if (!ownerObject)
		return;

	MikanObjectSystemPtr ownerSystem= ownerObject->getOwnerSystem();
	if (!ownerSystem)
		return;

	MikanPropertyValue propertyValue= {};
	propertyValue.ownerSystem.setUtf8Value(ownerSystem->getObjectSystemClassName().c_str());
	propertyValue.ownerComponentClass.setUtf8Value(ownerComponent->getComponentClassName().c_str());
	propertyValue.componentId= ownerComponent->getDefinition()->getComponentId();
	propertyValue.fieldName.setUtf8Value(propertyName.c_str());
	if (!ownerComponent->getPropertyValue(propertyName, propertyValue.fieldValue))
		return;

	// Broadcast to all clients (depending on subscription settings)
	for (auto& connection_it : clientConnections)
	{
		connection_it.second->publishPropertyChangedEvent(propertyValue);
	}
}

// Property Request Handlers
void PropertyRequestHandler::setPropertyValueHandler(const ClientRequest& request, ClientResponse& response)
{
	PropertySetValueRequest setValueRequest;
	if (!readTypedRequest(request.utf8RequestString, setValueRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const char* ownerSystemName= setValueRequest.ownerSystem.getUtf8Value();
	MikanObjectSystemPtr objectSystem= getProjectManager()->getSystemByName(ownerSystemName);
	if (!objectSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const char* propertyValueName= setValueRequest.fieldName.getUtf8Value();
	const MikanVariant& variantValue= setValueRequest.fieldValue;

	if (setValueRequest.componentId != -1)
	{
		MikanComponentPtr componentPtr= objectSystem->getComponentById(setValueRequest.componentId);
		if (!componentPtr)
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
			return;
		}

		// Set the property value on the component
		if (!componentPtr->setPropertyValue(propertyValueName, variantValue))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
			return;
		}
	}
	else
	{
		// Set the property value on the system
		if (!objectSystem->setPropertyValue(propertyValueName, variantValue))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
			return;
		}
	}

	// Return a successful response
	PropertySetValueResponse setValueResponse;
	writeTypedJsonResponse(request.requestId, setValueResponse, response);
}

void PropertyRequestHandler::getPropertyValueHandler(const ClientRequest& request, ClientResponse& response)
{
	PropertyGetValueRequest getValueRequest;
	if (!readTypedRequest(request.utf8RequestString, getValueRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const char* ownerSystemName= getValueRequest.ownerSystem.getUtf8Value();
	MikanObjectSystemPtr objectSystem= getProjectManager()->getSystemByName(ownerSystemName);
	if (!objectSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Build the response
	PropertyGetValueResponse getValueResponse= {};
	getValueResponse.propertyValue.ownerSystem= getValueRequest.ownerSystem;
	getValueResponse.propertyValue.componentId= getValueRequest.componentId;
	getValueResponse.propertyValue.fieldName= getValueRequest.fieldName;

	const char* propertyValueName= getValueRequest.fieldName.getUtf8Value();
	if (getValueRequest.componentId != -1)
	{
		MikanComponentPtr componentPtr= objectSystem->getComponentById(getValueRequest.componentId);
		if (!componentPtr)
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
			return;
		}

		// Get the property value from the component
		if (!componentPtr->getPropertyValue(propertyValueName, getValueResponse.propertyValue.fieldValue))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
			return;
		}
	}
	else
	{
		// Get the property value from the system
		if (!objectSystem->getPropertyValue(propertyValueName, getValueResponse.propertyValue.fieldValue))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
			return;
		}
	}

	writeTypedJsonResponse(request.requestId, getValueResponse, response);
}

void PropertyRequestHandler::getComponentValuesHandler(const ClientRequest& request, ClientResponse& response)
{
	ComponentGetValuesRequest componentValuesRequest;
	if (!readTypedRequest(request.utf8RequestString, componentValuesRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const char* ownerSystemName= componentValuesRequest.ownerSystem.getUtf8Value();
	MikanObjectSystemPtr objectSystem= getProjectManager()->getSystemByName(ownerSystemName);
	if (!objectSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	MikanComponentPtr componentPtr= objectSystem->getComponentById(componentValuesRequest.componentId);
	if (!componentPtr)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	Serialization::StructTypeHandle valuesStruct= componentPtr->getClientAPIValuesStructType();
	if (!valuesStruct)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Build the response
	ComponentGetValuesResponse getValuesResponse= {};
	getValuesResponse.ownerSystem= componentValuesRequest.ownerSystem;
	getValuesResponse.componentClassName= componentPtr->getComponentClassName().c_str();

	// Extract the values into the response polymorphic object
	std::string serializeError;
	if (!Serialization::serializeFromEntity(std::static_pointer_cast<IEntityAccessor>(componentPtr),
											getValuesResponse.valuesObject.allocateByType(valuesStruct), valuesStruct,
											serializeError))
	{
		MIKAN_LOG_ERROR("PropertyRequestHandler::getComponentValuesHandler")
			<< "Failed to serialize component values: " << serializeError;
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	writeTypedJsonResponse(request.requestId, getValuesResponse, response);
}

void PropertyRequestHandler::getComponentListHandler(const ClientRequest& request, ClientResponse& response)
{
	GetComponentListRequest getComponentListRequest;
	if (!readTypedRequest(request.utf8RequestString, getComponentListRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const char* ownerSystemName= getComponentListRequest.ownerSystem.getUtf8Value();
	MikanObjectSystemPtr objectSystem= getProjectManager()->getSystemByName(ownerSystemName);
	if (!objectSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Build the response
	ComponentListResponse componentListResponse= {};
	const char* componentClassName= getComponentListRequest.componentClassName.getUtf8Value();
	std::vector<int> componentIdVector;
	if (!objectSystem->getComponentIdList(componentClassName, componentIdVector))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}
	componentListResponse.componentIdList.assign(componentIdVector.data(),
												 componentIdVector.data() + componentIdVector.size());

	writeTypedJsonResponse(request.requestId, componentListResponse, response);
}

void PropertyRequestHandler::getSystemValuesHandler(const ClientRequest& request, ClientResponse& response)
{
	SystemGetValuesRequest systemValuesRequest;
	if (!readTypedRequest(request.utf8RequestString, systemValuesRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const char* ownerSystemName= systemValuesRequest.ownerSystem.getUtf8Value();
	MikanObjectSystemPtr objectSystem= getProjectManager()->getSystemByName(ownerSystemName);
	if (!objectSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	Serialization::StructTypeHandle valuesStruct= objectSystem->getClientAPIValuesStructType();
	if (!valuesStruct)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Build the response
	SystemGetValuesResponse getValuesResponse= {};
	getValuesResponse.ownerSystem= systemValuesRequest.ownerSystem;

	// Extract the values into the response polymorphic object
	std::string serializeError;
	if (!Serialization::serializeFromEntity(std::static_pointer_cast<IEntityAccessor>(objectSystem),
											getValuesResponse.valuesObject.allocateByType(valuesStruct), valuesStruct,
											serializeError))
	{
		MIKAN_LOG_ERROR("PropertyRequestHandler::getSystemValuesHandler")
			<< "Failed to serialize system values: " << serializeError;
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	writeTypedJsonResponse(request.requestId, getValuesResponse, response);
}

void PropertyRequestHandler::createSystemObjectHandler(const ClientRequest& request, ClientResponse& response)
{
	// Deserialize the request parameters
	SystemCreateObjectRequest createObjectRequest;
	if (!readTypedRequest(request.utf8RequestString, createObjectRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Find the object system to create the object in
	const char* ownerSystemName= createObjectRequest.ownerSystem.getUtf8Value();
	MikanObjectSystemPtr objectSystem= getProjectManager()->getSystemByName(ownerSystemName);
	if (!objectSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Attempt to create the object on the specified system
	if (!objectSystem->addNewObjectByUntypedDefinition(createObjectRequest.componentClassName.getUtf8Value(),
													   createObjectRequest.initParams))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Return a successful response
	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

void PropertyRequestHandler::destroySystemObjectHandler(const ClientRequest& request, ClientResponse& response)
{
	// Deserialize the request parameters
	SystemDestroyObjectRequest destroyObjectRequest;
	if (!readTypedRequest(request.utf8RequestString, destroyObjectRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Find the object system to destroy the object from
	const char* ownerSystemName= destroyObjectRequest.ownerSystem.getUtf8Value();
	MikanObjectSystemPtr objectSystem= getProjectManager()->getSystemByName(ownerSystemName);
	if (!objectSystem)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Find the component to destroy
	MikanComponentPtr componentPtr= objectSystem->getComponentById(destroyObjectRequest.componentId);
	if (!componentPtr)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Destroy the owner object that contains the component
	if (!componentPtr->destroyOwnerObject())
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::RequestFailed, response);
		return;
	}

	// Return a successful response
	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

void PropertyRequestHandler::setPropertyNotifyModeHandler(const ClientRequest& request, ClientResponse& response)
{
	SetPropertyNotifyMode setPropertyNotifyMode;
	if (!readTypedRequest(request.utf8RequestString, setPropertyNotifyMode))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	MikanClientConnectionStatePtr clientState= m_owner->getConnectedClientState(request.connectionId);
	if (!clientState)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::UnknownClient, response);
		return;
	}

	if (!clientState->setPropertyNotifyMode(
			setPropertyNotifyMode.systemFilter.getUtf8Value(), setPropertyNotifyMode.componentFilter.getUtf8Value(),
			setPropertyNotifyMode.propertyFilter.getUtf8Value(), setPropertyNotifyMode.notifyMode))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
		return;
	}

	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

void PropertyRequestHandler::getPropertyDescriptorsHandler(const ClientRequest& request, ClientResponse& response)
{
	GetPropertyDescriptors getDescriptorsRequest;
	if (!readTypedRequest(request.utf8RequestString, getDescriptorsRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	PropertyDescriptorResponse propertyDescriptorResponse;

	MikanPropertyDatabaseConstPtr propertyDatabase= getProjectManager()->getPropertyDatabaseConst();
	PropertyDatabaseEnumerator enumerator(propertyDatabase, getDescriptorsRequest.systemFilter.getUtf8Value(),
										  getDescriptorsRequest.componentFilter.getUtf8Value(),
										  getDescriptorsRequest.propertyFilter.getUtf8Value());
	while (enumerator.isValid())
	{
		int propertyIndex= enumerator.getCurrentPropertyIndex();
		const MikanPropertyEntry* propertyEntry= propertyDatabase->getPropertyByIndex(propertyIndex);

		MikanPropertyDescriptor descriptorResult= {};
		descriptorResult.ownerSystemClass.setUtf8Value(propertyEntry->systemName.c_str());
		descriptorResult.ownerComponentClass.setUtf8Value(propertyEntry->componentClassName.c_str());
		descriptorResult.fieldName.setUtf8Value(propertyEntry->descriptor->getName().c_str());
		descriptorResult.fieldType= propertyEntry->descriptor->getDataType();
		descriptorResult.isReadOnly= propertyEntry->descriptor->isReadOnly();

		propertyDescriptorResponse.descriptor_list.push_back(descriptorResult);

		enumerator.next();
	}

	writeTypedJsonResponse(request.requestId, propertyDescriptorResponse, response);
}
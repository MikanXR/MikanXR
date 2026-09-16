#pragma once

#include "MikanTypedObjectSystem.h"
#include "ScriptComponent.h"
#include "ScriptError.h"
#include "ScriptHttpRouteTable.h"
#include "ScriptingFwd.h"

class ScriptObjectSystemDefinition
	: public MikanTypedObjectSystemDefinition<ScriptComponent, ScriptDefinition, MikanScriptID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<ScriptComponent, ScriptDefinition, MikanScriptID>;

	ScriptObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	// The project's HTTP trigger routes. Every edit notifies this one name, so
	// the whole table is the unit the transaction recorder captures.
	static const std::string k_httpRoutesPropertyId;
	inline const ScriptHttpRouteTable& getHttpRoutes() const { return m_httpRoutes; }
	void setHttpRoutes(const ScriptHttpRouteTable& routes);

private:
	ScriptHttpRouteTable m_httpRoutes;
};

// Owns the project's one Lua state. Every script in the pool loads its
// behavior into it in pool order, and any change to the pool or to a script's
// path rebuilds it on the next update, never inside the change that requested
// it. The system also owns the HTTP route table and keeps the request handler's
// routes matching it.
class ScriptObjectSystem : public MikanTypedObjectSystem<ScriptComponent, ScriptDefinition, MikanScriptID,
														 ScriptObjectSystem, ScriptObjectSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<ScriptComponent, ScriptDefinition, MikanScriptID, ScriptObjectSystem,
										ScriptObjectSystemDefinition>;

	ScriptObjectSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "ScriptObjectSystem";
	virtual std::string getObjectSystemClassName() const override { return k_objectSystemClassName; }

	virtual bool init(MikanObjectSystemDefinitionPtr definitionPtr) override;
	virtual void postInit() override;
	virtual void dispose() override;
	virtual void update(float deltaSeconds) override;

	inline ProjectScriptContextPtr getScriptContext() const { return m_scriptContext; }
	inline void requestReload() { m_bReloadPending= true; }
	// Tear down the state and load every script with a path again, in pool order
	void reloadAllScripts();
	// The load errors of the last reload, for a listener that subscribed after it
	inline const std::vector<ScriptError>& getLastLoadErrors() const { return m_lastLoadErrors; }

	// Create an empty timestamped .lua under the project's scripts folder and
	// register it
	ScriptComponentPtr addNewScript();

	// Route edits go through the definition, so each one records a transaction
	bool addHttpRoute(const ScriptHttpRoute& route);
	bool setHttpRoute(size_t index, const ScriptHttpRoute& route);
	bool removeHttpRoute(size_t index);
	// True when the route names a loaded script with that HttpTrigger_ method
	bool isHttpRouteResolved(const ScriptHttpRoute& route) const;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	MulticastDelegate<void(const ScriptError& error)> OnScriptError;
	MulticastDelegate<void(bool bSuccess)> OnScriptsReloaded;

private:
	void disposeScriptContext();
	void onScriptError(const ScriptError& error);
	void onDefinitionPropertyChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);
	// Push the route table into the request handler, which installs the routes
	void applyHttpRoutes();

	// Component reference fields follow object creation and destruction in
	// every system, so a script never holds a handle to a dead object
	void bindObjectLifecycleEvents();
	void unbindObjectLifecycleEvents();
	void onObjectFinalized(MikanObjectSystemPtr objectSystem, MikanObjectPtr object);
	void onObjectWillBeDestroyed(MikanObjectSystemPtr objectSystem, MikanComponentPtr primaryComponent);

	ProjectScriptContextPtr m_scriptContext;
	std::vector<ScriptError> m_lastLoadErrors;
	bool m_bReloadPending= false;
	bool m_bLifecycleEventsBound= false;
	bool m_bDefinitionEventsBound= false;
};

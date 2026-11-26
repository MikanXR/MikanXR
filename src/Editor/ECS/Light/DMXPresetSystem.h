#pragma once

#include "ComponentFwd.h"
#include "DMXPresetComponent.h"
#include "LightSystemFwd.h"
#include "MikanLightTypes.h"
#include "MikanTypedObjectSystem.h"
#include "ObjectSystemConfigFwd.h"

#include <memory>
#include <string>

// -- DMXPresetSystemDefinition -----
class DMXPresetSystemDefinition
	: public MikanTypedObjectSystemDefinition<DMXPresetComponent, DMXPresetDefinition, MikanDMXPresetID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<DMXPresetComponent, DMXPresetDefinition, MikanDMXPresetID>;

	DMXPresetSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);
};

// -- DMXPresetSystem -----
class DMXPresetSystem : public MikanTypedObjectSystem<DMXPresetComponent, DMXPresetDefinition, MikanDMXPresetID,
													  DMXPresetSystem, DMXPresetSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<DMXPresetComponent, DMXPresetDefinition, MikanDMXPresetID, DMXPresetSystem,
										DMXPresetSystemDefinition>;

	DMXPresetSystem(ProjectManagerPtr ownerProjectManager);

	inline static const std::string k_objectSystemClassName= "DMXPresetSystem";
	virtual std::string getObjectSystemClassName() const override { return k_objectSystemClassName; }

	virtual void postInit() override;
	virtual void dispose() override;

	// A default preset binds to the first group, so it has fixtures to address
	virtual MikanComponentPtr addNewObjectWithDefaultDefinition(const std::string& primaryComponentClass) override;

	inline DMXPresetComponentPtr getPresetById(MikanDMXPresetID presetId) const
	{
		return Super::getTypedComponentById(presetId);
	}
	inline DMXPresetComponentPtr getPresetByName(const std::string& name) const
	{
		return Super::getTypedComponentByName(name);
	}

	// An empty preset for the group, named by the caller or by default
	DMXPresetComponentPtr createPreset(MikanDMXFixtureGroupID groupId, const std::string& name);
	bool removePreset(MikanDMXPresetID presetId);
	// Destroy every preset addressing the group, each as its own transaction.
	// Called before the group's own removal so undo recreates the group first.
	void removePresetsForGroup(MikanDMXFixtureGroupID groupId);

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

private:
	// A scene that fell back to a destroyed preset lets go of it. This runs inside
	// the destroy's transaction, so undoing the destroy restores the scene's choice.
	void onPresetComponentDisposed(MikanObjectSystemPtr objectSystem, MikanComponentConstPtr component);

	bool m_bLifecycleEventsBound= false;
};

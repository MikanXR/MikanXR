#include "PropertyNotificationGuardTests.h"
#include "unit_test.h"

#include "CommonConfig.h"
#include "MikanComponent.h"
#include "MikanVariantTypes.h"
#include "MulticastDelegate.h"
#include "PropertyInterface.h"

// -- Component classes under test --
#include "Anchor/AnchorComponent.h"
#include "Camera/CameraComponent.h"
#include "Compositor/CompositorComponent.h"
#include "Light/LightEnvironmentComponent.h"
#include "Light/DMXFixtureGroupComponent.h"
#include "Light/DMXPresetComponent.h"
#include "Light/DMXSequenceComponent.h"
#include "Light/RGBPixelGridComponent.h"
#include "Light/RGBSpotLightComponent.h"
#include "Marker/MarkerComponent.h"
#include "Output/RTPOutputComponent.h"
#include "Output/SpoutOutputComponent.h"
#include "Scene/SceneComponent.h"
#include "Scene/TransformComponent.h"
#include "Script/ScriptComponent.h"
#include "Shape/BoxShapeComponent.h"
#include "Shape/ModelShapeComponent.h"
#include "Shape/QuadShapeComponent.h"
#include "Stage/StageComponent.h"
#include "Stencil/BoxStencilComponent.h"
#include "Stencil/ModelStencilComponent.h"
#include "Stencil/QuadStencilComponent.h"
#include "TrackingMount/TrackingMountComponent.h"
#include "TrackingVolume/MarkerTrackingVolumeComponent.h"
#include "TrackingVolume/VRTrackingVolumeComponent.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
// Properties excluded from the drive across every class, each with a reason:
// - transform relative_*/parent and component_name: the definition write is
//   gated on component init, and reparenting needs a live project
// - compositor_graph_path: rebuilding the node graph needs a live renderer
static const std::set<std::string> k_skippedPropertyNames= {
	"relative_scale",      "relative_quaternion", "relative_rotation",     "relative_position",
	"parent_transform_id", "component_name",      "compositor_graph_path",
};

// (class, property) pairs where a successful set is known not to notify,
// each entry a standing gap to burn down:
// - RGBSpotLightComponent red/green/blue are runtime-only members with no
//   definition backing, so they cannot notify or persist
// - display_tracking_space is a runtime-only viewport display toggle
static const std::set<std::string> k_noNotifyAllowlist= {
	"RGBSpotLightComponent/red",
	"RGBSpotLightComponent/green",
	"RGBSpotLightComponent/blue",
	"VRTrackingVolumeComponent/display_tracking_space",
};

// Collector for the names a definition notifies during one set call
class NotificationRecorder
{
public:
	void onPropertyChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet)
	{
		for (const std::string& name : changedPropertySet.getSet())
		{
			notifiedNames.insert(name);
		}
	}

	std::set<std::string> notifiedNames;
};

// Synthesize a value different from the current one, so setters with
// equality guards still mutate and notify
static bool makePerturbedValue(const MikanVariant& currentValue, MikanVariant& outValue)
{
	switch (currentValue.value_type)
	{
	case MikanVariantType::BOOL:
		outValue.setValue(!currentValue.getBoolValue());
		return true;
	case MikanVariantType::UBYTE:
		outValue.setValue((uint8_t)(currentValue.getUByteValue() + 1));
		return true;
	case MikanVariantType::USHORT:
		outValue.setValue((uint16_t)(currentValue.getUShortValue() + 1));
		return true;
	case MikanVariantType::INT:
		outValue.setValue(currentValue.getIntValue() + 1);
		return true;
	case MikanVariantType::LONG:
		outValue.setValue(currentValue.getLongValue() + 1);
		return true;
	case MikanVariantType::FLOAT:
		outValue.setValue(currentValue.getFloatValue() + 0.5f);
		return true;
	case MikanVariantType::DOUBLE:
		outValue.setValue(currentValue.getDoubleValue() + 0.5);
		return true;
	case MikanVariantType::STRING:
		outValue.setValue(std::string(currentValue.getUtf8Value()) + "_guard");
		return true;
	case MikanVariantType::VECTOR2F:
	{
		MikanVector2f v= currentValue.getVector2fValue();
		v.x+= 0.25f;
		outValue.setValue(v);
		return true;
	}
	case MikanVariantType::VECTOR3F:
	{
		MikanVector3f v= currentValue.getVector3fValue();
		v.x+= 0.25f;
		outValue.setValue(v);
		return true;
	}
	case MikanVariantType::VECTOR4F:
	{
		MikanVector4f v= currentValue.getVector4fValue();
		v.x+= 0.25f;
		outValue.setValue(v);
		return true;
	}
	case MikanVariantType::QUATERNIONF:
	{
		MikanQuatf q= currentValue.getQuaternionfValue();
		q.x+= 0.25f;
		outValue.setValue(q);
		return true;
	}
	default:
		// Array/map/object typed properties are not driven
		return false;
	}
}

struct GuardTestEntry
{
	const char* componentClassName;
	std::function<MikanComponentPtr()> makeComponent;
	std::function<void(std::vector<PropertyDescriptorConstPtr>&)> getDescriptors;
};

#define GUARD_ENTRY(ComponentClass, DefinitionClass)                                                                   \
	{#ComponentClass,                                                                                                  \
	 []() -> MikanComponentPtr                                                                                         \
	 {                                                                                                                 \
		 auto component= std::make_shared<ComponentClass>(MikanObjectWeakPtr());                                       \
		 component->setDefinition(std::make_shared<DefinitionClass>());                                                \
		 return component;                                                                                             \
	 },                                                                                                                \
	 [](std::vector<PropertyDescriptorConstPtr>& out) { ComponentClass::getPropertyDescriptors(out); }}

// Definition-backed component classes safe to drive without a live project.
// Deliberately absent, with reasons:
// - USBVideoSourceComponent / NetworkVideoSourceComponent: setters reach
//   device handles and connection state
// - VRDeviceComponent: transient runtime-only devices
// - texture source components: setters reach CEF/Spout runtime state
static const GuardTestEntry k_guardTestEntries[]= {
	GUARD_ENTRY(AnchorComponent, AnchorDefinition),
	GUARD_ENTRY(CameraComponent, CameraDefinition),
	GUARD_ENTRY(CompositorComponent, CompositorDefinition),
	GUARD_ENTRY(LightEnvironmentComponent, LightEnvironmentDefinition),
	GUARD_ENTRY(RGBPixelGridComponent, RGBPixelGridDefinition),
	GUARD_ENTRY(DMXFixtureGroupComponent, DMXFixtureGroupDefinition),
	GUARD_ENTRY(DMXPresetComponent, DMXPresetDefinition),
	GUARD_ENTRY(DMXSequenceComponent, DMXSequenceDefinition),
	GUARD_ENTRY(RGBSpotLightComponent, RGBSpotLightDefinition),
	GUARD_ENTRY(MarkerComponent, MarkerDefinition),
	GUARD_ENTRY(SceneComponent, SceneComponentDefinition),
	GUARD_ENTRY(RTPOutputComponent, RTPOutputDefinition),
	GUARD_ENTRY(SpoutOutputComponent, SpoutOutputDefinition),
	GUARD_ENTRY(ScriptComponent, ScriptDefinition),
	GUARD_ENTRY(StageComponent, StageComponentDefinition),
	GUARD_ENTRY(BoxShapeComponent, BoxShapeDefinition),
	GUARD_ENTRY(ModelShapeComponent, ModelShapeDefinition),
	GUARD_ENTRY(QuadShapeComponent, QuadShapeDefinition),
	GUARD_ENTRY(BoxStencilComponent, BoxStencilDefinition),
	GUARD_ENTRY(ModelStencilComponent, ModelStencilDefinition),
	GUARD_ENTRY(QuadStencilComponent, QuadStencilDefinition),
	GUARD_ENTRY(TrackingMountComponent, TrackingMountDefinition),
	GUARD_ENTRY(MarkerTrackingVolumeComponent, MarkerTrackingVolumeDefinition),
	GUARD_ENTRY(VRTrackingVolumeComponent, VRTrackingVolumeDefinition),
};

#undef GUARD_ENTRY
} // namespace

bool property_notification_guard_test_sets_notify_with_known_names()
{
	UNIT_TEST_BEGIN("writable property sets notify with descriptor-resolvable names")

	for (const GuardTestEntry& entry : k_guardTestEntries)
	{
		MikanComponentPtr component= entry.makeComponent();
		MikanComponentDefinitionPtr definition= component->getDefinition();

		std::vector<PropertyDescriptorConstPtr> descriptors;
		entry.getDescriptors(descriptors);

		std::set<std::string> descriptorNames;
		for (const PropertyDescriptorConstPtr& descriptor : descriptors)
		{
			descriptorNames.insert(descriptor->getName());
		}

		NotificationRecorder recorder;
		definition->OnPropertyChanged+= MakeDelegate(&recorder, &NotificationRecorder::onPropertyChanged);

		for (const PropertyDescriptorConstPtr& descriptor : descriptors)
		{
			const std::string& propertyName= descriptor->getName();
			if (descriptor->isReadOnly() || k_skippedPropertyNames.count(propertyName) > 0)
				continue;

			MikanVariant currentValue;
			if (!component->getPropertyValue(propertyName, currentValue))
				continue;

			MikanVariant newValue;
			if (!makePerturbedValue(currentValue, newValue))
				continue;

			recorder.notifiedNames.clear();
			if (!component->setPropertyValue(propertyName, newValue))
				continue;

			const std::string allowKey= std::string(entry.componentClassName) + "/" + propertyName;

			// A successful set on a writable property must notify
			if (recorder.notifiedNames.empty() && k_noNotifyAllowlist.count(allowKey) == 0)
			{
				fprintf(stdout, "    FAILED: %s set '%s' succeeded without notifying\n", entry.componentClassName,
						propertyName.c_str());
				success= false;
			}

			// Every notified name must resolve to a descriptor of this class
			for (const std::string& notifiedName : recorder.notifiedNames)
			{
				if (descriptorNames.count(notifiedName) == 0)
				{
					fprintf(stdout, "    FAILED: %s set '%s' notified unknown name '%s'\n", entry.componentClassName,
							propertyName.c_str(), notifiedName.c_str());
					success= false;
				}
			}
		}

		definition->OnPropertyChanged-= MakeDelegate(&recorder, &NotificationRecorder::onPropertyChanged);
	}

	UNIT_TEST_COMPLETE()
}

bool run_property_notification_guard_tests()
{
	UNIT_TEST_MODULE_BEGIN("property_notification_guard")
	UNIT_TEST_MODULE_CALL_TEST(property_notification_guard_test_sets_notify_with_known_names);
	UNIT_TEST_MODULE_END()
}

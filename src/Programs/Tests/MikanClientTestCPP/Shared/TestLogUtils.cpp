#include "MikanAnchorTypes.h"
#include "MikanCameraTypes.h"
#include "MikanComponentTypes.h"
#include "MikanCompositorTypes.h"
#include "TestLogUtils.h"
#include "MikanMarkerTypes.h"
#include "MikanPropertyTypes.h"
#include "MikanSceneTypes.h"
#include "MikanStageTypes.h"
#include "MikanStencilTypes.h"
#include "MikanTextureSourceTypes.h"
#include "MikanTrackingMountTypes.h"
#include "MikanTrackingVolumeTypes.h"
#include "MikanTransformTypes.h"
#include "MikanVideoSourceTypes.h"
#include "MikanVRDeviceTypes.h"
#include "MikanMathTypes.h"

#include "Logger.h"

void TestLogUtils::logComponent(const MikanComponentValues& componentInfo)
{
	MIKAN_LOG_INFO("MikanComponent") << "Component ID: " << componentInfo.component_id;
	MIKAN_LOG_INFO("MikanComponent") << "Component Name: " << componentInfo.component_name.getUtf8Value();
}

void TestLogUtils::logComponent(const MikanTransformComponentValues& transformInfo)
{
	logComponent((const MikanComponentValues&)transformInfo);

	const MikanVector3f& s= transformInfo.relative_scale;
	const MikanQuatf& q= transformInfo.relative_quaternion;
	const MikanVector3f& t= transformInfo.relative_position;

	MIKAN_LOG_INFO("TransformComponent") << "Parent Transform Id: " << transformInfo.parent_transform_id;
	MIKAN_LOG_INFO("TransformComponent") << "Scale: " << s.x << ", " << s.y << ", " << s.z;
	MIKAN_LOG_INFO("TransformComponent") << "Quaternion: " << q.w << ", " << q.x << ", " << q.y << ", " << q.z;
	MIKAN_LOG_INFO("TransformComponent") << "Position: " << t.x << ", " << t.y << ", " << t.z;
}

void TestLogUtils::logComponent(const MikanAnchorComponentValues& anchorInfo)
{
	logComponent((const MikanTransformComponentValues&)anchorInfo);

	MIKAN_LOG_INFO("AnchorComponent") << "Anchor Id: " << anchorInfo.component_id;
}

void TestLogUtils::logComponent(const MikanStencilComponentValues& stencilInfo)
{
	logComponent((const MikanTransformComponentValues&)stencilInfo);

	MIKAN_LOG_INFO("StencilComponent") << "Is Disabled: " << (stencilInfo.is_disabled ? "true" : "false");
	MIKAN_LOG_INFO("StencilComponent") << "Cull Mode: " << stencilInfo.cull_mode;
}

void TestLogUtils::logComponent(const MikanQuadStencilComponentValues& quadInfo)
{
	logComponent((const MikanStencilComponentValues&)quadInfo);

	MIKAN_LOG_INFO("QuadStencilComponent") << "Quad Width: " << quadInfo.quad_width;
	MIKAN_LOG_INFO("QuadStencilComponent") << "Quad Height: " << quadInfo.quad_height;
}

void TestLogUtils::logComponent(const MikanBoxStencilComponentValues& boxInfo)
{
	logComponent((const MikanStencilComponentValues&)boxInfo);

	MIKAN_LOG_INFO("BoxStencilComponent") << "Box X Size: " << boxInfo.box_x_size;
	MIKAN_LOG_INFO("BoxStencilComponent") << "Box Y Size: " << boxInfo.box_y_size;
	MIKAN_LOG_INFO("BoxStencilComponent") << "Box Z Size: " << boxInfo.box_z_size;
}

void TestLogUtils::logComponent(const MikanModelStencilComponentValues& modelInfo)
{
	logComponent((const MikanStencilComponentValues&)modelInfo);

	MIKAN_LOG_INFO("ModelStencilComponent") << "Model: " << modelInfo.model_path.getUtf8Value();
}

void TestLogUtils::logComponent(const MikanVRDeviceComponentValues& vrDeviceInfo)
{
	MIKAN_LOG_INFO("VRDeviceComponent") << "Device Name: " << vrDeviceInfo.vr_device_path.getUtf8Value();
}

void TestLogUtils::logTransform(const MikanTransform& xform)
{
	const MikanVector3f& s= xform.scale;
	const MikanVector3f& t= xform.position;
	const MikanQuatf& q= xform.rotation;

	MIKAN_LOG_INFO("TransformComponent") << "  Scale: " << s.x << ", " << s.y << ", " << s.z;
	MIKAN_LOG_INFO("TransformComponent") << "  Rotation: " << q.x << ", " << q.y << ", " << q.z << ", " << q.w;
	MIKAN_LOG_INFO("TransformComponent") << "  Position: " << t.x << ", " << t.y << ", " << t.z;
}
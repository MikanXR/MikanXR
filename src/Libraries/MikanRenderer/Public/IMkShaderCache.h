#pragma once

#include "MkRendererExport.h"
#include "MkRendererFwd.h"

#include <memory>
#include <string>
#include <map>

#define INTERNAL_MATERIAL_PT_FULLSCREEN_RGB_TEXTURE "Internal_PT_FullscreenRGBTexture"
#define INTERNAL_MATERIAL_PT_UNDISTORT_FULLSCREEN_RGB_TEXTURE "Internal_PT_UndistortFullscreenRGBTexture"
#define INTERNAL_MATERIAL_PT_CONVERT_NV12_TO_RGBA "Internal_PT_ConvertNV12ToRGBA"
#define INTERNAL_MATERIAL_PT_FULLSCREEN_RGBA_TEXTURE "Internal_PT_FullscreenRGBATexture"
// Box-filter downsample of a texture rendered above the destination resolution. The RGBA
// variant can weight color by coverage so a silhouette edge does not drag the empty
// region's color in with it.
#define INTERNAL_MATERIAL_PT_RESOLVE_DOWNSAMPLE_RGB "Internal_PT_ResolveDownsampleRGB"
#define INTERNAL_MATERIAL_PT_RESOLVE_DOWNSAMPLE_RGBA "Internal_PT_ResolveDownsampleRGBA"
#define INTERNAL_MATERIAL_TEXT "Internal_Text"
#define INTERNAL_MATERIAL_UNPACK_RGBA_DEPTH_TEXTURE "Internal_UnpackRGBADepthTexture"
#define INTERNAL_MATERIAL_P_WIREFRAME "Internal_P_Wireframe"
#define INTERNAL_MATERIAL_P_SOLID_COLOR "Internal_P_SolidColor"
#define INTERNAL_MATERIAL_PC_UNLIT_COLOR "Internal_PC_UnlitColor"
#define INTERNAL_MATERIAL_PNT_TEXTURED "Internal_PNT_Textured"
#define INTERNAL_MATERIAL_PNT_TEXTURED_LIT_COLORED "Internal_PNT_TexturedLitColored"
#define INTERNAL_MATERIAL_P_LINEAR_DEPTH "Internal_P_LinearDepth"
#define INTERNAL_MATERIAL_PT_NORMALIZE_DEPTH "Internal_PT_NormalizeDepth"
#define INTERNAL_MATERIAL_PT_LINEAR_TO_HARDWARE_DEPTH "Internal_PT_LinearToHardwareDepth"
#define INTERNAL_MATERIAL_PT_PM5544_TEST_CARD "Internal_PT_PM5544TestCard"
#define INTERNAL_MATERIAL_PT_TEXTURED "Internal_PT_Textured"
#define INTERNAL_MATERIAL_P_CONE_VOLUME "Internal_P_ConeVolume"
#define INTERNAL_MATERIAL_P_SH_ENVIRONMENT "Internal_P_SHEnvironment"
#define INTERNAL_MATERIAL_PT_LINEARIZE_RGB "Internal_PT_LinearizeRGB"
#define INTERNAL_MATERIAL_PT_LINEAR_TO_SRGB "Internal_PT_LinearToSRGB"

class IMkShaderCache
{
public:
	virtual ~IMkShaderCache() {}

	virtual bool startup()= 0;
	virtual void shutdown()= 0;

	virtual MkMaterialPtr registerMaterial(IMkShaderCodeConstPtr code)= 0;
	virtual MkMaterialConstPtr getMaterialByName(const std::string& name)= 0;

	virtual IMkShaderPtr fetchCompiledIMkShader(IMkShaderCodeConstPtr code)= 0;
};

MIKAN_RENDERER_FUNC(IMkShaderCachePtr) createMkShaderCache(class IMkGraphicsContext* ownerContext);
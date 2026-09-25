#pragma once

#include "SharedTextureFwd.h"
#include "SharedTextureExport.h"
#include "SharedTextureLogger.h"

#include <stdint.h>
#include <string>
#include <memory>
#include <functional>

enum class SharedClientGraphicsApi : int
{
	UNKNOWN= -1,

	Direct3D9,
	Direct3D11,
	Direct3D12,
	OpenGL,
	Metal,
	Vulkan,
};

enum class SharedColorBufferType : int
{
	NOCOLOR,
	RGB24,
	// DXGI_FORMAT_R8G8B8A8_UNORM / DXGI_FORMAT_R8G8B8A8_TYPELESS
	RGBA32,
	// DXGI_FORMAT_B8G8R8A8_UNORM / DXGI_FORMAT_B8G8R8A8_TYPELESS
	BGRA32,
	// 16-bit-per-channel half-float (64bpp), RGBA order / DXGI_FORMAT_R16G16B16A16_FLOAT
	RGBA16F,
};

enum class SharedDepthBufferType : int
{
	NODEPTH,
	// Raw float non-linear depth values from the z-buffer (source engine world units)
	// https://developer.nvidia.com/blog/visualizing-depth-precision/
	FLOAT_DEVICE_DEPTH,
	// Linearized float distance-from-camera values (source engine world units)
	FLOAT_SCENE_DEPTH,
	// DXGI_FORMAT_R8G8B8A8_UNORM / DXGI_FORMAT_R8G8B8A8_TYPELESS
	PACK_DEPTH_RGBA,
};

// Optional second color buffer carrying a per-channel shadow (light survival) factor.
// Composited multiplicatively against the scene so it darkens/tints the background;
// identity (no shadow) is white. Mirrors SharedColorBufferType.
enum class SharedShadowBufferType : int
{
	NOSHADOW,
	RGB24,
	// DXGI_FORMAT_R8G8B8A8_UNORM / DXGI_FORMAT_R8G8B8A8_TYPELESS
	RGBA32,
	// DXGI_FORMAT_B8G8R8A8_UNORM / DXGI_FORMAT_B8G8R8A8_TYPELESS
	BGRA32,
	// 16-bit-per-channel half-float (64bpp), RGBA order / DXGI_FORMAT_R16G16B16A16_FLOAT
	RGBA16F,
};

struct SharedTextureDescriptor
{
	SharedColorBufferType color_buffer_type= SharedColorBufferType::NOCOLOR;
	SharedDepthBufferType depth_buffer_type= SharedDepthBufferType::NODEPTH;
	SharedShadowBufferType shadow_buffer_type= SharedShadowBufferType::NOSHADOW;
	// Size of the color buffer
	uint32_t width= 0;
	uint32_t height= 0;
	// Size of the depth and shadow buffers. 0 means same as width/height. The writers size
	// their senders from the texture each write hands them rather than from these, but the
	// descriptor round-trips back out through getCameraRenderTargetDescriptor, so a field
	// missing here is a field the server never learns about.
	uint32_t aux_width= 0;
	uint32_t aux_height= 0;
	SharedClientGraphicsApi graphicsAPI= SharedClientGraphicsApi::UNKNOWN;
};

// The Vulkan client's device, mirroring MikanVulkanDeviceInterface without the Vulkan headers:
// every Vulkan handle is a pointer on the x64 builds this library ships. Passed as the
// apiDeviceInterface of a Vulkan accessor.
struct SharedVulkanDeviceInterface
{
	void* instance= nullptr;       // VkInstance
	void* physicalDevice= nullptr; // VkPhysicalDevice
	void* device= nullptr;         // VkDevice
	void* queue= nullptr;          // VkQueue, graphics-capable
	uint32_t queueFamilyIndex= 0;
};

// One frame's image from a Vulkan client, mirroring MikanVulkanTexture. Passed as the
// ApiTexturePtr of a Vulkan accessor's write calls.
struct SharedVulkanTexture
{
	void* image= nullptr; // VkImage
	int32_t layout= 0;    // VkImageLayout the image is in on entry and is returned to
	int32_t format= 0;    // VkFormat
	uint32_t width= 0;
	uint32_t height= 0;
};

class ISharedTextureWriteAccessor
{
public:
	virtual ~ISharedTextureWriteAccessor() {}

	virtual bool initialize(const struct SharedTextureDescriptor* descriptor, bool bEnableFrameCounter,
							void* apiDeviceInterface= nullptr, void* apiCommandQueueInterface= nullptr)= 0;
	virtual void dispose()= 0;

	virtual bool writeColorFrameTexture(void* ApiTexturePtr)= 0;
	virtual bool writeDepthFrameTexture(void* ApiTexturePtr, float zNear, float zFar)= 0;
	virtual bool writeShadowFrameTexture(void* ApiTexturePtr)= 0;
	virtual void* getPackDepthTextureResourcePtr() const= 0;
	virtual bool getIsInitialized() const= 0;
	virtual const SharedTextureDescriptor* getRenderTargetDescriptor() const= 0;
	virtual void setLogCallback(SharedTextureLogCallback callback)= 0;
};

MIKAN_SHAREDTEXTURE_FUNC(ISharedTextureWriteAccessorPtr) createSharedTextureWriteAccessor(const std::string& prefix,
																						  MikanCameraID cameraId);
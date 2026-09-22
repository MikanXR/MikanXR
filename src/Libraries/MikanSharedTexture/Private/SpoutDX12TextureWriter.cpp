#include "SharedTextureWriterBackend.h"
#include "SharedTextureLogger.h"
#include "SpoutDX.h"
#include "SpoutDX12.h"
#include "SpoutDXDepthTexturePacker.h"

#include <sstream>
#include <string>
#include <vector>

class SpoutDX12TextureWriter : public ISharedTextureWriterBackend
{
public:
	SpoutDX12TextureWriter(const SharedTextureWriterContext& context)
		: m_context(context)
		, m_logger(*context.logger)
		, m_spoutColorFrame()
		, m_spoutDepthFrame()
		, m_spoutShadowFrame()
		, m_depthTexturePacker(nullptr)
	{
	}

	virtual ~SpoutDX12TextureWriter() { dispose(); }

	bool init() override
	{
		const SharedTextureDescriptor* descriptor= m_context.descriptor;
		ID3D12Device* d3d12Device= (ID3D12Device*)m_context.apiDeviceInterface;
		// Optional client command queue. When supplied, the D3D11On12 device shares it so the
		// wrapped-resource copy is GPU-ordered after the client's rendering (no flicker/tearing).
		IUnknown* commandQueue= (IUnknown*)m_context.apiCommandQueueInterface;
		IUnknown** ppCommandQueue= (commandQueue != nullptr) ? &commandQueue : nullptr;
		bool bSuccess= true;

		dispose();

		applySpoutDXLogPolicy();

		// Initialize the color spout frame
		if (descriptor->color_buffer_type == SharedColorBufferType::RGBA32
			|| descriptor->color_buffer_type == SharedColorBufferType::BGRA32
			|| descriptor->color_buffer_type == SharedColorBufferType::RGBA16F)
		{
			if (m_spoutColorFrame.OpenDirectX12(d3d12Device, ppCommandQueue)
				&& m_spoutColorFrame.SetSenderName(m_context.colorSenderName.c_str()))
			{
				if (descriptor->color_buffer_type == SharedColorBufferType::RGBA16F)
					m_spoutColorFrame.SetSenderFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
				else if (descriptor->color_buffer_type == SharedColorBufferType::BGRA32)
					m_spoutColorFrame.SetSenderFormat(DXGI_FORMAT_B8G8R8A8_UNORM);
				else
					m_spoutColorFrame.SetSenderFormat(DXGI_FORMAT_R8G8B8A8_UNORM);

				if (!m_context.bEnableFrameCounter)
					m_spoutColorFrame.DisableFrameCount();

				m_bIsColorFrameInitialized= true;
			}
			else
			{
				m_logger.log(SharedTextureLogLevel::info,
							 "SpoutDX11TextureWriter::init() - Error initializing color spout frame");
				return false;
			}
		}
		else
		{
			std::stringstream ss;
			ss << "SpoutDX11TextureWriter::init() - color buffer type not supported: ";
			ss << (int)descriptor->color_buffer_type;
			m_logger.log(SharedTextureLogLevel::info, ss.str());
			return false;
		}

		// Initialize the depth spout frame, if requested
		if (descriptor->depth_buffer_type != SharedDepthBufferType::NODEPTH)
		{
			if (m_spoutDepthFrame.OpenDirectX12(d3d12Device, ppCommandQueue)
				&& m_spoutDepthFrame.SetSenderName(m_context.depthSenderName.c_str()))
			{
				// Initialize the depth texture packer if we are sending float depth textures
				if (descriptor->depth_buffer_type == SharedDepthBufferType::FLOAT_DEVICE_DEPTH
					|| descriptor->depth_buffer_type == SharedDepthBufferType::FLOAT_SCENE_DEPTH)
				{
					m_depthTexturePacker= new SpoutDXDepthTexturePacker(m_logger, m_spoutDepthFrame, descriptor);
					if (!m_depthTexturePacker->init())
					{
						m_logger.log(SharedTextureLogLevel::info,
									 "SpoutDX11TextureWriter::init() - Error initializing float depth packer");
						return false;
					}
				}

				m_spoutDepthFrame.SetSenderFormat(DXGI_FORMAT_R8G8B8A8_UNORM);

				if (!m_context.bEnableFrameCounter)
					m_spoutDepthFrame.DisableFrameCount();

				m_bIsDepthFrameInitialized= true;
			}
			else
			{
				m_logger.log(SharedTextureLogLevel::info,
							 "SpoutDX11TextureWriter::init() - Error initializing depth spout frame");
				return false;
			}
		}

		// Initialize the (optional) shadow spout frame. It's a color-like RGBA8/BGRA8 buffer.
		if (descriptor->shadow_buffer_type != SharedShadowBufferType::NOSHADOW)
		{
			if (m_spoutShadowFrame.OpenDirectX12(d3d12Device, ppCommandQueue)
				&& m_spoutShadowFrame.SetSenderName(m_context.shadowSenderName.c_str()))
			{
				if (descriptor->shadow_buffer_type == SharedShadowBufferType::RGBA16F)
					m_spoutShadowFrame.SetSenderFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
				else if (descriptor->shadow_buffer_type == SharedShadowBufferType::BGRA32)
					m_spoutShadowFrame.SetSenderFormat(DXGI_FORMAT_B8G8R8A8_UNORM);
				else
					m_spoutShadowFrame.SetSenderFormat(DXGI_FORMAT_R8G8B8A8_UNORM);

				if (!m_context.bEnableFrameCounter)
					m_spoutShadowFrame.DisableFrameCount();

				m_bIsShadowFrameInitialized= true;
			}
			else
			{
				m_logger.log(SharedTextureLogLevel::info,
							 "SpoutDX12TextureWriter::init() - Error initializing shadow spout frame");
				return false;
			}
		}

		return true;
	}

	void dispose()
	{
		if (m_depthTexturePacker != nullptr)
		{
			delete m_depthTexturePacker;
			m_depthTexturePacker= nullptr;
		}

		releaseWrappedResources(m_colorWrappedResources);
		m_spoutColorFrame.ReleaseSender();
		m_spoutColorFrame.CloseDirectX12();
		m_bIsColorFrameInitialized= false;

		releaseWrappedResources(m_depthWrappedResources);
		m_spoutDepthFrame.ReleaseSender();
		m_spoutDepthFrame.CloseDirectX12();
		m_bIsDepthFrameInitialized= false;

		releaseWrappedResources(m_shadowWrappedResources);
		m_spoutShadowFrame.ReleaseSender();
		m_spoutShadowFrame.CloseDirectX12();
		m_bIsShadowFrameInitialized= false;

		DisableSpoutLog();
	}

	bool writeColorFrameTexture(void* apiTexturePtr) override
	{
		ID3D12Resource* dx12TextureResource= (ID3D12Resource*)apiTexturePtr;

		bool bSuccess= false;

		if (m_bIsColorFrameInitialized)
		{
			ID3D11Resource* wrappedTexture=
				getOrWrapResource(m_spoutColorFrame, m_colorWrappedResources, dx12TextureResource);
			if (wrappedTexture != nullptr)
			{
				bSuccess= m_spoutColorFrame.SendDX11Resource(wrappedTexture);
			}
		}

		return bSuccess;
	}

	bool writeDepthFrameTexture(void* apiTexturePtr, float zNear, float zFar) override
	{
		ID3D12Resource* dx12TextureResource= (ID3D12Resource*)apiTexturePtr;

		bool bSuccess= false;

		if (m_bIsDepthFrameInitialized)
		{
			ID3D11Resource* wrappedTexture=
				getOrWrapResource(m_spoutDepthFrame, m_depthWrappedResources, dx12TextureResource);
			if (wrappedTexture != nullptr)
			{
				if (m_depthTexturePacker != nullptr)
				{
					// Convert the float depth texture to a RGBA8 texture using a shader
					// (Spout can only send RGBA8 textures)
					ID3D11Texture2D* pTexture11= (ID3D11Texture2D*)wrappedTexture;
					ID3D11Texture2D* packedDepthTexture=
						m_depthTexturePacker->packDepthTexture(pTexture11, zNear, zFar);

					if (packedDepthTexture != nullptr)
					{
						bSuccess= m_spoutDepthFrame.SendTexture(packedDepthTexture);
					}
				}
				else
				{
					bSuccess= m_spoutDepthFrame.SendDX11Resource(wrappedTexture);
				}
			}
		}

		return bSuccess;
	}

	bool writeShadowFrameTexture(void* apiTexturePtr) override
	{
		ID3D12Resource* dx12TextureResource= (ID3D12Resource*)apiTexturePtr;

		bool bSuccess= false;

		if (m_bIsShadowFrameInitialized)
		{
			ID3D11Resource* wrappedTexture=
				getOrWrapResource(m_spoutShadowFrame, m_shadowWrappedResources, dx12TextureResource);
			if (wrappedTexture != nullptr)
			{
				bSuccess= m_spoutShadowFrame.SendDX11Resource(wrappedTexture);
			}
		}

		return bSuccess;
	}

	void* getPackDepthTextureResourcePtr() const override
	{
		return m_depthTexturePacker != nullptr ? m_depthTexturePacker->getPackedDepthTextureResourcePtr() : nullptr;
	}

private:
	// A client that pipelines its captures hands over a different staging texture each
	// publish, so the 11on12 wrap of each one is kept rather than recreated per frame
	struct WrappedResource
	{
		ID3D12Resource* dx12Resource= nullptr;
		ID3D11Resource* dx11Resource= nullptr;
	};
	using WrappedResourceCache= std::vector<WrappedResource>;
	static constexpr size_t k_maxWrappedResources= 8;

	ID3D11Resource* getOrWrapResource(spoutDX12& spoutFrame, WrappedResourceCache& cache,
									  ID3D12Resource* dx12TextureResource)
	{
		if (dx12TextureResource == nullptr)
			return nullptr;

		for (const WrappedResource& entry : cache)
		{
			if (entry.dx12Resource == dx12TextureResource)
				return entry.dx11Resource;
		}

		// The oldest wrap goes when the cache is full, since a client rotating through
		// more staging textures than this is not one that reuses them
		if (cache.size() >= k_maxWrappedResources)
		{
			cache.front().dx11Resource->Release();
			cache.erase(cache.begin());
		}

		// Wrap as GENERIC_READ to match the rest state (SRVMask) the client leaves the
		// staging texture in. Combined with InState == OutState in WrapDX12Resource, this
		// keeps 11on12 from issuing barriers that conflict with the client's state tracker.
		WrappedResource entry;
		if (!spoutFrame.WrapDX12Resource(dx12TextureResource, &entry.dx11Resource, D3D12_RESOURCE_STATE_GENERIC_READ))
			return nullptr;

		entry.dx12Resource= dx12TextureResource;
		cache.push_back(entry);

		return entry.dx11Resource;
	}

	static void releaseWrappedResources(WrappedResourceCache& cache)
	{
		for (WrappedResource& entry : cache)
		{
			if (entry.dx11Resource != nullptr)
				entry.dx11Resource->Release();
		}
		cache.clear();
	}

	const SharedTextureWriterContext& m_context;
	SharedTextureLogger& m_logger;
	spoutDX12 m_spoutColorFrame;
	WrappedResourceCache m_colorWrappedResources;
	spoutDX12 m_spoutDepthFrame;
	WrappedResourceCache m_depthWrappedResources;
	spoutDX12 m_spoutShadowFrame;
	WrappedResourceCache m_shadowWrappedResources;
	SpoutDXDepthTexturePacker* m_depthTexturePacker= nullptr;
	bool m_bIsColorFrameInitialized= false;
	bool m_bIsDepthFrameInitialized= false;
	bool m_bIsShadowFrameInitialized= false;
};

ISharedTextureWriterBackendPtr createDX12TextureWriter(const SharedTextureWriterContext& context)
{
	return std::make_unique<SpoutDX12TextureWriter>(context);
}

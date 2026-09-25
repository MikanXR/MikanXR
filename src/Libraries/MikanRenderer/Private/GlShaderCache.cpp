#include "IMkShaderCache.h"
#include "MkMaterial.h"
#include "IMkShader.h"
#include "IMkShaderCode.h"
#include "Logger.h"

namespace InternalShaders
{
bool registerInternalShaders(IMkShaderCache* shaderCache);
}

class GlShaderCache : public IMkShaderCache
{
public:
	GlShaderCache()= delete;
	GlShaderCache(IMkGraphicsContext* ownerContext)
		: m_ownerContext(ownerContext)
	{
	}
	virtual ~GlShaderCache() { shutdown(); }

	virtual bool startup() override { return InternalShaders::registerInternalShaders(this); }

	virtual void shutdown() override { m_programCache.clear(); }

	MkMaterialPtr GlShaderCache::registerMaterial(IMkShaderCodeConstPtr code)
	{
		const std::string materialName= code->getProgramName();

		auto it= m_materialCache.find(materialName);
		if (it != m_materialCache.end())
		{
			return it->second;
		}

		IMkShaderPtr program= fetchCompiledIMkShader(code);
		if (program)
		{
			auto material= std::make_shared<MkMaterial>(materialName, program);

			m_materialCache.insert({materialName, material});
			return material;
		}
		else
		{
			MIKAN_LOG_ERROR("GlShaderCache::registerMaterial") << "Failed to compile material: " << materialName;
			return MkMaterialPtr();
		}
	}

	MkMaterialConstPtr GlShaderCache::getMaterialByName(const std::string& name)
	{
		auto it= m_materialCache.find(name);
		if (it != m_materialCache.end())
		{
			return it->second;
		}

		return MkMaterialConstPtr();
	}

	IMkShaderPtr GlShaderCache::fetchCompiledIMkShader(IMkShaderCodeConstPtr code)
	{
		auto it= m_programCache.find(code->getProgramName());
		if (it != m_programCache.end())
		{
			IMkShaderPtr existingProgram= it->second;

			if (existingProgram->getProgramCode()->getCodeHash() == code->getCodeHash())
			{
				// Found a compiled version of the code
				return existingProgram;
			}
			else
			{
				// Old compiled program is stale so delete it
				m_programCache.erase(it);
			}
		}

		// (Re)compile program and add it to the cache
		IMkShaderPtr program= createIMkShader(code);
		if (program->compileProgram())
		{
			m_programCache[code->getProgramName()]= program;
			return program;
		}
		else
		{
			// Clean up the program if it failed to compile
			return nullptr;
		}
	}

private:
	IMkGraphicsContext* m_ownerContext;
	std::map<std::string, IMkShaderPtr> m_programCache;
	std::map<std::string, MkMaterialPtr> m_materialCache;
};

IMkShaderCachePtr createMkShaderCache(class IMkGraphicsContext* ownerContext)
{
	return std::make_shared<GlShaderCache>(ownerContext);
}

namespace InternalShaders
{
IMkShaderCodeConstPtr getPTTexturedFullScreenRGBQuad()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_FULLSCREEN_RGB_TEXTURE,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbTexture;

				void main()
				{
					vec3 col = texture(rgbTexture, TexCoords).rgb;
					FragColor = vec4(col, 1.0);
				} 
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbTexture", eUniformSemantic::rgbTexture);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTUndistortTexturedFullScreenRGBQuad()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_UNDISTORT_FULLSCREEN_RGB_TEXTURE,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbTexture;
				uniform sampler2D distortionTexture;

				void main()
				{
					vec2 offset = texture(distortionTexture, TexCoords.xy).rg;
					vec3 col = texture(rgbTexture, offset).rgb;

					FragColor = vec4(col, 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbTexture", eUniformSemantic::rgbTexture);
		x_shaderCode->addUniform("distortionTexture", eUniformSemantic::distortionTexture);
	}

	return x_shaderCode;
}

// NV12 (Y + interleaved UV planes) -> RGBA color conversion (ticket "Phase 6").
// Replaces the CUDA NV12ConversionKernel that used to do this on the plugin
// side - the plugin now only copies the decoded planes into GL textures (see
// CudaGLInterop.h in MikanARKitVideo), this shader does the actual YUV->RGB math.
IMkShaderCodeConstPtr getPTConvertNV12ToRGBAQuad()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_CONVERT_NV12_TO_RGBA,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D lumaTexture;
				uniform sampler2D chromaTexture;
				// 1.0 selects BT.709 (HD) limited-range coefficients, 0.0 selects
				// BT.601 (SD) - set from the source frame's resolution (see
				// ARKitVideoSourceComponent::processDirectVideoFrame), the same
				// resolution-based heuristic MikanGStreamerVideoDevice::
				// extractColorimetryInfo falls back to when caps don't carry
				// explicit colorimetry.
				uniform float floatConstant0;

				void main()
				{
					float y = texture(lumaTexture, TexCoords).r * 255.0;
					vec2 uv = texture(chromaTexture, TexCoords).rg * 255.0;
					float u = uv.x - 128.0;
					float v = uv.y - 128.0;

					float fy = 1.164 * (y - 16.0);

					vec3 rgb601 = vec3(fy + 1.596 * v, fy - 0.391 * u - 0.813 * v, fy + 2.018 * u);
					vec3 rgb709 = vec3(fy + 1.793 * v, fy - 0.213 * u - 0.533 * v, fy + 2.112 * u);
					vec3 rgb = mix(rgb601, rgb709, floatConstant0);

					FragColor = vec4(clamp(rgb / 255.0, 0.0, 1.0), 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("lumaTexture", eUniformSemantic::lumaTexture);
		x_shaderCode->addUniform("chromaTexture", eUniformSemantic::chromaTexture);
		x_shaderCode->addUniform("floatConstant0", eUniformSemantic::floatConstant0);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTTexturedFullScreenRGBAQuad()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_FULLSCREEN_RGBA_TEXTURE,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbaTexture;

				void main()
				{
					FragColor = texture(rgbaTexture, TexCoords).rgba;
				} 
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbaTexture", eUniformSemantic::rgbaTexture);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTResolveDownsampleRGBQuad()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_RESOLVE_DOWNSAMPLE_RGB,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbTexture;
				// Source size in texels
				uniform vec2 screenSize;
				// Source texels covered by one destination pixel, per axis
				uniform float floatConstant0;
				uniform float floatConstant1;

				void main()
				{
					// One tap per source texel the destination pixel covers, capped to match the
					// camera's maximum client render scale. Taps sit at the centers of equal
					// sub-intervals of the footprint, so the estimate is exact at integer ratios.
					int tapsX = clamp(int(ceil(floatConstant0)), 1, 4);
					int tapsY = clamp(int(ceil(floatConstant1)), 1, 4);
					vec2 footprint = vec2(floatConstant0, floatConstant1) / screenSize;
					vec2 origin = TexCoords - 0.5 * footprint;
					vec2 tapStep = footprint / vec2(float(tapsX), float(tapsY));

					vec3 sumRGB = vec3(0.0);
					for (int y = 0; y < tapsY; ++y)
					{
						for (int x = 0; x < tapsX; ++x)
						{
							vec2 uv = origin + (vec2(float(x), float(y)) + 0.5) * tapStep;
							sumRGB += texture(rgbTexture, uv).rgb;
						}
					}

					FragColor = vec4(sumRGB / float(tapsX * tapsY), 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbTexture", eUniformSemantic::rgbTexture);
		x_shaderCode->addUniform("screenSize", eUniformSemantic::screenSize);
		x_shaderCode->addUniform("floatConstant0", eUniformSemantic::floatConstant0);
		x_shaderCode->addUniform("floatConstant1", eUniformSemantic::floatConstant1);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTResolveDownsampleRGBAQuad()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_RESOLVE_DOWNSAMPLE_RGBA,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbaTexture;
				// Source size in texels
				uniform vec2 screenSize;
				// Source texels covered by one destination pixel, per axis
				uniform float floatConstant0;
				uniform float floatConstant1;
				// What the alpha channel means: 0 nothing, 1 coverage, 2 one minus coverage
				uniform float floatConstant2;

				void main()
				{
					// One tap per source texel the destination pixel covers, capped to match the
					// camera's maximum client render scale. Taps sit at the centers of equal
					// sub-intervals of the footprint, so the estimate is exact at integer ratios.
					int tapsX = clamp(int(ceil(floatConstant0)), 1, 4);
					int tapsY = clamp(int(ceil(floatConstant1)), 1, 4);
					vec2 footprint = vec2(floatConstant0, floatConstant1) / screenSize;
					vec2 origin = TexCoords - 0.5 * footprint;
					vec2 tapStep = footprint / vec2(float(tapsX), float(tapsY));
					float tapCount = float(tapsX * tapsY);

					int alphaMode = int(floatConstant2 + 0.5);

					vec4 sumPlain = vec4(0.0);
					vec3 sumWeightedRGB = vec3(0.0);
					float sumCoverage = 0.0;
					for (int y = 0; y < tapsY; ++y)
					{
						for (int x = 0; x < tapsX; ++x)
						{
							vec2 uv = origin + (vec2(float(x), float(y)) + 0.5) * tapStep;
							vec4 texel = texture(rgbaTexture, uv);

							float coverage = alphaMode == 2 ? (1.0 - texel.a) : texel.a;
							sumPlain += texel;
							sumWeightedRGB += texel.rgb * coverage;
							sumCoverage += coverage;
						}
					}

					if (alphaMode == 0)
					{
						// The alpha channel carries no coverage, so every channel gets a plain box
						FragColor = sumPlain / tapCount;
					}
					else
					{
						// Averaging color and alpha separately blends the empty region's color into
						// a silhouette edge, which then composites darker than it should. Weighting
						// color by coverage is the premultiplied average, decoded back into the
						// convention the layer material reads.
						vec3 color = sumCoverage > 0.0 ? sumWeightedRGB / sumCoverage : vec3(0.0);
						float coverage = sumCoverage / tapCount;

						FragColor = vec4(color, alphaMode == 2 ? (1.0 - coverage) : coverage);
					}
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbaTexture", eUniformSemantic::rgbaTexture);
		x_shaderCode->addUniform("screenSize", eUniformSemantic::screenSize);
		x_shaderCode->addUniform("floatConstant0", eUniformSemantic::floatConstant0);
		x_shaderCode->addUniform("floatConstant1", eUniformSemantic::floatConstant1);
		x_shaderCode->addUniform("floatConstant2", eUniformSemantic::floatConstant2);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getTextShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_TEXT,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				uniform vec2 screenSize;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(2.0*(aPos.x / screenSize.x) - 1.0, 1.0 - 2.0*(aPos.y / screenSize.y), 0.0, 1.0); 
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D glyphTexture;

				void main()
				{
					vec4 col = texture(glyphTexture, TexCoords);
					FragColor = col;
				} 
			)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("glyphTexture", eUniformSemantic::rgbaTexture);
		x_shaderCode->addUniform("screenSize", eUniformSemantic::screenSize);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getUnpackRGBALinearDepthTextureShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_UNPACK_RGBA_DEPTH_TEXTURE,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;
				out float gl_FragDepth;

				in vec2 TexCoords;

				uniform sampler2D rgbaPackedDepthTexture;

				void main()
				{
					// Convert rgba8 packed linear depth to linear depth float
					vec4 rgba = texture(rgbaPackedDepthTexture, TexCoords).rgba;
					float linearDepth= dot( rgba, vec4(1.0, 1/255.0, 1/65025.0, 1/16581375.0) );

					// Output linear depth to color and non-linear depth to depth buffer
					FragColor = vec4(linearDepth, linearDepth, linearDepth, 1.0);
					gl_FragDepth = linearDepth;
				} 
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbaPackedDepthTexture", eUniformSemantic::rgbaTexture);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPWireframeShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_P_WIREFRAME,
										  // vertex shader
										  R""""(
				#version 410
				uniform mat4 mvpMatrix;
				layout(location = 0) in vec3 in_position;
				void main()
				{
					gl_Position = mvpMatrix * vec4(in_position.xyz, 1);
					// Bias toward the camera so wireframe overlays win the depth
					// tie against the coincident solid mesh they are drawn over
					gl_Position.z -= 0.001 * gl_Position.w;
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 410 core
				uniform vec4 diffuseColor; 
				out vec4 out_FragColor;
				void main()
				{
					out_FragColor = diffuseColor;
				}
				)"""");
		x_shaderCode->addVertexAttribute("in_position", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addUniform("mvpMatrix", eUniformSemantic::modelViewProjectionMatrix);
		x_shaderCode->addUniform("diffuseColor", eUniformSemantic::diffuseColorRGBA);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPSolidColorShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_P_SOLID_COLOR,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec3 aPos;

				uniform mat4 mvpMatrix;

				void main()
				{
					gl_Position = mvpMatrix * vec4(aPos, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				uniform vec4 diffuseColor;

				void main()
				{    
					FragColor = diffuseColor;
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addUniform("mvpMatrix", eUniformSemantic::modelViewProjectionMatrix);
		x_shaderCode->addUniform("diffuseColor", eUniformSemantic::diffuseColorRGBA);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPCUnlitColorShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PC_UNLIT_COLOR,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec3 inPosition;
				layout (location = 1) in vec4 inColor0; 

				uniform mat4 mvpMatrix;

				out vec4 fragColor;
				void main()
				{
					fragColor = inColor0;
					gl_Position = mvpMatrix * vec4(inPosition, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				in vec4 fragColor;
				out vec4 finalColor;

				void main()
				{    
					finalColor = fragColor;
				}
				)"""");
		x_shaderCode->addVertexAttribute("inPosition", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("inColor0", eVertexDataType::datatype_vec4, eVertexSemantic::color);
		x_shaderCode->addUniform("mvpMatrix", eUniformSemantic::modelViewProjectionMatrix);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTTexturedShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_TEXTURED,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec3 aPos;
				layout (location = 1) in vec2 aTexCoords;

				uniform mat4 mvpMatrix;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = mvpMatrix * vec4(aPos, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbTexture;

				void main()
				{
					vec3 col = texture(rgbTexture, TexCoords).rgb;
					FragColor = vec4(col, 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("mvpMatrix", eUniformSemantic::modelViewProjectionMatrix);
		x_shaderCode->addUniform("rgbTexture", eUniformSemantic::rgbTexture);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPNTTexturedShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PNT_TEXTURED,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec3 aPos;
				layout(location = 1) in vec3 v3NormalIn; 
				layout (location = 2) in vec2 aTexCoords;

				uniform mat4 mvpMatrix;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = mvpMatrix * vec4(aPos, 1.0);
				}  
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbTexture;

				void main()
				{
					vec3 col = texture(rgbTexture, vec2(TexCoords.x, 1 - TexCoords.y)).rgb;
					FragColor = vec4(col, 1.0);
				} 
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("v3NormalIn", eVertexDataType::datatype_vec3, eVertexSemantic::normal);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("mvpMatrix", eUniformSemantic::modelViewProjectionMatrix);
		x_shaderCode->addUniform("rgbTexture", eUniformSemantic::diffuseTexture);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPNTTexturedColoredShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PNT_TEXTURED_LIT_COLORED,
										  // vertex shader
										  R""""(
				#version 410 
				uniform mat4 matrix; 
				layout(location = 0) in vec3 aPos; 
				layout(location = 1) in vec3 v3NormalIn; 
				layout(location = 2) in vec2 v2TexCoordsIn; 

				out vec3 Normal; // Normal for the fragment shader
				out vec2 TexCoords; // Texture coordinate for the fragment shader
				out vec3 FragPos; // Fragment position in world space

				uniform mat4 model; // Model matrix
				uniform mat4 view; // View matrix
				uniform mat4 projection; // Projection matrix

				void main() 
				{ 
					FragPos = vec3(model * vec4(aPos, 1.0)); // Position in world space
					Normal = mat3(transpose(inverse(model))) * v3NormalIn; // Transform normal to world space
					TexCoords = v2TexCoordsIn;
    
					gl_Position = projection * view * vec4(FragPos, 1.0); // Calculate the final position

				}
				)"""",
										  // fragment shader
										  R""""(
				#version 410 core

				in vec3 Normal; // Normal from the vertex shader
				in vec2 TexCoords; // Texture coordinate from the vertex shader
				in vec3 FragPos; // Position in world space from the vertex shader

				out vec4 FragColor;

				uniform sampler2D diffuse_tex;
				uniform vec3 lightDir; // The direction of the light
				uniform vec3 lightColor; // The color of the light
				uniform vec4 modelColor;
				uniform float ambientStrength;
				uniform vec3 cameraPos;
				uniform vec3 specularColor;
				uniform float shininess;

				void main()
				{
					// Ambient
					vec3 ambient = ambientStrength * lightColor;

					// Diffuse
					vec3 norm = normalize(Normal);
					vec3 lightDirNormalized = normalize(-lightDir);
					float diff = max(dot(norm, lightDirNormalized), 0.0);
					vec3 diffuse = diff * lightColor;

					// Specular
					vec3 viewDir = normalize(cameraPos - FragPos);
					vec3 reflectDir = reflect(lightDir, norm);
					float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
					vec3 specular = spec * specularColor * lightColor;

					// Combine ambient, diffuse, and specular
					vec4 lighting = vec4(ambient + diffuse + specular, 1.0);

					FragColor = texture(diffuse_tex, TexCoords) * lighting * modelColor;
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("v3NormalIn", eVertexDataType::datatype_vec3, eVertexSemantic::normal);
		x_shaderCode->addVertexAttribute("v2TexCoordsIn", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("model", eUniformSemantic::modelMatrix);
		x_shaderCode->addUniform("view", eUniformSemantic::viewMatrix);
		x_shaderCode->addUniform("projection", eUniformSemantic::projectionMatrix);
		x_shaderCode->addUniform("diffuse_tex", eUniformSemantic::diffuseTexture);
		x_shaderCode->addUniform("modelColor", eUniformSemantic::diffuseColorRGBA);
		x_shaderCode->addUniform("lightDir", eUniformSemantic::lightDirection);
		x_shaderCode->addUniform("lightColor", eUniformSemantic::lightColorRGB);
		x_shaderCode->addUniform("ambientStrength", eUniformSemantic::ambientStrength);
		x_shaderCode->addUniform("cameraPos", eUniformSemantic::cameraPosition);
		x_shaderCode->addUniform("specularColor", eUniformSemantic::specularColorRGB);
		x_shaderCode->addUniform("shininess", eUniformSemantic::specularHighlights);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPLinearDepthShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_P_LINEAR_DEPTH,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec3 aPos;

				uniform mat4 modelMatrix;
				uniform mat4 viewMatrix;
				uniform mat4 projMatrix;

				out float depth;

				void main()
				{
					mat4 mvMatrix= viewMatrix * modelMatrix;
					mat4 mvpMatrix= projMatrix * mvMatrix;

					gl_Position = mvpMatrix * vec4(aPos, 1.0);
					depth = -(mvMatrix * vec4(aPos, 1.0)).z;
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core

				uniform mat4 projMatrix;

				in float depth;

				out vec4 FragColor;
				out float gl_FragDepth;

				void main()
				{
					// Compute the zNear and zFar from projection matrix
					float A = projMatrix[2].z;
					float B = projMatrix[3].z;
					float zNear = - B / (1.0 - A);
					float zFar  =   B / (1.0 + A);

					// Compute linear depth [0, 1]
					float linearDepth = clamp((depth - zNear)/(zFar - zNear), 0.0, 1.0);

					// Output linear depth to color and depth buffers
					FragColor = vec4(linearDepth, linearDepth, linearDepth, 1.0);
					gl_FragDepth = linearDepth;
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addUniform("modelMatrix", eUniformSemantic::modelMatrix);
		x_shaderCode->addUniform("viewMatrix", eUniformSemantic::viewMatrix);
		x_shaderCode->addUniform("projMatrix", eUniformSemantic::projectionMatrix);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTVisualizeGLDepthShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		// Assumes float depth texture with non-linear depth values in [0, 1] range
		// and the perspective projection formula is using the OpenGL convention where:
		// Near plane(z = zNear) -> NDC z = -1 -> depth buffer = 0
		// Far plane(z = zFar) -> NDC z = 1 -> depth buffer = 1
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_NORMALIZE_DEPTH,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D depthTexture;
				uniform float zNear;
				uniform float zFar;

				void main()
				{
					// Sample non-linear depth from depth buffer [0, 1]
					float depth = texture(depthTexture, TexCoords).r;

					// Convert from [0, 1] depth buffer range to [-1, 1] NDC range
					float z_ndc = depth * 2.0 - 1.0;

					// Convert non-linear depth to linear eye-space depth using OpenGL perspective projection formula
					float eyeDepth = (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));

					// Normalize to [0, 1] range for visualization
					float zNorm = eyeDepth / zFar;

					FragColor = vec4(zNorm, zNorm, zNorm, 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("depthTexture", eUniformSemantic::depthTexture);
		x_shaderCode->addUniform("zNear", eUniformSemantic::zNear);
		x_shaderCode->addUniform("zFar", eUniformSemantic::zFar);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTLinearToHardwareDepthShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		// The inverse of Internal_P_LinearDepth: takes the linear [0, 1] depth that shader wrote
		// (measured between the zNear and zFar recovered from the same projection matrix) and writes
		// the hardware depth the fixed-function pipeline would have produced for that eye depth, so
		// geometry rasterized afterwards through the same projection depth tests against it.
		// Color output is a placeholder; callers mask color writes.
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_LINEAR_TO_HARDWARE_DEPTH,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;
				out float gl_FragDepth;

				in vec2 TexCoords;

				uniform sampler2D depthTexture;
				uniform mat4 projMatrix;

				void main()
				{
					// Compute the zNear and zFar from projection matrix (same as Internal_P_LinearDepth)
					float A = projMatrix[2].z;
					float B = projMatrix[3].z;
					float zNear = - B / (1.0 - A);
					float zFar  =   B / (1.0 + A);

					// Linear depth [0, 1] back to eye-space depth
					float linearDepth = texture(depthTexture, TexCoords).r;
					float eyeDepth = zNear + linearDepth * (zFar - zNear);

					// Eye-space depth to hardware depth through the OpenGL perspective mapping
					float z_ndc = (zFar + zNear - 2.0 * zNear * zFar / eyeDepth) / (zFar - zNear);
					float hardwareDepth = (linearDepth >= 1.0) ? 1.0 : clamp(z_ndc * 0.5 + 0.5, 0.0, 1.0);

					FragColor = vec4(hardwareDepth, hardwareDepth, hardwareDepth, 1.0);
					gl_FragDepth = hardwareDepth;
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("depthTexture", eUniformSemantic::depthTexture);
		x_shaderCode->addUniform("projMatrix", eUniformSemantic::projectionMatrix);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPM5544TestCardShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		// PM5544 Test Card from here: https://www.shadertoy.com/view/4ccyWH
		// CRT effects from here: https://www.shadertoy.com/view/XtlSD7
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_PM5544_TEST_CARD,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				uniform vec2  screenSize;
				uniform float time;

				#define color(a, b) ((b) ? 244. - (a) : (a)) / 255.
				const vec2 R = vec2(768, 576);

				// ---- CRT effects ------------------------------------------------

				vec2 CRTCurveUV(vec2 uv)
				{
					uv = uv * 2.0 - 1.0;
					vec2 offset = abs(uv.yx) / vec2(6.0, 4.0);
					uv = uv + uv * offset * offset;
					uv = uv * 0.5 + 0.5;
					return uv;
				}

				void DrawVignette(inout vec3 color, vec2 uv)
				{
					float vignette = uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
					vignette = clamp(pow(16.0 * vignette, 0.3), 0.0, 1.0);
					color *= vignette;
				}

				void DrawScanline(inout vec3 color, vec2 uv)
				{
					float scanline = clamp(0.95 + 0.05 * cos(3.14 * (uv.y + 0.008 * time) * 240.0), 0.0, 1.0);
					float grille   = 0.85 + 0.15 * clamp(1.5 * cos(3.14 * uv.x * 640.0), 0.0, 1.0);
					color *= scanline * grille * 1.2;
				}

				// ---- PM5544 test card -------------------------------------------

				vec3 PM5544(vec2 I)
				{
					vec3 col = vec3(122) / 255.;

					I = (I - .5) * R;

					vec2 i = I / 42. + .5,
					     p = fract(i) * 42.,
					     q = abs(I);

					if (length(I) < 252.)
					{
						int y = int(I.y) / 21,
						    i = int(ceil(I.x / 84.));

						vec2 c = vec2(61, 242) / 255.;

						if      (y < -8) col = q.x < 19. ? c.yxx : c.yyx;
						else if (y < -6) col = vec3(float(q.x > 126.));
						else if (y < -4) col = .2 * vec3(floor((I.x+sin(time)*100.0) / 84.)) + .6;
						else if (y <  0)
						{
							float w, p;
							if (i == -2) { w = 1.5;  p = 5.4; }
							if (i == -1) { w = 2.5;  p = 5.3; }
							if (i ==  0) { w = 3.5;  p = 3.3; }
							if (i ==  1) { w = 4.;   p = 0.8; }
							if (i ==  2) { w = 4.5;  p = 4.1; }
							if (i ==  3) { w = 5.25; p = 1.1; }
							col = .3625 * vec3(sin(36.8 * w * fract(I.x / 84.) + p + time*5.0) + 1.);
						}
						else if (y < 1)
						{
							col = vec3(0);
							if (p.x > 40. || p.x <  2.) col = vec3(2) / 3.;
							if (p.x > 41. || p.x <  1.) col = vec3(1);
							if (q.x < 2.) col = vec3(float(4 - int(q.x))) / 3.;
							if (abs(p.y - 21.) < 1.) col = vec3(1);
						}
						else if (q.x < 19. && y < 3) col = vec3(float(q.x < 2. ? 4 - int(q.x) : 0)) / 3.;
						else if (y < 5)
						{ 
							int i2 = int(ceil((I.x-sin(time)*75.0) / 84.));

							col = vec3(c[(5 * i2 + 3) / 7 & 1], c[int(i2 < 1)], c[i2 & 1]);
						}
						else if (y < 7) col = vec3(1) - .75 * mod(ceil((I.x+time*25.0) / 29.6), 2.);
						else if (y < 9)
						{
							col = vec3(float(q.x < 126.));
							I.x = abs(I.x + 105.);
							if (I.x < 2.) col = vec3(float(int(I.x) - 1)) / 3.;
						}
						else col = vec3(float(q.x > 84. || y > 10));
					}
					else
					{
						i = floor(i);
						if (abs(i.x) > 8. || abs(i.y) > 6.) col = vec3(float(mod(i.x + i.y, 2.) < .5));
						if (p.y > 41. || p.y < 1.) col = vec3(1);
						if (q.x > 273. && q.x < 314. && q.y < 230.)
							col = I.x > 0. ? color(vec3(122, 100, 233), I.y > 0.)
							               : color(vec3(184,  90, 122), I.y > 0.);
						if (q.x > 232. && q.x < 273. && q.y > 148. && q.y < 230.)
							col = color(vec3(157, 122, 30), I.y > 0.);
						if (q.x < 271. || q.x > 275. || q.y < 148. || q.y > 230.)
						{
							if (p.x > 40. || p.x < 2.) col = (col + 2.) / 3.;
							if (p.x > 41. || p.x < 1.) col = vec3(1);
						}
					}

					return col;
				}

				// ---- Entry point ------------------------------------------------

				void main()
				{
					FragColor = vec4(0, 0, 0, 1);

					// Flat UV [0,1] and CRT-curved UV
					vec2 uv    = gl_FragCoord.xy / screenSize;
					vec2 crtUV = CRTCurveUV(uv);

					// Black outside the curved screen edge
					if (crtUV.x < 0.0 || crtUV.x > 1.0 || crtUV.y < 0.0 || crtUV.y > 1.0)
						return;

					// Map curved UV back to PM5544 pixel space
					vec2 I = crtUV * screenSize;
					if (1.5 * abs(I.x - .5 * screenSize.x) > screenSize.y) return;
					I.x -= .5 * screenSize.x;
					I /= R * screenSize.y / R.y;
					I.x += .5;

					vec3 col = PM5544(I);

					// CRT post-processing (vignette uses curved UV, scanlines use flat UV)
					DrawVignette(col, crtUV);
					DrawScanline(col, uv);

					FragColor = vec4(col, 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("screenSize", eUniformSemantic::screenSize);
		x_shaderCode->addUniform("time", eUniformSemantic::floatConstant0);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPConeVolumeShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_P_CONE_VOLUME,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec3 aPos;

				uniform mat4 mvpMatrix;

				out float vertZ;

				void main()
				{
					vertZ = aPos.z;
					gl_Position = mvpMatrix * vec4(aPos, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in float vertZ;

				uniform vec4 diffuseColor;

				void main()
				{
					// Bake intensity fade into RGB so it works with additive (ONE, ONE) blending.
					// fade = 1 at tip (z=0), 0 at base (z=-1).
					float fade = clamp(1.0 + vertZ, 0.0, 1.0);
					FragColor = vec4(diffuseColor.rgb * (diffuseColor.a * fade), 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addUniform("mvpMatrix", eUniformSemantic::modelViewProjectionMatrix);
		x_shaderCode->addUniform("diffuseColor", eUniformSemantic::diffuseColorRGBA);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPSHEnvironmentShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		// Draws an order-2 spherical harmonic environment on a mesh, evaluating
		// radiance along the direction from the mesh's origin to the fragment.
		// Intended for a unit sphere, where the object-space position IS that
		// direction - so the mesh must be drawn with an unrotated transform for
		// the environment (which is world space) to line up.
		//
		// This is the editor twin of the Unreal skydome material, and uses the
		// same basis constants as sh_eval_basis in SphericalHarmonics.cpp.
		// Unreal needs a Y/Z swap because its space is a handedness flip away;
		// here the direction is already Mikan space, so there is no swap.
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_P_SH_ENVIRONMENT,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec3 aPos;

				uniform mat4 mvpMatrix;

				out vec3 vertDirection;

				void main()
				{
					vertDirection = aPos;
					gl_Position = mvpMatrix * vec4(aPos, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec3 vertDirection;

				uniform vec3 shCoefficient0;
				uniform vec3 shCoefficient1;
				uniform vec3 shCoefficient2;
				uniform vec3 shCoefficient3;
				uniform vec3 shCoefficient4;
				uniform vec3 shCoefficient5;
				uniform vec3 shCoefficient6;
				uniform vec3 shCoefficient7;
				uniform vec3 shCoefficient8;

				void main()
				{
					vec3 n = normalize(vertDirection);

					vec3 radiance =
						  0.282095 * shCoefficient0
						+ 0.488603 * (n.y * shCoefficient1 + n.z * shCoefficient2 + n.x * shCoefficient3)
						+ 1.092548 * (n.x * n.y) * shCoefficient4
						+ 1.092548 * (n.y * n.z) * shCoefficient5
						+ 0.315392 * (3.0 * n.z * n.z - 1.0) * shCoefficient6
						+ 1.092548 * (n.x * n.z) * shCoefficient7
						+ 0.546274 * (n.x * n.x - n.y * n.y) * shCoefficient8;

					// Order-2 SH rings negative around sharp lights - that is
					// inherent to the representation, not a bad fit - so clamp
					// exactly as every other consumer of this environment does.
					FragColor = vec4(max(radiance, vec3(0.0)), 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec3, eVertexSemantic::position);
		x_shaderCode->addUniform("mvpMatrix", eUniformSemantic::modelViewProjectionMatrix);
		x_shaderCode->addUniform("shCoefficient0", eUniformSemantic::shCoefficient0);
		x_shaderCode->addUniform("shCoefficient1", eUniformSemantic::shCoefficient1);
		x_shaderCode->addUniform("shCoefficient2", eUniformSemantic::shCoefficient2);
		x_shaderCode->addUniform("shCoefficient3", eUniformSemantic::shCoefficient3);
		x_shaderCode->addUniform("shCoefficient4", eUniformSemantic::shCoefficient4);
		x_shaderCode->addUniform("shCoefficient5", eUniformSemantic::shCoefficient5);
		x_shaderCode->addUniform("shCoefficient6", eUniformSemantic::shCoefficient6);
		x_shaderCode->addUniform("shCoefficient7", eUniformSemantic::shCoefficient7);
		x_shaderCode->addUniform("shCoefficient8", eUniformSemantic::shCoefficient8);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTLinearizeRGBShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		// Converts a non-linear (gamma/sRGB encoded) RGB texture to linear color space.
		// The transferFunction uniform carries the integer value of the eVideoTransferFunction
		// enum (from IVideoDevice.h) as a float. Only a pragmatic subset is handled; anything
		// unrecognized (HLG, PQ, log, INVALID, etc.) falls back to an sRGB decode.
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_LINEARIZE_RGB,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbTexture;
				uniform float transferFunction;

				// eVideoTransferFunction enum values (see IVideoDevice.h)
				const int TF_NoGamma   = 0;
				const int TF_Gamma_1_0 = 1;
				const int TF_Gamma_1_8 = 2;
				const int TF_Gamma_2_0 = 3;
				const int TF_Gamma_2_2 = 4;
				const int TF_Gamma_2_6 = 5;
				const int TF_Gamma_2_8 = 6;
				const int TF_BT709     = 8;
				const int TF_SRGB      = 14;

				vec3 srgbToLinear(vec3 c)
				{
					vec3 lo = c / 12.92;
					vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
					return mix(lo, hi, step(vec3(0.04045), c));
				}

				vec3 bt709ToLinear(vec3 c)
				{
					vec3 lo = c / 4.5;
					vec3 hi = pow((c + 0.099) / 1.099, vec3(1.0 / 0.45));
					return mix(lo, hi, step(vec3(0.081), c));
				}

				void main()
				{
					vec3 encoded = texture(rgbTexture, TexCoords).rgb;
					int tf = int(floor(transferFunction + 0.5));

					vec3 linearColor;
					if (tf == TF_Gamma_1_0 || tf == TF_NoGamma)
						linearColor = encoded; // already linear
					else if (tf == TF_BT709)
						linearColor = bt709ToLinear(encoded);
					else if (tf == TF_Gamma_1_8)
						linearColor = pow(encoded, vec3(1.8));
					else if (tf == TF_Gamma_2_0)
						linearColor = pow(encoded, vec3(2.0));
					else if (tf == TF_Gamma_2_2)
						linearColor = pow(encoded, vec3(2.2));
					else if (tf == TF_Gamma_2_6)
						linearColor = pow(encoded, vec3(2.6));
					else if (tf == TF_Gamma_2_8)
						linearColor = pow(encoded, vec3(2.8));
					else // TF_SRGB and all unrecognized values fall back to sRGB decode
						linearColor = srgbToLinear(encoded);

					FragColor = vec4(linearColor, 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbTexture", eUniformSemantic::rgbTexture);
		x_shaderCode->addUniform("transferFunction", eUniformSemantic::floatConstant0);
	}

	return x_shaderCode;
}

IMkShaderCodeConstPtr getPTLinearToSRGBShaderCode()
{
	static IMkShaderCodePtr x_shaderCode= nullptr;

	if (x_shaderCode == nullptr)
	{
		// Converts a linear RGB texture back to sRGB encoding (plain gamma encode, no tonemapping).
		x_shaderCode= createIMkShaderCode(INTERNAL_MATERIAL_PT_LINEAR_TO_SRGB,
										  // vertex shader
										  R""""(
				#version 330 core
				layout (location = 0) in vec2 aPos;
				layout (location = 1) in vec2 aTexCoords;

				out vec2 TexCoords;

				void main()
				{
					TexCoords = aTexCoords;
					gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
				}
				)"""",
										  // fragment shader
										  R""""(
				#version 330 core
				out vec4 FragColor;

				in vec2 TexCoords;

				uniform sampler2D rgbTexture;

				vec3 linearToSrgb(vec3 c)
				{
					c = clamp(c, 0.0, 1.0);
					vec3 lo = c * 12.92;
					vec3 hi = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
					return mix(lo, hi, step(vec3(0.0031308), c));
				}

				void main()
				{
					vec3 linearColor = texture(rgbTexture, TexCoords).rgb;
					FragColor = vec4(linearToSrgb(linearColor), 1.0);
				}
				)"""");
		x_shaderCode->addVertexAttribute("aPos", eVertexDataType::datatype_vec2, eVertexSemantic::position);
		x_shaderCode->addVertexAttribute("aTexCoords", eVertexDataType::datatype_vec2, eVertexSemantic::texCoord);
		x_shaderCode->addUniform("rgbTexture", eUniformSemantic::rgbTexture);
	}

	return x_shaderCode;
}

bool registerInternalShaders(IMkShaderCache* shaderCache)
{
	std::vector<IMkShaderCodeConstPtr> internalShaders= {
		getPTTexturedFullScreenRGBQuad(),
		getPTUndistortTexturedFullScreenRGBQuad(),
		getPTConvertNV12ToRGBAQuad(),
		getPTTexturedFullScreenRGBAQuad(),
		getPTResolveDownsampleRGBQuad(),
		getPTResolveDownsampleRGBAQuad(),
		getTextShaderCode(),
		getUnpackRGBALinearDepthTextureShaderCode(),
		getPWireframeShaderCode(),
		getPSolidColorShaderCode(),
		getPCUnlitColorShaderCode(),
		getPTTexturedShaderCode(),
		getPNTTexturedShaderCode(),
		getPNTTexturedColoredShaderCode(),
		getPLinearDepthShaderCode(),
		getPTVisualizeGLDepthShaderCode(),
		getPTLinearToHardwareDepthShaderCode(),
		getPM5544TestCardShaderCode(),
		getPConeVolumeShaderCode(),
		getPSHEnvironmentShaderCode(),
		getPTLinearizeRGBShaderCode(),
		getPTLinearToSRGBShaderCode(),
	};

	bool bSuccess= true;
	for (IMkShaderCodeConstPtr code : internalShaders)
	{
		if (!shaderCache->registerMaterial(code))
		{
			MIKAN_LOG_ERROR("InternalShaders::registerInternalShaders()")
				<< "Failed to compile " << code->getProgramName();
			bSuccess= false;
		}
	}

	return bSuccess;
}
}; // namespace InternalShaders

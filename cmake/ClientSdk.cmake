# ClientSdk.cmake
#
# Installs the client SDK: what a client application compiles, links, and loads to talk to
# Mikan, and nothing else. It lands in its own tree beside the app install so a client can
# copy it directory by directory without picking files out of the editor's payload.
#
#   dist/ClientSDK/Win64/
#     include/              the headers named in MIKAN_CLIENT_SDK_HEADERS
#     lib/                  import libraries for the DLLs those headers export from
#     bin/                  MikanClientAPI.dll and its runtime dependency closure
#     bindings/csharp/      MikanClientCSharp.dll and its references (Visual Studio builds only)
#     bindings/typescript/  the compiled @mikanxr/client package (when npm is available)
#     symbols/              PDBs for the Mikan DLLs in bin/
#     version.txt
#
# INSTALL installs the SDK along with everything else. INSTALL_CLIENT_SDK builds and installs
# the SDK alone:
#
#   cmake --build build --target INSTALL_CLIENT_SDK --config Release
#
# Every rule belongs to one of two components. ClientSDK clears the tree and installs
# everything but the C# bindings. ClientSDKCSharp adds the C# bindings to an existing tree, so
# a build that only produces the assembly can add it to a tree another build installed without
# replacing that tree's DLLs. INSTALL_CLIENT_SDK installs both, in that order.
#
# Every rule lives in this file, and nothing reaches the SDK without being named here. Keep
# the lists below in step with the client API: a header a client needs that is missing from
# MIKAN_CLIENT_SDK_HEADERS fails the ClientSdkConsumer build.

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(MIKAN_CLIENT_SDK_INSTALL_PATH "${MIKAN_ROOT_INSTALL_PATH}/ClientSDK/${ARCH_LABEL}")

	# The native client surface. All of MikanClientAPI/Public, plus the headers of the two
	# libraries it reaches into. MikanCoreCAPI.h is the core C API the C# bindings call, and
	# MikanCoreVulkanTypes.h needs the client's own Vulkan headers.
	set(_sdk_client_api_dir "${ROOT_DIR}/src/Libraries/MikanClientAPI/Public")
	set(_sdk_client_core_dir "${ROOT_DIR}/src/Libraries/MikanClientCore/Public")
	set(_sdk_serialization_dir "${ROOT_DIR}/src/Libraries/MikanSerialization/Public")
	set(MIKAN_CLIENT_SDK_HEADERS
		"${_sdk_client_api_dir}/MikanAPI.h"
		"${_sdk_client_api_dir}/MikanAPIExport.h"
		"${_sdk_client_api_dir}/MikanAPITypes.h"
		"${_sdk_client_api_dir}/MikanAnchorTypes.h"
		"${_sdk_client_api_dir}/MikanCameraEvents.h"
		"${_sdk_client_api_dir}/MikanCameraRequests.h"
		"${_sdk_client_api_dir}/MikanCameraTypes.h"
		"${_sdk_client_api_dir}/MikanClientEvents.h"
		"${_sdk_client_api_dir}/MikanClientRequests.h"
		"${_sdk_client_api_dir}/MikanClientTypes.h"
		"${_sdk_client_api_dir}/MikanComponentTypes.h"
		"${_sdk_client_api_dir}/MikanCompositorEvents.h"
		"${_sdk_client_api_dir}/MikanCompositorTypes.h"
		"${_sdk_client_api_dir}/MikanEditorTypes.h"
		"${_sdk_client_api_dir}/MikanFunctionRequests.h"
		"${_sdk_client_api_dir}/MikanFunctionTypes.h"
		"${_sdk_client_api_dir}/MikanLightEvents.h"
		"${_sdk_client_api_dir}/MikanLightRequests.h"
		"${_sdk_client_api_dir}/MikanLightTypes.h"
		"${_sdk_client_api_dir}/MikanMarkerRequests.h"
		"${_sdk_client_api_dir}/MikanMarkerTypes.h"
		"${_sdk_client_api_dir}/MikanMathTypes.h"
		"${_sdk_client_api_dir}/MikanPropertyEvents.h"
		"${_sdk_client_api_dir}/MikanPropertyRequests.h"
		"${_sdk_client_api_dir}/MikanPropertyTypes.h"
		"${_sdk_client_api_dir}/MikanRemoteControlEvents.h"
		"${_sdk_client_api_dir}/MikanRemoteControlRequests.h"
		"${_sdk_client_api_dir}/MikanRemoteControlTypes.h"
		"${_sdk_client_api_dir}/MikanRenderTargetRequests.h"
		"${_sdk_client_api_dir}/MikanResponseFuture.h"
		"${_sdk_client_api_dir}/MikanSceneTypes.h"
		"${_sdk_client_api_dir}/MikanScriptEvents.h"
		"${_sdk_client_api_dir}/MikanScriptRequests.h"
		"${_sdk_client_api_dir}/MikanScriptTypes.h"
		"${_sdk_client_api_dir}/MikanShapeRequests.h"
		"${_sdk_client_api_dir}/MikanShapeTypes.h"
		"${_sdk_client_api_dir}/MikanStageTypes.h"
		"${_sdk_client_api_dir}/MikanStencilRequests.h"
		"${_sdk_client_api_dir}/MikanStencilTypes.h"
		"${_sdk_client_api_dir}/MikanTextureSourceEvents.h"
		"${_sdk_client_api_dir}/MikanTextureSourceTypes.h"
		"${_sdk_client_api_dir}/MikanTrackingMountTypes.h"
		"${_sdk_client_api_dir}/MikanTrackingVolumeTypes.h"
		"${_sdk_client_api_dir}/MikanTransformTypes.h"
		"${_sdk_client_api_dir}/MikanTypeFwd.h"
		"${_sdk_client_api_dir}/MikanVRDeviceTypes.h"
		"${_sdk_client_api_dir}/MikanVariantTypes.h"
		"${_sdk_client_api_dir}/MikanVideoSourceEvents.h"
		"${_sdk_client_api_dir}/MikanVideoSourceRequests.h"
		"${_sdk_client_api_dir}/MikanVideoSourceTypes.h"
		"${_sdk_client_core_dir}/MikanCoreCAPI.h"
		"${_sdk_client_core_dir}/MikanCoreExport.h"
		"${_sdk_client_core_dir}/MikanCoreTypes.h"
		"${_sdk_client_core_dir}/MikanCoreVulkanTypes.h"
		"${_sdk_serialization_dir}/SerializableList.h"
		"${_sdk_serialization_dir}/SerializableMap.h"
		"${_sdk_serialization_dir}/SerializableObjectPtr.h"
		"${_sdk_serialization_dir}/SerializableString.h"
		"${_sdk_serialization_dir}/SerializationExport.h"
		"${_sdk_serialization_dir}/SerializationProperty.h")

	# The DLLs the SDK headers export from, so a client links their import libraries
	set(MIKAN_CLIENT_SDK_LINKED_TARGETS
		MikanClientAPI
		MikanClientCore
		MikanSerialization)

	# The rest of MikanClientAPI.dll's import closure that this build produces
	set(MIKAN_CLIENT_SDK_RUNTIME_TARGETS
		MikanCoreApp
		MikanSharedTexture
		MikanUtility)

	# Prebuilt DLLs in that closure. MikanSharedTexture.dll also imports D3DCOMPILER_47.dll,
	# which Windows ships in System32, so it is left out.
	set(MIKAN_CLIENT_SDK_RUNTIME_FILES
		"${RFK_SHARED_LIBRARIES}"
		"${GLEW_SHARED_LIBRARIES}"
		"${SPOUT2_SHARED_LIBRARIES}")

	# The managed assembly's references, copied beside it in its output folder by the build
	set(MIKAN_CLIENT_SDK_CSHARP_REFERENCES
		Newtonsoft.Json.dll
		System.Buffers.dll
		System.Memory.dll
		System.Numerics.Vectors.dll
		System.Runtime.CompilerServices.Unsafe.dll)

	# Installing over an earlier SDK would keep files these lists have since dropped, so the
	# native component starts from an empty tree. Install rules run in declaration order, and
	# this is the first one that writes the SDK.
	install(CODE "file(REMOVE_RECURSE \"${MIKAN_CLIENT_SDK_INSTALL_PATH}\")"
		COMPONENT ClientSDK)

	install(FILES ${MIKAN_CLIENT_SDK_HEADERS}
		DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/include"
		COMPONENT ClientSDK)

	# install(FILES) rather than install(TARGETS): the latter falls back to default
	# destinations under CMAKE_INSTALL_PREFIX for any artifact kind it is not given one for,
	# which would scatter each library's PUBLIC_HEADER set outside the SDK.
	foreach(target IN LISTS MIKAN_CLIENT_SDK_LINKED_TARGETS)
		install(FILES $<TARGET_LINKER_FILE:${target}>
			DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/lib"
			COMPONENT ClientSDK)
	endforeach()

	foreach(target IN LISTS MIKAN_CLIENT_SDK_LINKED_TARGETS MIKAN_CLIENT_SDK_RUNTIME_TARGETS)
		install(FILES $<TARGET_FILE:${target}>
			DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/bin"
			COMPONENT ClientSDK)
		install(FILES $<TARGET_PDB_FILE:${target}>
			DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/symbols"
			COMPONENT ClientSDK
			OPTIONAL)
	endforeach()

	install(FILES ${MIKAN_CLIENT_SDK_RUNTIME_FILES}
		DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/bin"
		COMPONENT ClientSDK)

	file(WRITE "${CMAKE_BINARY_DIR}/ClientSDK/version.txt" "${MIKAN_PROJECT_NAME}")
	install(FILES "${CMAKE_BINARY_DIR}/ClientSDK/version.txt"
		DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}"
		COMPONENT ClientSDK)

	# Only defined when npm was found. The package is its manifest and the compiled output,
	# minus the compiled tests.
	if(TARGET MikanClientTypeScript)
		install(FILES "${ROOT_DIR}/bindings/typescript/package.json"
			DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/bindings/typescript"
			COMPONENT ClientSDK)
		install(DIRECTORY "${ROOT_DIR}/bindings/typescript/dist"
			DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/bindings/typescript"
			COMPONENT ClientSDK
			PATTERN "tests" EXCLUDE)
	endif()

	# Only defined under a Visual Studio generator
	if(TARGET MikanClientCSharp)
		install(FILES $<TARGET_FILE:MikanClientCSharp>
			DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/bindings/csharp"
			COMPONENT ClientSDKCSharp)
		foreach(reference IN LISTS MIKAN_CLIENT_SDK_CSHARP_REFERENCES)
			install(FILES "$<TARGET_FILE_DIR:MikanClientCSharp>/${reference}"
				DESTINATION "${MIKAN_CLIENT_SDK_INSTALL_PATH}/bindings/csharp"
				COMPONENT ClientSDKCSharp)
		endforeach()
	endif()

	set(_sdk_install_commands
		COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --config $<CONFIG> --component ClientSDK)
	if(TARGET MikanClientCSharp)
		list(APPEND _sdk_install_commands
			COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --config $<CONFIG> --component ClientSDKCSharp)
	endif()

	add_custom_target(INSTALL_CLIENT_SDK
		${_sdk_install_commands}
		COMMENT "Installing the client SDK to ${MIKAN_CLIENT_SDK_INSTALL_PATH}")
	add_dependencies(INSTALL_CLIENT_SDK
		${MIKAN_CLIENT_SDK_LINKED_TARGETS}
		${MIKAN_CLIENT_SDK_RUNTIME_TARGETS}
		Refureku)
	if(TARGET MikanClientTypeScript)
		add_dependencies(INSTALL_CLIENT_SDK MikanClientTypeScript)
	endif()
	if(TARGET MikanClientCSharp)
		add_dependencies(INSTALL_CLIENT_SDK MikanClientCSharp)
	endif()
	set_target_properties(INSTALL_CLIENT_SDK PROPERTIES FOLDER "Packaging")
ENDIF()

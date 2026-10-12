# Client SDK consumer
#
# A standalone project, never added to the main build: it builds against an installed client
# SDK exactly as a client would, with that tree's include/ and lib/ and nothing from the
# MikanXR source tree. Each installed header compiles in a translation unit of its own, and the
# executable links every SDK import library, so a header or import library missing from the
# SDK fails this build. The executable has no reason to run.
#
#   cmake --build build --target INSTALL_CLIENT_SDK --config Release
#   cmake -S src/Programs/Tests/ClientSdkConsumer -B build/ClientSdkConsumer
#   cmake --build build/ClientSdkConsumer --config Release

cmake_minimum_required(VERSION 3.15)
project(ClientSdkConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(MIKAN_CLIENT_SDK_DIR "${_repo_root}/dist/ClientSDK/Win64"
	CACHE PATH "The installed client SDK tree to build against")
# MikanCoreVulkanTypes.h includes the Vulkan API headers, which a client supplies itself
set(MIKAN_CLIENT_SDK_VULKAN_INCLUDE_DIR "${_repo_root}/thirdparty/Vulkan-Headers/include"
	CACHE PATH "The Vulkan API headers the SDK's Vulkan types need")

if(NOT EXISTS "${MIKAN_CLIENT_SDK_DIR}/include/MikanAPI.h")
	message(FATAL_ERROR "No client SDK at ${MIKAN_CLIENT_SDK_DIR}: build INSTALL_CLIENT_SDK first, or set MIKAN_CLIENT_SDK_DIR")
endif()

set(_sdk_libraries MikanClientAPI MikanClientCore MikanSerialization)
foreach(library IN LISTS _sdk_libraries)
	add_library(mikan::${library} SHARED IMPORTED)
	set_target_properties(mikan::${library} PROPERTIES
		IMPORTED_LOCATION "${MIKAN_CLIENT_SDK_DIR}/bin/${library}.dll"
		IMPORTED_IMPLIB "${MIKAN_CLIENT_SDK_DIR}/lib/${library}.lib"
		INTERFACE_INCLUDE_DIRECTORIES "${MIKAN_CLIENT_SDK_DIR}/include")
endforeach()

# One translation unit per installed header, so each is proven to stand on its own
file(GLOB _sdk_headers CONFIGURE_DEPENDS "${MIKAN_CLIENT_SDK_DIR}/include/*.h")
set(_header_units)
foreach(header IN LISTS _sdk_headers)
	get_filename_component(_header_name "${header}" NAME)
	get_filename_component(_header_stem "${header}" NAME_WE)
	set(_header_unit "${CMAKE_CURRENT_BINARY_DIR}/HeaderUnits/${_header_stem}.cpp")
	file(CONFIGURE OUTPUT "${_header_unit}" CONTENT "#include \"${_header_name}\"\n")
	list(APPEND _header_units "${_header_unit}")
endforeach()

add_executable(ClientSdkConsumer
	"${CMAKE_CURRENT_LIST_DIR}/ClientSdkConsumer.cpp"
	${_header_units})
target_include_directories(ClientSdkConsumer PRIVATE "${MIKAN_CLIENT_SDK_VULKAN_INCLUDE_DIR}")
target_link_libraries(ClientSdkConsumer PRIVATE mikan::MikanClientAPI mikan::MikanClientCore mikan::MikanSerialization)

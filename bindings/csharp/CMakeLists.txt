PROJECT(MikanClientCSharp VERSION 1.0.0.0 LANGUAGES CSharp)
INCLUDE(CSharpUtilities)

# Create assembly info with current version.
CONFIGURE_FILE("${CMAKE_SOURCE_DIR}/bindings/csharp/AssemblyInfo.cs.template" "${CMAKE_BINARY_DIR}/Config/${PROJECT_NAME}/AssemblyInfo.cs")

# TODO: Remove need to check in generated files
# See https://crascit.com/2017/04/18/generated-sources-in-cmake-builds/
file(GLOB MIKANCLIENT_CSHARP_SRC
	"${CMAKE_CURRENT_LIST_DIR}/*.cs"
	"${CMAKE_CURRENT_LIST_DIR}/Generated/*.cs"
	"${CMAKE_CURRENT_LIST_DIR}/Native/*.cs"
	"${CMAKE_CURRENT_LIST_DIR}/Properties/*.cs"
	"${CMAKE_CURRENT_LIST_DIR}/Serialization/*.cs"
)

ADD_LIBRARY(${PROJECT_NAME} SHARED
    "${CMAKE_BINARY_DIR}/Config/${PROJECT_NAME}/AssemblyInfo.cs"
    ${MIKANCLIENT_CSHARP_SRC}
)

# Find Nuget (install the latest CLI here: https://www.nuget.org/downloads).
find_program(NUGET nuget PATHS ${NUGET_PATH})
if(NOT NUGET)
    message(FATAL "CMake could not find the nuget command line tool. Please install it!")
else()
    # Copy the Nuget config file from source location to the CMake build directory.
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/packages.config.template 
	${CMAKE_CURRENT_BINARY_DIR}/packages.config COPYONLY)
    # Run Nuget using the .config file to installing any missing dependencies to the build directory.
	execute_process(COMMAND 
        ${NUGET} restore ${CMAKE_CURRENT_BINARY_DIR}/packages.config -SolutionDirectory ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Provide the path to the Nuget-installed references, in case this is a fresh project build.
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    VS_DOTNET_REFERENCE_Newtonsoft.Json 
    ${CMAKE_BINARY_DIR}/packages/Newtonsoft.Json.4.0.3/lib/net40/Newtonsoft.Json.dll)
	
# Set designer and XAML properties.
CSHARP_SET_DESIGNER_CS_PROPERTIES(
    "${CMAKE_BINARY_DIR}/Config/${PROJECT_NAME}/AssemblyInfo.cs"
)

# Set CLR assembly properties.
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DOTNET_REFERENCES "System;System.Core;System.Memory"
    VS_GLOBAL_ROOTNAMESPACE ${PROJECT_NAME}
)

# Put in an "ClientAPI" folder in the project viewer
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ClientAPI)

# Create a batch file wrapper to set PATH and run the code generator
set (CODEGEN_EXE_DIR $<TARGET_FILE_DIR:MikanClientCodeGen>)
set (CODEGEN_EXE $<TARGET_FILE_DIR:MikanClientCodeGen>/$<TARGET_NAME:MikanClientCodeGen>)
set (CODEGEN_WRAPPER ${CMAKE_CURRENT_BINARY_DIR}/run_csharp_codegen_$<CONFIG>.bat)
file(GENERATE OUTPUT ${CODEGEN_WRAPPER} CONTENT "@echo off
set PATH=${CODEGEN_EXE_DIR};%PATH%
\"${CODEGEN_EXE}\" %*
")

add_custom_target(MikanCSharpCodeGen
					WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
					COMMAND "${CODEGEN_WRAPPER}" "${CMAKE_CURRENT_LIST_DIR}/CSharpCodeGenConfig.json") 
set_target_properties(MikanCSharpCodeGen PROPERTIES FOLDER Programs)
add_dependencies(MikanCSharpCodeGen MikanClientCodeGen)

# Run the RefurekuGenerator BEFORE building the project to refresh generated files
add_dependencies(${PROJECT_NAME} MikanCSharpCodeGen)

# Install beside MikanClientTestCSharp, which loads the assembly from its own folder.
# Clients take it from the client SDK (cmake/ClientSdk.cmake).
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  install(TARGETS ${PROJECT_NAME} 
	  EXPORT ${PROJECT_NAME}Config
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()

# Step into the subdirectories
MESSAGE(STATUS "Stepping into Tests")
add_subdirectory(Tests)
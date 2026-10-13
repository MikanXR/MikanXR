PROJECT(MikanClientTestCSharp VERSION 1.0.0.0 LANGUAGES CSharp)
INCLUDE(CSharpUtilities)

# Create assembly info with current version.
CONFIGURE_FILE("${CMAKE_CURRENT_LIST_DIR}/AssemblyInfo.cs.template" "${CMAKE_BINARY_DIR}/Config/${PROJECT_NAME}/AssemblyInfo.cs")

file(GLOB MIKAN_CSHARP_TEST_SRC
	"${CMAKE_CURRENT_LIST_DIR}/*.cs"
	"${CMAKE_CURRENT_LIST_DIR}/Properties/*.cs"
	"${CMAKE_CURRENT_LIST_DIR}/Shared/*.cs"
	"${CMAKE_CURRENT_LIST_DIR}/DirectX/*.cs"
)

ADD_EXECUTABLE(${PROJECT_NAME}
    "${CMAKE_BINARY_DIR}/Config/${PROJECT_NAME}/AssemblyInfo.cs"
    ${MIKAN_CSHARP_TEST_SRC}
)

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES FOLDER Test)

# Set designer and XAML properties.
CSHARP_SET_DESIGNER_CS_PROPERTIES(
    "${CMAKE_BINARY_DIR}/Config/${PROJECT_NAME}/AssemblyInfo.cs"
)

# Define dependencies.
TARGET_LINK_LIBRARIES(${PROJECT_NAME} 
    PUBLIC MikanClientCSharp
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

# Set the .NET Framework version for the executable
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2")

# Provide the path to the Nuget-installed references, in case this is a fresh project build.
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    VS_DOTNET_REFERENCE_SharpDX 
    ${CMAKE_BINARY_DIR}/packages/SharpDX.4.2.0/lib/net45/SharpDX.dll)
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    VS_DOTNET_REFERENCE_SharpDX.D3DCompiler
    ${CMAKE_BINARY_DIR}/packages/SharpDX.D3DCompiler.4.2.0/lib/net45/SharpDX.D3DCompiler.dll)
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    VS_DOTNET_REFERENCE_SharpDX.Desktop
    ${CMAKE_BINARY_DIR}/packages/SharpDX.Desktop.4.2.0/lib/net45/SharpDX.Desktop.dll)
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    VS_DOTNET_REFERENCE_SharpDX.Direct3D11
    ${CMAKE_BINARY_DIR}/packages/SharpDX.Direct3D11.4.2.0/lib/net45/SharpDX.Direct3D11.dll)
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    VS_DOTNET_REFERENCE_SharpDX.DXGI
    ${CMAKE_BINARY_DIR}/packages/SharpDX.DXGI.4.2.0/lib/net45/SharpDX.DXGI.dll)
set_property(TARGET ${PROJECT_NAME} PROPERTY 
    VS_DOTNET_REFERENCE_SharpDX.Mathematics
    ${CMAKE_BINARY_DIR}/packages/SharpDX.Mathematics.4.2.0/lib/net45/SharpDX.Mathematics.dll)

# Set CLR assembly properties.
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DOTNET_REFERENCES 
	"System"
	"System.Core"
	"System.Drawing"
	"System.Windows.Forms"
	"System.Xml.Linq"
	"System.Data.DataSetExtensions"
	"Microsoft.CSharp"
	"System.Data"
	"System.Xml"
)

# Runtime dependencies
set(EASY_PROFILER_FOLDER ${ROOT_DIR}/deps/easy_profiler/bin)

# Post build - copy runtime dependencies to binary build folder (for debugging)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set_property(TARGET MikanClientTestCSharp PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:MikanClientTestCSharp>")

  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${SPOUT2_SHARED_LIBRARIES}" $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${GLEW_SHARED_LIBRARIES} $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanClientCore>/MikanClientCore.dll
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanClientAPI>/MikanClientAPI.dll
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanCoreApp>/MikanCoreApp.dll
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanRenderer>/MikanRenderer.dll
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanSerialization>/MikanSerialization.dll
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanSharedTexture>/MikanSharedTexture.dll
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:MikanUtility>/MikanUtility.dll
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          "${EASY_PROFILER_FOLDER}/easy_profiler.dll"
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
  add_custom_command(
    TARGET MikanClientTestCSharp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
          "${RFK_SHARED_LIBRARIES}"
          $<TARGET_FILE_DIR:MikanClientTestCSharp>)
ELSE() #Linux/Darwin
ENDIF()

# Install
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows") 
  install(TARGETS ${PROJECT_NAME} 
	  EXPORT ${PROJECT_NAME}Config
      RUNTIME DESTINATION ${MIKAN_ARCH_INSTALL_PATH})
ELSE() #Linux/Darwin
ENDIF()
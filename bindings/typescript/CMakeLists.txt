PROJECT(MikanClientTypeScript VERSION 1.0.0.0 LANGUAGES NONE)

# Collect all TypeScript source files for display in Visual Studio
file(GLOB MIKANCLIENT_TYPESCRIPT_ROOT_SRC
	"${CMAKE_CURRENT_LIST_DIR}/*.ts"
)

file(GLOB MIKANCLIENT_TYPESCRIPT_SERIALIZATION_SRC
	"${CMAKE_CURRENT_LIST_DIR}/Serialization/*.ts"
)

file(GLOB MIKANCLIENT_TYPESCRIPT_GENERATED_SRC
	"${CMAKE_CURRENT_LIST_DIR}/types/*.ts"
)

file(GLOB MIKANCLIENT_TYPESCRIPT_TESTS_SRC
	"${CMAKE_CURRENT_LIST_DIR}/tests/*.ts"
)

# Combine all source files
set(MIKANCLIENT_TYPESCRIPT_ALL_SRC
	${MIKANCLIENT_TYPESCRIPT_ROOT_SRC}
	${MIKANCLIENT_TYPESCRIPT_SERIALIZATION_SRC}
	${MIKANCLIENT_TYPESCRIPT_GENERATED_SRC}
	${MIKANCLIENT_TYPESCRIPT_TESTS_SRC}
)

# Set up source groups for Visual Studio
source_group("Client" FILES ${MIKANCLIENT_TYPESCRIPT_ROOT_SRC})
source_group("Serialization" FILES ${MIKANCLIENT_TYPESCRIPT_SERIALIZATION_SRC})
source_group("Generated" FILES ${MIKANCLIENT_TYPESCRIPT_GENERATED_SRC})
source_group("Tests" FILES ${MIKANCLIENT_TYPESCRIPT_TESTS_SRC})

# Find Node.js and npm
find_program(NPM_EXECUTABLE npm)
if(NOT NPM_EXECUTABLE)
    message(WARNING "npm was not found. TypeScript bindings will not be built automatically.")
    message(WARNING "Please install Node.js to enable TypeScript compilation.")
    return()
endif()

# Create the command to run the code generator
set(CODEGEN_EXE_DIR $<TARGET_FILE_DIR:MikanClientCodeGen>)
set(CODEGEN_EXE $<TARGET_FILE_DIR:MikanClientCodeGen>/$<TARGET_NAME:MikanClientCodeGen>)
set(CODEGEN_WRAPPER ${CMAKE_CURRENT_BINARY_DIR}/run_typescript_codegen_$<CONFIG>.bat)

file(GENERATE OUTPUT ${CODEGEN_WRAPPER} CONTENT "@echo off
set PATH=${CODEGEN_EXE_DIR};%PATH%
\"${CODEGEN_EXE}\" %*
")

add_custom_target(MikanTypeScriptCodeGen
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    COMMAND "${CODEGEN_WRAPPER}" "${CMAKE_CURRENT_LIST_DIR}/TypeScriptCodeGenConfig.json"
    COMMENT "Generating TypeScript bindings..."
)
set_target_properties(MikanTypeScriptCodeGen PROPERTIES FOLDER Programs)
add_dependencies(MikanTypeScriptCodeGen MikanClientCodeGen)

# Create a custom target for npm install
add_custom_target(MikanTypeScriptNpmInstall
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    COMMAND ${NPM_EXECUTABLE} install
    COMMENT "Installing TypeScript dependencies..."
)
add_dependencies(MikanTypeScriptNpmInstall MikanTypeScriptCodeGen)

# Create a custom target for TypeScript compilation
add_custom_target(${PROJECT_NAME} ALL
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    COMMAND ${NPM_EXECUTABLE} run build
    COMMENT "Compiling TypeScript bindings to JavaScript..."
    SOURCES ${MIKANCLIENT_TYPESCRIPT_ALL_SRC}
)
add_dependencies(${PROJECT_NAME} MikanTypeScriptNpmInstall)

# Put in a "ClientAPI" folder in the project viewer
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ClientAPI)
set_target_properties(MikanTypeScriptNpmInstall PROPERTIES FOLDER ClientAPI)

# Step into the subdirectories (if Tests/CMakeLists.txt exists)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/Tests/CMakeLists.txt")
    MESSAGE(STATUS "Stepping into Tests")
    add_subdirectory(Tests)
endif()
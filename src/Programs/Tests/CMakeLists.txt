# Step into the subdirectories
MESSAGE(STATUS "Stepping into MikanClientTestCPP")
add_subdirectory(MikanClientTestCPP)
# C# targets are only supported by the Visual Studio generators. Under
# other generators (e.g. Ninja, used in CI for sccache support) enabling
# the CSharp language fails, so skip this project there. It is not part
# of the CI test targets anyway.
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  MESSAGE(STATUS "Stepping into MikanClientTestCSharp")
  add_subdirectory(MikanClientTestCSharp)
else()
  MESSAGE(STATUS "Skipping MikanClientTestCSharp (requires a Visual Studio generator)")
endif()
MESSAGE(STATUS "Stepping into UnitTests")
add_subdirectory(UnitTests)
# ClientSdkConsumer is deliberately not stepped into: it is a standalone project that
# builds against the installed client SDK rather than the source tree.
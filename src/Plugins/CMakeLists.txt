# Step into the subdirectories
if(MIKAN_GSTREAMER_ENABLED)
  MESSAGE(STATUS "Stepping into MikanGStreamerVideo")
  add_subdirectory(MikanGStreamerVideo)
endif()
# MikanARKitVideo additionally needs the CUDA Toolkit - see cmake/ThirdParty.cmake.
if(MIKAN_WITH_ARKIT_VIDEO)
  MESSAGE(STATUS "Stepping into MikanARKitVideo")
  add_subdirectory(MikanARKitVideo)
endif()
MESSAGE(STATUS "Stepping into MikanSteamVR")
add_subdirectory(MikanSteamVR)
MESSAGE(STATUS "Stepping into MikanWMFVideo")
add_subdirectory(MikanWMFVideo)
#pragma once

// Asks a hybrid-GPU laptop's graphics driver to run this process on the discrete GPU.
// Without it, Optimus and AMD switchable graphics start an executable on the integrated
// GPU, where OpenGL cannot share textures with CUDA on the discrete GPU, and where
// shared textures cannot reach a client running on the discrete GPU.
//
// The drivers only read these symbols from the executable's own export table, never from
// a DLL, so expand the macro exactly once, at file scope, in each executable's entry-point
// source file.
#if defined(_WIN32)
#define MIKAN_REQUEST_DISCRETE_GPU()                                                                                   \
	extern "C"                                                                                                         \
	{                                                                                                                  \
		__declspec(dllexport) unsigned long NvOptimusEnablement= 0x00000001;                                           \
		__declspec(dllexport) int AmdPowerXpressRequestHighPerformance= 1;                                             \
	}
#else
#define MIKAN_REQUEST_DISCRETE_GPU()
#endif

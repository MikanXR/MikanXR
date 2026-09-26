//-- includes -----
#include <stdio.h>
#include <stdlib.h>
#include "unit_test.h"

//-- prototypes -----

//-- entry point -----
int main(int argc, char* argv[])
{
	UNIT_TEST_SUITE_BEGIN()
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_math_utility_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_crash_handler_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_math_glm_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_serialization_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_mikan_api_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_csharp_interop_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_arkit_wire_protocol_unit_tests);
	// arkit_cuda_gl_interop creates its own CUDA context (see project memory
	// project_cuda_context_corruption_after_fault for why a module that creates a
	// fresh CUDA context should run early, before anything that could leave the
	// driver in a bad state).
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_arkit_cuda_gl_interop_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_arkit_video_device_interfaces_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_arkit_video_source_system_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_sh_lighting_unit_tests);
	UNIT_TEST_SUITE_CALL_CPP_MODULE(run_onnx_unit_tests);
	UNIT_TEST_SUITE_END()

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
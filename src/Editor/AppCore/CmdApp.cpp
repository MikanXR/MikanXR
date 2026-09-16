#include "CmdApp.h"
#include "CrashHandler.h"
#include "DepthMeshGenerator.h"
#include "Logger.h"
#include "PathUtils.h"
#include "SceneLightingEstimator.h"
#include "TypeRegistry.h"
#include "Version.h"
#include "TrackerPoseCalibratorTests.h"
#include "ARKitDebugProtocolTests.h"
#include "AutomationProtocolTests.h"
#include "AutomationVariantTextTests.h"
#include "ClientApiPropertySchemaTests.h"
#include "ComponentNamingTests.h"
#include "DepthMeshGeneratorTests.h"
#include "DMXPresetPersistenceTests.h"
#include "DMXSequenceTests.h"
#include "DMXUniverseRLETests.h"
#include "FileVideoSourceTests.h"
#include "HttpServerTests.h"
#include "LegacyContentMigrationTests.h"
#include "LightEnvironmentPersistenceTests.h"
#include "LocalizationTests.h"
#include "MaterialCompilerTests.h"
#include "ModelGeometryPayloadTests.h"
#include "NodeGraphHistoryTests.h"
#include "NodeGraphPropertyNameTests.h"
#include "NodeLinkDirectionTests.h"
#include "PixelGridLayoutTests.h"
#include "ProjectAssetCatalogTests.h"
#include "PropertyNotificationGuardTests.h"
#include "ScriptContextTests.h"
#include "ScriptVariablePersistenceTests.h"

#include "Graphs/MaterialNodeGraph.h"
#include "MaterialCompiler/GlslShaderWriter.h"
#include "MaterialCompiler/MaterialCompiler.h"

#include <opencv2/opencv.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace
{
bool run_all_editor_unit_tests()
{
	bool success= true;
	success&= run_tracker_pose_calibrator_unit_tests();
	success&= run_arkit_debug_protocol_tests();
	success&= run_automation_protocol_tests();
	success&= run_automation_variant_text_tests();
	success&= run_client_api_property_schema_tests();
	success&= run_component_naming_tests();
	success&= run_depth_mesh_generator_unit_tests();
	success&= run_dmx_preset_persistence_tests();
	success&= run_dmx_sequence_tests();
	success&= run_dmx_universe_rle_tests();
	success&= run_file_video_source_tests();
	success&= run_http_server_tests();
	success&= run_legacy_content_migration_tests();
	success&= run_light_environment_persistence_tests();
	success&= run_localization_unit_tests();
	success&= run_material_compiler_tests();
	success&= run_model_geometry_payload_tests();
	success&= run_node_graph_history_tests();
	success&= run_node_graph_property_name_tests();
	success&= run_node_link_direction_tests();
	success&= run_pixel_grid_layout_tests();
	success&= run_project_asset_catalog_tests();
	success&= run_property_notification_guard_tests();
	success&= run_script_context_tests();
	success&= run_script_variable_persistence_tests();
	// Future: add more test modules here
	return success;
}
} // namespace

int CmdApp::exec(int argc, char** argv)
{
	// Run with unbuffered stdout so test progress survives a hard crash.
	// When stdout is a pipe (e.g. CI) it is fully buffered by default, so a
	// crash mid-test would discard all buffered fprintf output and hide which
	// test was running. Unbuffered output makes the last printed line the
	// crash site.
	setvbuf(stdout, nullptr, _IONBF, 0);

	parseCommandLine(argc, argv);

	// Command output is written to stdout directly
	LoggerSettings settings= {};
	settings.min_log_level= LogSeverityLevel::debug;
	settings.enable_console= false;
	settings.log_filename= "MikanCmd.log";
	log_init(settings);

	// Crash reports go where the editor's do unless a run points them elsewhere
	CrashHandlerSettings crashSettings= {};
	crashSettings.reportDirectory=
		getCommandLineStringArg("crashReportDir", (PathUtils::getProjectsRootDirectory() / "CrashReports").string());
	crashSettings.logFilePath= settings.log_filename;
	crashSettings.appName= "MikanCmd";
	crashSettings.appVersion= MIKAN_RELEASE_VERSION_STRING;
	CrashHandler::install(crashSettings);

	// Build the reflection type registry
	// (Used by the serialization-based commands such as the unit tests).
	Serialization::TypeRegistry::buildFromRfkDatabase();

	// Dispatch to the requested command.
	int result= EXIT_SUCCESS;
	if (!getCommandLineStringArg("crash").empty())
	{
		result= triggerCrash();
	}
	else if (hasCommandLineFlag("runTests"))
	{
		result= runTests();
	}
	else if (hasCommandLineFlag("depthMesh"))
	{
		result= generateDepthMesh();
	}
	else if (!getCommandLineStringArg("compileMaterial").empty())
	{
		result= compileMaterial();
	}
	else if (hasCommandLineFlag("estimateLighting") || !getCommandLineStringArg("image").empty())
	{
		result= estimateLighting();
	}
	else
	{
		printUsage();
		result= EXIT_FAILURE;
	}

	CrashHandler::uninstall();
	log_dispose();

	return result;
}

void CmdApp::parseCommandLine(int argc, char** argv)
{
	for (int i= 1; i < argc; ++i)
	{
		std::string arg= argv[i];
		if (arg.size() > 1 && arg[0] == '-')
		{
			std::string key= arg.substr(1); // strip leading '-'
			auto eqPos= key.find('=');
			if (eqPos != std::string::npos)
				m_commandLineParams[key.substr(0, eqPos)]= key.substr(eqPos + 1);
			else
				m_commandLineFlags.insert(key);
		}
	}
}

bool CmdApp::hasCommandLineFlag(const std::string& flag) const { return m_commandLineFlags.count(flag) > 0; }

std::string CmdApp::getCommandLineStringArg(const std::string& key, const std::string& defaultValue) const
{
	auto it= m_commandLineParams.find(key);
	return it != m_commandLineParams.end() ? it->second : defaultValue;
}

void CmdApp::printUsage() const
{
	fprintf(stdout,
			"MikanCmd - Mikan command-line tool\n"
			"\n"
			"Usage: MikanCmd <command>\n"
			"\n"
			"Commands:\n"
			"  -runTests    Run the editor unit test suites\n"
			"  -estimateLighting -image=<path> [-fov=<degrees>] [-models=<dir>]\n"
			"               [-mogeModels=<dir>] [-cpu] [-dump=<dir>]\n"
			"               Estimate scene lighting from a single frame and print the\n"
			"               recovered spherical harmonic environment.\n"
			"  -depthMesh -image=<path> -fov=<degrees> [-obj=<path>] [-mogeModels=<dir>]\n"
			"               [-stride=<n>] [-maxDepth=<metres>] [-cpu]\n"
			"               Generate a camera-space depth proxy mesh from a single frame\n"
			"               and write it as an OBJ.\n"
			"  -compileMaterial=<graph>\n"
			"               Compile a material graph (.matgraph) and write its .vert, .frag\n"
			"               and .compmat or .shapemat beside the graph file.\n"
			"  -crash=<kind> [-crashReportDir=<dir>]\n"
			"               Crash on purpose to exercise the crash reporter. Kinds: %s\n",
			CrashHandler::getTestCrashKinds());
}

int CmdApp::triggerCrash() const
{
	const std::string kind= getCommandLineStringArg("crash");

	fprintf(stdout, "Triggering a '%s' crash\n", kind.c_str());
	if (!CrashHandler::triggerTestCrash(kind))
	{
		fprintf(stderr, "Unknown crash kind '%s'. Kinds: %s\n", kind.c_str(), CrashHandler::getTestCrashKinds());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int CmdApp::compileMaterial() const
{
	// Config loading resolves relative paths against the project and resource roots, so
	// hand the loader an absolute path
	const std::filesystem::path graphPath= std::filesystem::absolute(getCommandLineStringArg("compileMaterial"));

	NodeGraphFactory::registerFactory<MaterialNodeGraphFactory>();

	auto materialGraph= std::dynamic_pointer_cast<MaterialNodeGraph>(
		NodeGraphFactory::loadNodeGraph(nullptr, graphPath, MaterialNodeGraph::k_graphClassName));
	if (!materialGraph)
	{
		fprintf(stdout, "error: '%s' is not a loadable material graph\n", graphPath.string().c_str());
		return EXIT_FAILURE;
	}

	GlslShaderWriter writer;
	MaterialCompileResult result= materialGraph->compile(writer);
	for (const NodeEvaluationError& error : result.errors)
	{
		fprintf(stdout, "error: node %d: %s\n", error.errorNodeId, error.errorMessage.c_str());
	}
	if (result.hasErrors())
	{
		return EXIT_FAILURE;
	}

	std::string writeError;
	if (!MaterialCompiler::writeOutputs(result, graphPath, writeError))
	{
		fprintf(stdout, "error: %s\n", writeError.c_str());
		return EXIT_FAILURE;
	}

	fprintf(stdout, "wrote %s\n", MaterialCompiler::getVertexShaderPathForGraph(graphPath).string().c_str());
	fprintf(stdout, "wrote %s\n", MaterialCompiler::getFragmentShaderPathForGraph(graphPath).string().c_str());
	fprintf(stdout, "wrote %s\n",
			MaterialCompiler::getMaterialPathForGraph(graphPath, materialGraph->getDomain()).string().c_str());

	return EXIT_SUCCESS;
}

int CmdApp::estimateLighting() const
{
	const std::string imagePath= getCommandLineStringArg("image");
	const std::string modelDirectory= getCommandLineStringArg("models", "models/marigold");
	const std::string dumpDirectory= getCommandLineStringArg("dump");

	if (imagePath.empty())
	{
		fprintf(stdout, "error: -image=<path> is required\n");
		return EXIT_FAILURE;
	}

	cv::Mat bgrImage= cv::imread(imagePath, cv::IMREAD_COLOR);
	if (bgrImage.empty())
	{
		fprintf(stdout, "error: could not read image '%s'\n", imagePath.c_str());
		return EXIT_FAILURE;
	}
	fprintf(stdout, "image  : %s (%dx%d)\n", imagePath.c_str(), bgrImage.cols, bgrImage.rows);

	SceneLightingEstimator::Config config;
	config.modelDirectory= modelDirectory;
	config.mogeModelDirectory= getCommandLineStringArg("mogeModels", "models/moge2");
	config.preferGpu= !hasCommandLineFlag("cpu");

	// The denoise loop starts from random latents, so the seed changes the
	// result. Exposed so the run-to-run spread can be measured rather than
	// guessed at.
	const std::string seedArg= getCommandLineStringArg("seed");
	if (!seedArg.empty())
		config.seed= (unsigned int)strtoul(seedArg.c_str(), nullptr, 10);

	SceneLightingEstimator estimator;
	if (!estimator.startup(config))
	{
		fprintf(stdout, "error: failed to initialize the estimator (see MikanCmd.log)\n");
		return EXIT_FAILURE;
	}
	fprintf(stdout, "backend: %s\n", estimator.getActiveExecutionProvider());

	// No tracked camera here, so the camera-space result is reported as-is.
	// The FOV only affects MoGe-2's metric depth recovery, not the normals the
	// fit consumes, so a nominal default is fine for standalone images.
	const float fovXDegrees= (float)atof(getCommandLineStringArg("fov", "60").c_str());

	SceneLightingEstimator::Result result;
	if (!estimator.estimate(bgrImage, glm::mat3(1.f), fovXDegrees, result))
	{
		fprintf(stdout, "error: estimation failed (see MikanCmd.log)\n");
		return EXIT_FAILURE;
	}

	fprintf(stdout, "samples: %d used, %d rejected\n", result.sampleCount, result.rejectedPixelCount);
	fprintf(stdout, "l1/l0  : %.4f%s\n", result.directionality,
			result.directionality < 0.25f ? "  (near-ambient: key direction not meaningful)" : "");
	fprintf(stdout, "key dir: %.4f %.4f %.4f\n", result.keyLightDirection.x, result.keyLightDirection.y,
			result.keyLightDirection.z);
	fprintf(stdout, "negative solid angle: %.1f%%  (expected non-zero for directional scenes)\n",
			result.negativeSolidAngleFraction * 100.f);

	const glm::vec3 ambient= result.environment.coefficients[0] * 0.282095f * 3.14159265f;
	fprintf(stdout, "ambient: %.4f %.4f %.4f\n", ambient.r, ambient.g, ambient.b);

	fprintf(stdout, "sh coefficients (camera space, r g b):\n");
	for (int i= 0; i < k_shCoefficientCount; ++i)
	{
		const glm::vec3& c= result.cameraSpaceEnvironment.coefficients[i];
		fprintf(stdout, "  [%d] %+.6f %+.6f %+.6f\n", i, c.r, c.g, c.b);
	}

	if (!dumpDirectory.empty())
	{
		std::filesystem::create_directories(dumpDirectory);
		const std::filesystem::path root(dumpDirectory);

		// Written as 32-bit float TIFF so the linear values survive round trip;
		// an 8-bit PNG would quantize away exactly what the fit consumes.
		struct DumpTarget
		{
			const cv::Mat* image;
			const char* name;
		};
		const DumpTarget targets[]= {{&result.modelOutputs.albedo, "albedo.tiff"},
									 {&result.modelOutputs.shading, "shading.tiff"},
									 {&result.modelOutputs.residual, "residual.tiff"},
									 {&result.modelOutputs.normals, "normals.tiff"}};
		for (const DumpTarget& target : targets)
		{
			const std::string path= (root / target.name).string();
			if (!cv::imwrite(path, *target.image))
				fprintf(stdout, "warning: failed to write %s\n", path.c_str());
		}

		// Display-encoded reconstructions: what the recovered environment says
		// the scene's lighting is, next to the shading it was fit against. The
		// difference is dominated by cast shadows, which a probe cannot carry.
		struct ReconstructionTarget
		{
			SceneLightingEstimator::eReconstructionView view;
			const char* name;
		};
		const ReconstructionTarget reconstructions[]= {
			{SceneLightingEstimator::eReconstructionView::lighting, "reconstruction_lighting.png"},
			{SceneLightingEstimator::eReconstructionView::relit, "reconstruction_relit.png"},
			{SceneLightingEstimator::eReconstructionView::modelShading, "reconstruction_target_shading.png"}};
		for (const ReconstructionTarget& target : reconstructions)
		{
			const cv::Mat image= SceneLightingEstimator::renderReconstructionImage(result, target.view);
			const std::string path= (root / target.name).string();
			if (image.empty() || !cv::imwrite(path, image))
				fprintf(stdout, "warning: failed to write %s\n", path.c_str());
		}

		fprintf(stdout, "dumped model outputs to %s\n", dumpDirectory.c_str());
	}

	return EXIT_SUCCESS;
}

int CmdApp::generateDepthMesh() const
{
	const std::string imagePath= getCommandLineStringArg("image");
	const std::string fovArg= getCommandLineStringArg("fov");

	if (imagePath.empty() || fovArg.empty())
	{
		fprintf(stdout, "error: -image=<path> and -fov=<degrees> are required\n");
		return EXIT_FAILURE;
	}

	cv::Mat bgrImage= cv::imread(imagePath, cv::IMREAD_COLOR);
	if (bgrImage.empty())
	{
		fprintf(stdout, "error: could not read image '%s'\n", imagePath.c_str());
		return EXIT_FAILURE;
	}
	fprintf(stdout, "image  : %s (%dx%d)\n", imagePath.c_str(), bgrImage.cols, bgrImage.rows);

	MoGeInference::Config config;
	config.modelDirectory= getCommandLineStringArg("mogeModels", "models/moge2");
	config.preferGpu= !hasCommandLineFlag("cpu");

	MoGeInference inference;
	if (!inference.startup(config))
	{
		fprintf(stdout, "error: failed to load the MoGe-2 model (see MikanCmd.log)\n");
		return EXIT_FAILURE;
	}
	fprintf(stdout, "backend: %s\n", inference.getActiveExecutionProvider());

	const float fovXDegrees= (float)atof(fovArg.c_str());
	MoGeInference::Result geometry;
	if (!inference.run(bgrImage, fovXDegrees, geometry))
	{
		fprintf(stdout, "error: inference failed (see MikanCmd.log)\n");
		return EXIT_FAILURE;
	}

	DepthMeshGenerator::Config meshConfig;
	meshConfig.vertexStride= atoi(getCommandLineStringArg("stride", "4").c_str());
	const std::string maxDepthArg= getCommandLineStringArg("maxDepth");
	if (!maxDepthArg.empty())
		meshConfig.maxDepth= (float)atof(maxDepthArg.c_str());

	DepthMeshGenerator::Mesh mesh;
	DepthMeshGenerator::Stats stats;
	if (!DepthMeshGenerator::generateMesh(geometry, meshConfig, mesh, stats))
	{
		fprintf(stdout, "error: mesh generation produced no triangles (see MikanCmd.log)\n");
		return EXIT_FAILURE;
	}

	fprintf(stdout, "mesh   : %zu vertices, %zu triangles (%d cells cut at depth discontinuities)\n",
			mesh.vertices.size(), mesh.getTriangleCount(), stats.culledDiscontinuityEdges);
	fprintf(stdout, "depth  : %.2f - %.2f m\n", stats.nearDepth, stats.farDepth);

	const std::string objPath= getCommandLineStringArg("obj", "depth_mesh.obj");

	// The source frame doubles as the proxy's projected texture.
	const std::filesystem::path texturePath= std::filesystem::path(objPath).replace_extension(".png");
	std::string textureFileName;
	if (cv::imwrite(texturePath.string(), bgrImage))
		textureFileName= texturePath.filename().string();
	else
		fprintf(stdout, "warning: failed to write '%s'; mesh will be untextured\n", texturePath.string().c_str());

	if (!DepthMeshGenerator::saveObj(mesh, objPath, "DepthProxyMesh", textureFileName))
	{
		fprintf(stdout, "error: failed to write '%s'\n", objPath.c_str());
		return EXIT_FAILURE;
	}
	fprintf(stdout, "wrote  : %s\n", objPath.c_str());

	return EXIT_SUCCESS;
}

int CmdApp::runTests() const
{
	const bool testsPassed= run_all_editor_unit_tests();

	return testsPassed ? EXIT_SUCCESS : EXIT_FAILURE;
}

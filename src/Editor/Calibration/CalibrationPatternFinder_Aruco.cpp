#include "CalibrationPatternFinder_Aruco.h"
#include "CalibrationRenderHelpers.h"
#include "CameraComponent.h"
#include "CameraMath.h"
#include "Colors.h"
#include "IEditorWindow.h"
#include "MathOpenCV.h"
#include "MathUtility.h"
#include "MathTypeConversion.h"
#include "MarkerComponent.h"
#include "MarkerObjectSystem.h"
#include "MikanTextRenderer.h"
#include "MikanObjectSystem.h"
#include "ProjectManager.h"
#include "StageComponent.h"
#include "TextStyle.h"
#include "TrackingVolumeComponent.h"
#include "VideoFrameDistortionView.h"

#include "opencv2/opencv.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/objdetect/aruco_detector.hpp"

//-- ArucoBoardData -----
class ArucoBoardData
{
public:
	ArucoBoardData()= default;

	int desiredArucoId;
	float markerLengthMM;

	cv::Ptr<cv::aruco::ArucoDetector> detector;
	std::vector<t_opencv_point2d_list> markerCorners;
	std::vector<int> markerVisibleIds;
	t_opencv_point2d_list charucoCorners;
	std::vector<int> charucoIds;
};

//-- CalibrationPatternFinder_Aruco -----
static void initArucoBoardData(ArucoBoardData* markerData, OpenCVCalibrationGeometry& opencvSolvePnPGeometry,
							   OpenCVCalibrationGeometry& opencvLensCalibrationGeometry,
							   OpenGLCalibrationGeometry& openglSolvePnPGeometry, eCharucoDictionaryType dictionaryType,
							   MarkerDefinitionConstPtr markerDefinition)
{
	const int desiredArucoId= markerDefinition->getArucoId();
	const float markerLengthMM= markerDefinition->getLengthMM();
	ArucoDictionaryPtr dictionary= CalibrationPatternFinder::getArucoDictionary(dictionaryType);

	// Use corner refinement to get the best possible corner locations
	cv::aruco::DetectorParameters detectorParams;
	detectorParams.cornerRefinementMethod= cv::aruco::CORNER_REFINE_SUBPIX;

	markerData->desiredArucoId= desiredArucoId;
	markerData->markerLengthMM= markerLengthMM;
	markerData->detector= cv::makePtr<cv::aruco::ArucoDetector>(*dictionary.get(), detectorParams);

	// The Aruco board is a square, so we can hardcode the points in ARUCO_CCW_CENTER style
	// Solve PnP points are on the XZ Plane
	opencvSolvePnPGeometry.points.clear();
	opencvSolvePnPGeometry.points.push_back(cv::Point3f(-markerLengthMM / 2.f, 0.f, markerLengthMM / 2.f));
	opencvSolvePnPGeometry.points.push_back(cv::Point3f(markerLengthMM / 2.f, 0.f, markerLengthMM / 2.f));
	opencvSolvePnPGeometry.points.push_back(cv::Point3f(markerLengthMM / 2.f, 0.f, -markerLengthMM / 2.f));
	opencvSolvePnPGeometry.points.push_back(cv::Point3f(-markerLengthMM / 2.f, 0.f, -markerLengthMM / 2.f));

	// Derive the other geometry from the OpenCV SolvePnP geometry
	opencvLensCalibrationGeometry.points.clear();
	openglSolvePnPGeometry.points.clear();
	for (int index= 0; index < 4; index++)
	{
		// Solve PnP points are on the XZ Plane
		const cv::Point3f& openCVSolvePnPPoint= opencvSolvePnPGeometry.points[index];

		// Lens calibration points are on the XY Plane
		cv::Point3f openCVLensCalibrationPoint(openCVSolvePnPPoint.x, openCVSolvePnPPoint.z, 0.f);
		opencvLensCalibrationGeometry.points.push_back(openCVLensCalibrationPoint);

		// OpenCV -> OpenGL coordinate system transform
		// Rendering world units in meters, not mm
		glm::vec3 openGLPoint(openCVSolvePnPPoint.x * k_millimeters_to_meters,
							  -openCVSolvePnPPoint.y * k_millimeters_to_meters,
							  -openCVSolvePnPPoint.z * k_millimeters_to_meters);
		openglSolvePnPGeometry.points.push_back(openGLPoint);
	}
}

// Fills outImagePoints with the desired marker's corners, or empties it when the
// marker is not among those found. Returns whether any marker was found.
static bool detectDesiredMarker(ArucoBoardData* markerData, const cv::Mat& grayscaleImage,
								t_opencv_point2d_list& outImagePoints)
{
	outImagePoints.clear();
	markerData->markerCorners.clear();
	markerData->detector->detectMarkers(grayscaleImage, markerData->markerCorners, markerData->markerVisibleIds);

	for (size_t index= 0; index < markerData->markerVisibleIds.size(); ++index)
	{
		if (markerData->markerVisibleIds[index] == markerData->desiredArucoId)
		{
			outImagePoints= markerData->markerCorners[index];
			break;
		}
	}

	return markerData->markerVisibleIds.size() > 0;
}

CalibrationPatternFinder_Aruco::CalibrationPatternFinder_Aruco(CameraComponentConstPtr cameraComponent,
															   VideoFrameDistortionView* distortionView)
	: CalibrationPatternFinder(distortionView)
	, m_markerData(new ArucoBoardData())
{
	StageComponentConstPtr ownerStage= cameraComponent->getOwnerStageComponent();
	assert(ownerStage != nullptr);
	IEditorWindow* ownerWindow= ownerStage->getOwnerEditorWindow();
	assert(ownerWindow != nullptr);
	MarkerObjectSystemPtr markerSystem= ownerWindow->getProjectManager()->getSystemOfType<MarkerObjectSystem>();
	assert(markerSystem != nullptr);
	TrackingVolumeDefinitionConstPtr trackingVolume= ownerStage->getTrackingVolumeDefinitionConst();
	assert(trackingVolume != nullptr);
	MarkerDefinitionConstPtr originMarker= trackingVolume->getOriginMarker();
	assert(originMarker != nullptr);

	initArucoBoardData(m_markerData, m_opencvSolvePnPGeometry, m_opencvLensCalibrationGeometry,
					   m_openglSolvePnPGeometry, markerSystem->getTypedDefinition()->getArucoDictionaryType(),
					   originMarker);
}

CalibrationPatternFinder_Aruco::CalibrationPatternFinder_Aruco(CameraComponentConstPtr cameraComponent,
															   VideoFrameDistortionView* distortionView,
															   MarkerDefinitionConstPtr markerDefinition)
	: CalibrationPatternFinder(distortionView)
	, m_markerData(new ArucoBoardData())
{
	StageComponentConstPtr ownerStage= cameraComponent->getOwnerStageComponent();
	assert(ownerStage != nullptr);
	IEditorWindow* ownerWindow= ownerStage->getOwnerEditorWindow();
	assert(ownerWindow != nullptr);
	MarkerObjectSystemPtr markerSystem= ownerWindow->getProjectManager()->getSystemOfType<MarkerObjectSystem>();
	assert(markerSystem != nullptr);
	assert(markerDefinition != nullptr);

	initArucoBoardData(m_markerData, m_opencvSolvePnPGeometry, m_opencvLensCalibrationGeometry,
					   m_openglSolvePnPGeometry, markerSystem->getTypedDefinition()->getArucoDictionaryType(),
					   markerDefinition);
}

CalibrationPatternFinder_Aruco::~CalibrationPatternFinder_Aruco() { delete m_markerData; }

bool CalibrationPatternFinder_Aruco::findNewCalibrationPattern(const float minSeperationDist)
{
	// Clear out the previous images points
	bool bImagePointsValid= false;
	m_currentImagePoints.clear();

	// Fetch the source image buffer we are searching for the pattern in
	cv::Mat* gsSourceBuffer= getGrayscaleVideoFrameInput();
	if (gsSourceBuffer == nullptr)
		return false;

	return detectDesiredMarker(m_markerData, *gsSourceBuffer, m_currentImagePoints);
}

bool CalibrationPatternFinder_Aruco::estimateMarkerPoseInImage(const cv::Mat& grayscaleImage,
															   const MikanMonoIntrinsics& intrinsics,
															   MarkerDefinitionConstPtr markerDefinition,
															   eCharucoDictionaryType dictionaryType,
															   glm::dmat4& outApertureToMarkerXform)
{
	if (!markerDefinition || grayscaleImage.empty())
		return false;

	ArucoBoardData markerData;
	OpenCVCalibrationGeometry opencvSolvePnPGeometry;
	OpenCVCalibrationGeometry opencvLensCalibrationGeometry;
	OpenGLCalibrationGeometry openglSolvePnPGeometry;
	initArucoBoardData(&markerData, opencvSolvePnPGeometry, opencvLensCalibrationGeometry, openglSolvePnPGeometry,
					   dictionaryType, markerDefinition);

	t_opencv_point2d_list imagePoints;
	detectDesiredMarker(&markerData, grayscaleImage, imagePoints);
	if (imagePoints.empty())
		return false;

	cv::Quatd cameraToMarkerRot;
	cv::Vec3d cameraToMarkerVecMM;
	if (!computeOpenCVCameraRelativePatternTransform(intrinsics, imagePoints, opencvSolvePnPGeometry.points,
													 cameraToMarkerRot, cameraToMarkerVecMM, nullptr))
	{
		return false;
	}

	convertOpenCVCameraRelativePoseToGLMMat(cameraToMarkerRot, cameraToMarkerVecMM, outApertureToMarkerXform);
	return true;
}

bool CalibrationPatternFinder_Aruco::fetchLastFoundCalibrationPattern(t_opencv_point2d_list& outImagePoints,
																	  t_opencv_pointID_list& outImagePointIDs,
																	  cv::Point2f outBoundingQuad[4])
{
	// If it's a valid new location, append it to the board list
	if (areCurrentImagePointsValid())
	{
		// Keep track of the corners of all of the chessboards we sample
		outBoundingQuad[0]= m_currentImagePoints[0];
		outBoundingQuad[1]= m_currentImagePoints[1];
		outBoundingQuad[2]= m_currentImagePoints[2];
		outBoundingQuad[3]= m_currentImagePoints[3];

		outImagePoints.clear();
		for (const auto& imagePoint : m_currentImagePoints)
		{
			outImagePoints.push_back(imagePoint);
		}

		outImagePointIDs.clear();
		outImagePointIDs.push_back(m_markerData->desiredArucoId);

		// Remember the last valid captured points
		m_lastValidImagePoints= m_currentImagePoints;

		return true;
	}

	return false;
}

void CalibrationPatternFinder_Aruco::renderCalibrationPattern2D() const
{
	CalibrationPatternFinder::renderCalibrationPattern2D();

	IMkGraphicsContext* graphicsContext= getDistortionView()->getGraphicsContext();

	// Draw the marker corners, if any
	TextStyle style= getDefaultTextStyle();
	style.horizontalAlignment= eHorizontalTextAlignment::Middle;
	style.verticalAlignment= eVerticalTextAlignment::Middle;
	style.color= Colors::Yellow;

	static int debugDrawIndex= -1;

	for (int quadIndex= 0; quadIndex < m_markerData->markerCorners.size(); quadIndex++)
	{
		if (debugDrawIndex != -1 && debugDrawIndex != quadIndex)
			continue;

		const t_opencv_point2d_list& corners= m_markerData->markerCorners[quadIndex];

		drawQuadList2d(graphicsContext, m_frameWidth, m_frameHeight,
					   (float*)corners.data(), // cv::point2f is just two floats
					   (int)corners.size(), Colors::Yellow);

		if (quadIndex < m_markerData->markerVisibleIds.size())
		{
			int markerId= m_markerData->markerVisibleIds[quadIndex];

			cv::Point2f quadCenter;
			opencv_point2f_compute_average(corners, quadCenter);

			drawTextAtTrackerPosition(graphicsContext, style, m_frameWidth, m_frameHeight,
									  glm::vec2(quadCenter.x, quadCenter.y), L"%d", markerId);
		}
	}
}
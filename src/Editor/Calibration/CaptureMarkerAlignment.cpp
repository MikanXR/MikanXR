#include "CaptureMarkerAlignment.h"
#include "CalibrationPatternFinder_Aruco.h"
#include "CameraMath.h"
#include "Capture.h"
#include "FrameCoupledAlignment.h"
#include "MikanVideoSourceTypes.h"
#include "PoseTrack.h"

#include "DepthProjection.h"
#include "DepthTrackReader.h"
#include "OcclusionMeshBuilder.h"

#include "opencv2/imgproc.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace
{
// The annulus of table fitted around the marker, in marker half-widths: past
// the printed square, which returns poor depth, and within reach of a tabletop
constexpr float k_innerRadiusHalfWidths= 1.2f;
constexpr float k_outerRadiusHalfWidths= 3.f;
// Points farther than this from the shot's marker plane are not table
constexpr float k_planeWindowM= 0.02f;
constexpr int k_minPoints= 40;
constexpr float k_ransacInlierM= 0.003f;
constexpr int k_ransacIterations= 64;
// A fitted plane tilted further than this from the marker's normal is rejected
constexpr float k_maxTiltDeg= 10.f;

// Least squares plane through points: the centroid and the normal of least
// spread
bool fitPlane(const std::vector<glm::vec3>& points, glm::vec3& outCentroid, glm::vec3& outNormal)
{
	if (points.size() < 3)
		return false;

	glm::dvec3 centroid(0.0);
	for (const glm::vec3& point : points)
		centroid+= glm::dvec3(point);
	centroid/= (double)points.size();

	cv::Matx33d covariance= cv::Matx33d::zeros();
	for (const glm::vec3& point : points)
	{
		const glm::dvec3 d= glm::dvec3(point) - centroid;
		const double c[3]= {d.x, d.y, d.z};
		for (int row= 0; row < 3; ++row)
			for (int column= 0; column < 3; ++column)
				covariance(row, column)+= c[row] * c[column];
	}

	cv::Mat eigenvalues, eigenvectors;
	if (!cv::eigen(cv::Mat(covariance), eigenvalues, eigenvectors))
		return false;

	// Eigenvalues come back in descending order, so the last row is the normal
	outCentroid= glm::vec3(centroid);
	outNormal= glm::normalize(glm::vec3((float)eigenvectors.at<double>(2, 0), (float)eigenvectors.at<double>(2, 1),
										(float)eigenvectors.at<double>(2, 2)));
	return true;
}
} // namespace

namespace CaptureMarkerAlignment
{
bool solveWorldToStage(const cv::Mat& markerImage, const PoseTrackFrame& markerShotFrame, int imageWidth,
					   int imageHeight, MarkerDefinitionConstPtr originMarker, eCharucoDictionaryType dictionaryType,
					   glm::mat4& outWorldToStage, std::string& outError)
{
	if (!originMarker)
	{
		outError= "no origin marker";
		return false;
	}

	if (markerImage.empty() || markerImage.cols != imageWidth || markerImage.rows != imageHeight)
	{
		outError= "the marker image is not the size its intrinsics describe";
		return false;
	}

	cv::Mat grayscaleImage;
	if (markerImage.channels() == 1)
		grayscaleImage= markerImage;
	else
		cv::cvtColor(markerImage, grayscaleImage, cv::COLOR_BGR2GRAY);

	MikanMonoIntrinsics intrinsics;
	createMonoIntrinsicsFromPinhole(imageWidth, imageHeight, markerShotFrame.fx, markerShotFrame.fy, markerShotFrame.cx,
									markerShotFrame.cy, intrinsics);

	glm::dmat4 apertureToMarker;
	if (!CalibrationPatternFinder_Aruco::estimateMarkerPoseInImage(grayscaleImage, intrinsics, originMarker,
																   dictionaryType, apertureToMarker))
	{
		outError= "the origin marker was not found in the marker image";
		return false;
	}

	// One sample: the shot's camera is where the pose says it was, so there is
	// nothing to average
	outWorldToStage=
		glm::mat4(FrameCoupledAlignment::computeWorldToStageSample(apertureToMarker, markerShotFrame.cameraToWorld));
	return true;
}

bool solveWorldToStage(Capture& capture, MarkerDefinitionConstPtr originMarker, eCharucoDictionaryType dictionaryType,
					   glm::mat4& outWorldToStage, std::string& outError)
{
	const PoseTrack& markerTrack= capture.getPoseTrack(eCaptureMemberRole::marker_pose);
	if (!markerTrack.isLoaded() || markerTrack.getFrameCount() == 0)
	{
		outError= "the capture has no marker pose";
		return false;
	}

	cv::Mat markerImage;
	if (!capture.decodeImage(eCaptureMemberRole::marker_image, markerImage, outError))
		return false;

	return solveWorldToStage(markerImage, markerTrack.getFrame(0), markerTrack.getImageWidth(),
							 markerTrack.getImageHeight(), originMarker, dictionaryType, outWorldToStage, outError);
}

TableHeightEstimator::TableHeightEstimator(const glm::mat4& worldToStage, float markerLengthM)
{
	const glm::mat4 markerToWorld= glm::inverse(worldToStage);
	m_markerCentre= glm::vec3(markerToWorld[3]);
	m_markerNormal= glm::normalize(glm::vec3(markerToWorld[1]));
	m_innerRadius= k_innerRadiusHalfWidths * 0.5f * markerLengthM;
	m_outerRadius= k_outerRadiusHalfWidths * 0.5f * markerLengthM;
}

bool TableHeightEstimator::addFrame(const cv::Mat& depthMeters, const cv::Mat& confidence,
									const PinholeIntrinsics& depthIntrinsics, const glm::mat4& cameraToWorld)
{
	const DepthViewFrame view= DepthProjection::makeFrame(cameraToWorld);
	float centreDepth= 0.f;
	int centreU= 0, centreV= 0;
	if (!DepthProjection::projectToPixel(view, depthIntrinsics, depthMeters.cols, depthMeters.rows, m_markerCentre,
										 centreDepth, centreU, centreV))
	{
		return false;
	}

	// The pixel box that holds the annulus at the marker's depth
	const int radiusU= (int)std::ceil(depthIntrinsics.fx * m_outerRadius / centreDepth) + 1;
	const int radiusV= (int)std::ceil(depthIntrinsics.fy * m_outerRadius / centreDepth) + 1;
	const int u0= std::max(centreU - radiusU, 0), u1= std::min(centreU + radiusU, depthMeters.cols - 1);
	const int v0= std::max(centreV - radiusV, 0), v1= std::min(centreV + radiusV, depthMeters.rows - 1);

	std::vector<glm::vec3> points;
	for (int v= v0; v <= v1; ++v)
	{
		const float* depthRow= depthMeters.ptr<float>(v);
		const uint8_t* confidenceRow= confidence.ptr<uint8_t>(v);
		for (int u= u0; u <= u1; ++u)
		{
			const float z= depthRow[u];
			if (confidenceRow[u] < 2 || !(z > 0.f))
				continue;

			const glm::vec4 cameraPoint((float)((u - depthIntrinsics.cx) / depthIntrinsics.fx) * z,
										(float)((v - depthIntrinsics.cy) / depthIntrinsics.fy) * z, z, 1.f);
			const glm::vec3 worldPoint= glm::vec3(cameraToWorld * cameraPoint);
			const glm::vec3 offset= worldPoint - m_markerCentre;
			const float height= glm::dot(offset, m_markerNormal);
			const float radius= glm::length(offset - height * m_markerNormal);
			if (std::fabs(height) <= k_planeWindowM && radius >= m_innerRadius && radius <= m_outerRadius)
				points.push_back(worldPoint);
		}
	}
	if ((int)points.size() < k_minPoints)
		return false;

	// RANSAC for the table among stray objects at its edge, seeded per frame so
	// a rerun is identical, then a least squares refit on the inliers
	std::mt19937 rng((uint32_t)m_heights.size() + 1);
	std::uniform_int_distribution<size_t> pick(0, points.size() - 1);
	std::vector<glm::vec3> bestInliers;
	std::vector<glm::vec3> inliers;
	for (int iteration= 0; iteration < k_ransacIterations; ++iteration)
	{
		const glm::vec3& a= points[pick(rng)];
		const glm::vec3& b= points[pick(rng)];
		const glm::vec3& c= points[pick(rng)];
		const glm::vec3 cross= glm::cross(b - a, c - a);
		const float crossLength= glm::length(cross);
		if (crossLength < 1e-9f)
			continue;

		const glm::vec3 normal= cross / crossLength;
		inliers.clear();
		for (const glm::vec3& point : points)
		{
			if (std::fabs(glm::dot(point - a, normal)) <= k_ransacInlierM)
				inliers.push_back(point);
		}
		if (inliers.size() > bestInliers.size())
			bestInliers.swap(inliers);
	}
	if ((int)bestInliers.size() < k_minPoints)
		return false;

	glm::vec3 centroid, normal;
	if (!fitPlane(bestInliers, centroid, normal))
		return false;

	const float cosine= std::fabs(glm::dot(normal, m_markerNormal));
	if (cosine < std::cos(glm::radians(k_maxTiltDeg)))
		return false;

	// Where the line through the marker centre along its normal meets the plane
	m_heights.push_back(glm::dot(normal, centroid - m_markerCentre) / glm::dot(normal, m_markerNormal));
	return true;
}

bool TableHeightEstimator::estimate(float& outHeightM, float& outSpreadM) const
{
	if ((int)m_heights.size() < k_minFrames)
		return false;

	std::vector<float> values= m_heights;
	const size_t middle= values.size() / 2;
	std::nth_element(values.begin(), values.begin() + middle, values.end());
	outHeightM= values[middle];

	for (float& value : values)
		value= std::fabs(value - outHeightM);
	std::nth_element(values.begin(), values.begin() + middle, values.end());
	outSpreadM= values[middle];
	return true;
}

bool snapToTableHeight(const std::filesystem::path& capturePath, const CaptureArchiveMember& depthMember,
					   const CaptureArchiveMember& confidenceMember, const PoseTrack& track,
					   const glm::mat4& worldToStage, float markerLengthM, const std::atomic<bool>* bCancel,
					   TableHeightResult& outResult, std::string& outError)
{
	DepthTrackReader reader;
	if (!reader.open(capturePath, depthMember, confidenceMember, track.getDepthWidth(), track.getDepthHeight(),
					 track.getDepthUnitMm(), track.getFrameCount()))
	{
		outError= "the depth members could not be read";
		return false;
	}

	TableHeightEstimator estimator(worldToStage, markerLengthM);
	cv::Mat depth, confidence;
	for (size_t trackIndex= 0; trackIndex < track.getFrameCount(); ++trackIndex)
	{
		if (bCancel != nullptr && *bCancel)
		{
			outError= "cancelled";
			return false;
		}

		const PoseTrackFrame& frame= track.getFrame(trackIndex);
		if (!reader.readRecord(frame.fileIndex, depth, confidence))
			continue;

		PinholeIntrinsics colour;
		colour.fx= frame.fx;
		colour.fy= frame.fy;
		colour.cx= frame.cx;
		colour.cy= frame.cy;
		const PinholeIntrinsics depthIntrinsics= scaleIntrinsics(colour, track.getImageWidth(), track.getImageHeight(),
																 track.getDepthWidth(), track.getDepthHeight());
		estimator.addFrame(depth, confidence, depthIntrinsics,
						   OcclusionMeshBuilder::toOpenCvCameraToWorld(frame.cameraToWorld));
	}

	outResult.framesUsed= estimator.getFramesUsed();
	if (!estimator.estimate(outResult.heightM, outResult.spreadM))
	{
		outError= "too few frames saw the table around the marker";
		return false;
	}

	outResult.snappedWorldToStage= snapWorldToStageHeight(worldToStage, outResult.heightM);
	return true;
}

glm::mat4 snapWorldToStageHeight(const glm::mat4& worldToStage, float heightM)
{
	const glm::mat4 markerToWorld= glm::inverse(worldToStage);
	const glm::vec3 markerNormal= glm::normalize(glm::vec3(markerToWorld[1]));
	const glm::mat4 snappedMarkerToWorld= glm::translate(glm::mat4(1.f), heightM * markerNormal) * markerToWorld;
	return glm::inverse(snappedMarkerToWorld);
}
} // namespace CaptureMarkerAlignment

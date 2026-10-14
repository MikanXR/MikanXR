#include "FrameToModelRegistration.h"
#include "SmallMotion.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace FrameRegistrationInternal
{
glm::dvec3 toDVec3(const cv::Vec3f& v) { return glm::dvec3(v[0], v[1], v[2]); }
} // namespace FrameRegistrationInternal

void FrameToModelRegistration::unproject(const cv::Mat& depthMeters, const PinholeIntrinsics& intrinsics,
										 cv::Mat& outPoints)
{
	outPoints.create(depthMeters.rows, depthMeters.cols, CV_32FC3);

	for (int y= 0; y < depthMeters.rows; ++y)
	{
		const float* depthRow= depthMeters.ptr<float>(y);
		cv::Vec3f* pointRow= outPoints.ptr<cv::Vec3f>(y);
		const double rayY= ((double)y - intrinsics.cy) / intrinsics.fy;

		for (int x= 0; x < depthMeters.cols; ++x)
		{
			const double depth= depthRow[x];
			const double rayX= ((double)x - intrinsics.cx) / intrinsics.fx;
			pointRow[x]= cv::Vec3f((float)(rayX * depth), (float)(rayY * depth), (float)depth);
		}
	}
}

void FrameToModelRegistration::computeFrameNormals(const cv::Mat& points, const cv::Mat& valid, cv::Mat& outNormals,
												   cv::Mat& outOk)
{
	const int height= points.rows;
	const int width= points.cols;

	outNormals.create(height, width, CV_32FC3);
	outNormals.setTo(cv::Scalar::all(0.0));
	outOk.create(height, width, CV_8UC1);
	outOk.setTo(cv::Scalar::all(0));

	// Border pixels have no central difference, so they keep a zero normal
	for (int y= 1; y < height - 1; ++y)
	{
		const cv::Vec3f* pointRow= points.ptr<cv::Vec3f>(y);
		const cv::Vec3f* pointRowAbove= points.ptr<cv::Vec3f>(y - 1);
		const cv::Vec3f* pointRowBelow= points.ptr<cv::Vec3f>(y + 1);
		const uint8_t* validRow= valid.ptr<uint8_t>(y);
		const uint8_t* validRowAbove= valid.ptr<uint8_t>(y - 1);
		const uint8_t* validRowBelow= valid.ptr<uint8_t>(y + 1);
		cv::Vec3f* normalRow= outNormals.ptr<cv::Vec3f>(y);
		uint8_t* okRow= outOk.ptr<uint8_t>(y);

		for (int x= 1; x < width - 1; ++x)
		{
			const glm::dvec3 point= FrameRegistrationInternal::toDVec3(pointRow[x]);
			const glm::dvec3 dx= FrameRegistrationInternal::toDVec3(pointRow[x + 1])
								 - FrameRegistrationInternal::toDVec3(pointRow[x - 1]);
			const glm::dvec3 dy= FrameRegistrationInternal::toDVec3(pointRowBelow[x])
								 - FrameRegistrationInternal::toDVec3(pointRowAbove[x]);

			bool bOk= validRow[x] != 0 && validRow[x + 1] != 0 && validRow[x - 1] != 0 && validRowBelow[x] != 0
					  && validRowAbove[x] != 0;

			// A neighbour step longer than a tenth of the depth is a silhouette, not a surface
			const double depth= std::max(point.z, 1e-6);
			bOk= bOk && glm::length(dx) < 0.1 * depth && glm::length(dy) < 0.1 * depth;

			glm::dvec3 normal= glm::cross(dx, dy);
			const double length= glm::length(normal);
			bOk= bOk && length > 1e-12;
			normal/= std::max(length, 1e-12);

			// The camera looks down +Z, so a normal facing it has negative Z
			if (normal.z > 0.0)
				normal= -normal;

			normalRow[x]= cv::Vec3f((float)normal.x, (float)normal.y, (float)normal.z);
			okRow[x]= bOk ? 1 : 0;
		}
	}
}

RegistrationResult FrameToModelRegistration::registerFrame(const cv::Mat& depthMeters, const cv::Mat& weight,
														   const PinholeIntrinsics& intrinsics,
														   const glm::mat4& priorCameraToWorld, const ModelMap& model,
														   const glm::mat4& modelCameraToWorld,
														   const RegistrationConfig& config)
{
	const int height= depthMeters.rows;
	const int width= depthMeters.cols;

	// Usable frame pixels: in range, weighted, and with a trustworthy normal
	cv::Mat valid(height, width, CV_8UC1);
	for (int y= 0; y < height; ++y)
	{
		const float* depthRow= depthMeters.ptr<float>(y);
		const float* weightRow= weight.ptr<float>(y);
		uint8_t* validRow= valid.ptr<uint8_t>(y);
		for (int x= 0; x < width; ++x)
		{
			const float depth= depthRow[x];
			validRow[x]=
				(std::isfinite(depth) && depth > config.depthMin && depth < config.depthMax && weightRow[x] > 0.f) ? 1
																												   : 0;
		}
	}

	cv::Mat points;
	unproject(depthMeters, intrinsics, points);
	cv::Mat normals;
	cv::Mat normalOk;
	computeFrameNormals(points, valid, normals, normalOk);

	std::vector<glm::dvec3> cameraPoints;
	std::vector<glm::dvec3> cameraNormals;
	cameraPoints.reserve((size_t)width * height);
	cameraNormals.reserve((size_t)width * height);
	for (int y= 0; y < height; ++y)
	{
		const cv::Vec3f* pointRow= points.ptr<cv::Vec3f>(y);
		const cv::Vec3f* normalRow= normals.ptr<cv::Vec3f>(y);
		const uint8_t* validRow= valid.ptr<uint8_t>(y);
		const uint8_t* okRow= normalOk.ptr<uint8_t>(y);
		for (int x= 0; x < width; ++x)
		{
			if (validRow[x] != 0 && okRow[x] != 0)
			{
				cameraPoints.push_back(FrameRegistrationInternal::toDVec3(pointRow[x]));
				cameraNormals.push_back(FrameRegistrationInternal::toDVec3(normalRow[x]));
			}
		}
	}

	const glm::dmat4 priorPose(priorCameraToWorld);
	const glm::dmat4 priorPoseInv= glm::inverse(priorPose);
	const glm::dmat4 modelWorldToCamera= glm::inverse(glm::dmat4(modelCameraToWorld));
	const glm::dmat3 modelRotation(modelWorldToCamera);
	const glm::dvec3 modelTranslation(modelWorldToCamera[3]);

	const int modelWidth= model.points.cols;
	const int modelHeight= model.points.rows;

	glm::dmat4 pose= priorPose;
	const double maxDist= (double)config.maxCorrespondenceM;
	const double minDist= maxDist / 3.0;
	const double cosLimit= std::cos(glm::radians((double)config.maxNormalAngleDeg));
	const int iterations= config.iterations;
	const int minCorrespondences= config.minCorrespondences;
	const double huber= (double)config.huberM;

	// The prior enters the normal equations as a diagonal block on the
	// deviation from the prior pose, so each round pulls toward the prior by
	// exactly as much as the data leaves unconstrained.
	const double sigmaR= glm::radians((double)config.priorSigmaDeg);
	const double sigmaT= (double)config.priorSigmaM;
	const double priorWeight[6]= {1.0 / (sigmaR * sigmaR), 1.0 / (sigmaR * sigmaR), 1.0 / (sigmaR * sigmaR),
								  1.0 / (sigmaT * sigmaT), 1.0 / (sigmaT * sigmaT), 1.0 / (sigmaT * sigmaT)};
	// The data term is weighted by its own noise so the prior reads in the
	// same units: thousands of points at that noise outweigh the prior along
	// any direction they constrain, and leave it in charge of the rest.
	const double dataWeight= 1.0 / ((double)config.dataSigmaM * (double)config.dataSigmaM);

	double rmsBefore= std::numeric_limits<double>::quiet_NaN();
	double rmsAfter= std::numeric_limits<double>::quiet_NaN();
	int count= 0;
	int lastIteration= 0;
	for (int iteration= 1; iteration <= iterations; ++iteration)
	{
		lastIteration= iteration;

		const double gate=
			maxDist - (maxDist - minDist) * (double)(iteration - 1) / (double)std::max(iterations - 1, 1);
		const glm::dmat3 rotation(pose);
		const glm::dvec3 translation(pose[3]);

		// Normal equations of the point-to-plane residuals, row-major 6x6
		double A[36]= {};
		double b[6]= {};
		double sumSquares= 0.0;
		count= 0;

		for (size_t i= 0; i < cameraPoints.size(); ++i)
		{
			const glm::dvec3 worldPoint= rotation * cameraPoints[i] + translation;
			const glm::dvec3 worldNormal= rotation * cameraNormals[i];

			// Associate by projecting into the model camera
			const glm::dvec3 modelCameraPoint= modelRotation * worldPoint + modelTranslation;
			const double z= modelCameraPoint.z;
			if (!(z > 1e-3))
				continue;

			// Round half to even, as the projection's reference does
			const double ud= std::nearbyint(modelCameraPoint.x / z * intrinsics.fx + intrinsics.cx);
			const double vd= std::nearbyint(modelCameraPoint.y / z * intrinsics.fy + intrinsics.cy);
			if (!(ud >= 0.0 && ud < (double)modelWidth && vd >= 0.0 && vd < (double)modelHeight))
				continue;

			const int u= (int)ud;
			const int v= (int)vd;
			if (model.hit.at<uint8_t>(v, u) == 0)
				continue;

			const glm::dvec3 modelPoint= FrameRegistrationInternal::toDVec3(model.points.at<cv::Vec3f>(v, u));
			const glm::dvec3 modelNormal= FrameRegistrationInternal::toDVec3(model.normals.at<cv::Vec3f>(v, u));
			const glm::dvec3 diff= worldPoint - modelPoint;
			if (!(glm::length(diff) < gate))
				continue;
			if (!(glm::dot(worldNormal, modelNormal) > cosLimit))
				continue;

			const double residual= glm::dot(diff, modelNormal);
			++count;
			sumSquares+= residual * residual;

			// Point-to-plane Jacobian for a left-multiplied twist on the pose:
			// d(n . (exp(xi) p - m)) / d xi = [ (p x n), n ]
			const glm::dvec3 pointCrossNormal= glm::cross(worldPoint, modelNormal);
			const double J[6]= {pointCrossNormal.x, pointCrossNormal.y, pointCrossNormal.z,
								modelNormal.x,      modelNormal.y,      modelNormal.z};

			// Huber weighting keeps a few bad associations from steering the solve
			const double absResidual= std::fabs(residual);
			const double w= (absResidual <= huber ? 1.0 : huber / std::max(absResidual, 1e-9)) * dataWeight;

			for (int row= 0; row < 6; ++row)
			{
				const double wJRow= w * J[row];
				for (int col= row; col < 6; ++col)
				{
					A[row * 6 + col]+= wJRow * J[col];
				}
				b[row]-= wJRow * residual;
			}
		}

		if (count < minCorrespondences)
			break;

		const double rms= std::sqrt(sumSquares / (double)count);
		if (iteration == 1)
			rmsBefore= rms;
		rmsAfter= rms;

		// Mirror the accumulated upper triangle
		for (int row= 0; row < 6; ++row)
		{
			for (int col= 0; col < row; ++col)
			{
				A[row * 6 + col]= A[col * 6 + row];
			}
		}

		// Deviation of the current estimate from the prior as a twist in the
		// same left-multiplied parameterisation as the update
		const glm::dmat4 deviation= pose * priorPoseInv;
		const glm::dvec3 deviationRotation= SmallMotion::logSo3(glm::dmat3(deviation));
		const glm::dvec3 deviationTranslation(deviation[3]);
		const double deviationTwist[6]= {deviationRotation.x,    deviationRotation.y,    deviationRotation.z,
										 deviationTranslation.x, deviationTranslation.y, deviationTranslation.z};
		for (int k= 0; k < 6; ++k)
		{
			A[k * 6 + k]+= 1e-9 + priorWeight[k];
			b[k]-= priorWeight[k] * deviationTwist[k];
		}

		const cv::Mat normalMatrix(6, 6, CV_64F, A);
		const cv::Mat rhs(6, 1, CV_64F, b);
		cv::Mat solution;
		if (!cv::solve(normalMatrix, rhs, solution, cv::DECOMP_CHOLESKY))
			break;

		double twist[6];
		for (int k= 0; k < 6; ++k)
		{
			twist[k]= solution.at<double>(k, 0);
		}
		pose= SmallMotion::expSe3(twist) * pose;

		const double rotationStep= glm::length(glm::dvec3(twist[0], twist[1], twist[2]));
		const double translationStep= glm::length(glm::dvec3(twist[3], twist[4], twist[5]));
		if (translationStep < 1e-5 && rotationStep < 1e-5)
			break;
	}

	const glm::dmat4 delta= pose * priorPoseInv;
	const double correctionTranslation= glm::length(glm::dvec3(delta[3]));
	const double correctionRotation= SmallMotion::rotationAngleDeg(glm::dmat3(delta));
	const bool bAccepted= count >= minCorrespondences && std::isfinite(rmsAfter)
						  && correctionTranslation <= (double)config.maxCorrectionM
						  && correctionRotation <= (double)config.maxCorrectionDeg;

	RegistrationResult result;
	result.cameraToWorld= bAccepted ? glm::mat4(pose) : priorCameraToWorld;
	result.bAccepted= bAccepted;
	result.iterations= lastIteration;
	result.correspondences= count;
	result.rmsBeforeM= (float)rmsBefore;
	result.rmsAfterM= (float)rmsAfter;
	result.translationM= (float)correctionTranslation;
	result.rotationDeg= (float)correctionRotation;
	return result;
}

#include "FrameCoupledAlignment.h"

#include "glm/ext/quaternion_double.hpp"
#include "glm/ext/vector_double3.hpp"
#include "glm/ext/vector_double4.hpp"
#include "glm/gtc/quaternion.hpp"

glm::dmat4 FrameCoupledAlignment::computeWorldToStageSample(const glm::dmat4& markerInCamera,
															const glm::mat4& cameraInWorld)
{
	const glm::dmat4 cameraInStage= glm::inverse(markerInCamera);
	return cameraInStage * glm::inverse(glm::dmat4(cameraInWorld));
}

glm::dmat4 FrameCoupledAlignment::averageWorldToStage(const std::vector<glm::dmat4>& samples)
{
	if (samples.empty())
		return glm::dmat4(1.0);

	glm::dvec3 averageTranslation(0.0);
	glm::dquat averageRotation= glm::quat_cast(samples[0]);

	for (size_t index= 0; index < samples.size(); ++index)
	{
		const glm::dmat4& sample= samples[index];
		averageTranslation+= glm::dvec3(sample[3]);

		if (index == 0)
			continue;

		glm::dquat sampleRotation= glm::quat_cast(sample);
		if (glm::dot(averageRotation, sampleRotation) < 0.0)
		{
			sampleRotation= -sampleRotation;
		}

		// Running mean: weighting sample i by 1/(i+1) leaves the average of
		// everything seen so far
		averageRotation= glm::slerp(averageRotation, sampleRotation, 1.0 / (double)(index + 1));
	}
	averageTranslation/= (double)samples.size();

	glm::dmat4 average= glm::mat4_cast(glm::normalize(averageRotation));
	average[3]= glm::dvec4(averageTranslation, 1.0);
	return average;
}

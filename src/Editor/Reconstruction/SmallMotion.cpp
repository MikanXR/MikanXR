#include "SmallMotion.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace SmallMotionInternal
{
// Cross-product matrix of v, so skew(v) * p == cross(v, p). glm indexes
// matrices [column][row].
glm::dmat3 skew(const glm::dvec3& v)
{
	glm::dmat3 K(0.0);
	K[1][0]= -v.z;
	K[2][0]= v.y;
	K[0][1]= v.z;
	K[2][1]= -v.x;
	K[0][2]= -v.y;
	K[1][2]= v.x;
	return K;
}

// Rotation angle in radians from the trace, clamped against rounding
double rotationAngleRad(const glm::dmat3& rotation)
{
	const double trace= rotation[0][0] + rotation[1][1] + rotation[2][2];
	return std::acos(std::clamp((trace - 1.0) / 2.0, -1.0, 1.0));
}
} // namespace SmallMotionInternal

namespace SmallMotion
{
glm::dmat4 expSe3(const double twist[6])
{
	const glm::dvec3 omega(twist[0], twist[1], twist[2]);
	const glm::dvec3 translation(twist[3], twist[4], twist[5]);
	const double angle= glm::length(omega);
	const glm::dmat3 K= SmallMotionInternal::skew(omega);
	const glm::dmat3 identity(1.0);

	glm::dmat3 R;
	glm::dmat3 V;
	if (angle < 1e-9)
	{
		R= identity + K;
		V= identity;
	}
	else
	{
		const glm::dmat3 K2= K * K;
		const double angleSq= angle * angle;
		const double sinAngle= std::sin(angle);
		const double cosAngle= std::cos(angle);
		R= identity + (sinAngle / angle) * K + ((1.0 - cosAngle) / angleSq) * K2;
		V= identity + ((1.0 - cosAngle) / angleSq) * K + ((angle - sinAngle) / (angleSq * angle)) * K2;
	}

	// The rotation fills the upper-left block, the translation the last column
	glm::dmat4 T(R);
	T[3]= glm::dvec4(V * translation, 1.0);
	return T;
}

glm::dvec3 logSo3(const glm::dmat3& rotation)
{
	const double angle= SmallMotionInternal::rotationAngleRad(rotation);
	if (angle < 1e-9)
		return glm::dvec3(0.0);

	// Antisymmetric part R(2,1)-R(1,2), R(0,2)-R(2,0), R(1,0)-R(0,1) in
	// [row, col] terms, read through glm's [column][row] indexing
	const glm::dvec3 axis(rotation[1][2] - rotation[2][1], rotation[2][0] - rotation[0][2],
						  rotation[0][1] - rotation[1][0]);
	return axis / (2.0 * std::sin(angle)) * angle;
}

double rotationAngleDeg(const glm::dmat3& rotation)
{
	return glm::degrees(SmallMotionInternal::rotationAngleRad(rotation));
}
} // namespace SmallMotion

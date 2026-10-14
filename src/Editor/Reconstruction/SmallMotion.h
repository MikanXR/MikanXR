#pragma once

#include "glm/ext/matrix_double3x3.hpp"
#include "glm/ext/matrix_double4x4.hpp"
#include "glm/ext/vector_double3.hpp"

// Rigid-motion helpers for small corrections, in doubles.
namespace SmallMotion
{
// Exponential of a twist (rotation vector in twist[0..2], translation in
// twist[3..5]) to a 4x4 rigid transform
glm::dmat4 expSe3(const double twist[6]);
// Rotation vector of a rotation matrix (the log map), zero for identity
glm::dvec3 logSo3(const glm::dmat3& rotation);
// Angle of a rotation matrix in degrees
double rotationAngleDeg(const glm::dmat3& rotation);
} // namespace SmallMotion

#include "MeshCoverage.h"

#include "ParallelFor.h"

#include "glm/geometric.hpp"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <limits>

namespace MeshCoverageInternal
{
const float k_pi= 3.14159265358979f;
const int k_azimuthBins= 8;

void saturatingIncrement(uint16_t& count)
{
	if (count < std::numeric_limits<uint16_t>::max())
		++count;
}

// The bin a direction to a camera falls in around a vertex's normal: 0 when
// it is within the frontal cone, else 1 + its azimuth octant about the
// normal in a tangent frame fixed by the normal alone
int directionBin(const glm::vec3& normal, const glm::vec3& toCamera, float facing, float frontalCos)
{
	if (facing >= frontalCos)
		return 0;

	const glm::vec3 reference= std::fabs(normal.y) < 0.9f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);
	const glm::vec3 tangent= glm::normalize(glm::cross(reference, normal));
	const glm::vec3 bitangent= glm::cross(normal, tangent);
	const float azimuth= std::atan2(glm::dot(toCamera, bitangent), glm::dot(toCamera, tangent));
	const int octant= (int)std::floor((azimuth + k_pi) / (2.f * k_pi) * (float)k_azimuthBins);
	return 1 + std::clamp(octant, 0, k_azimuthBins - 1);
}
} // namespace MeshCoverageInternal

void MeshCoverageResult::remap(const std::vector<uint32_t>& sourceVertex)
{
	auto remapArray= [&sourceVertex](auto& values)
	{
		std::decay_t<decltype(values)> remapped(sourceVertex.size());
		for (size_t vertex= 0; vertex < sourceVertex.size(); ++vertex)
			remapped[vertex]= values[sourceVertex[vertex]];
		values= std::move(remapped);
	};
	remapArray(support);
	remapArray(supportHigh);
	remapArray(carve);
	remapArray(directions);
	remapArray(bestCos);
	remapArray(nearest);
	remapArray(coverageClass);
}

MeshCoverage::MeshCoverage(const TriangleMesh& mesh, float voxelSize, const MeshCoverageSettings& settings)
	: m_mesh(mesh)
	, m_settings(settings)
{
	using namespace MeshCoverageInternal;

	m_band= std::max(settings.supportBand, settings.bandVoxels * voxelSize);
	m_frontalCos= std::cos(settings.frontalDegrees * k_pi / 180.f);

	const size_t vertexCount= mesh.vertices.size();
	m_support.assign(vertexCount, 0);
	m_supportHigh.assign(vertexCount, 0);
	m_carve.assign(vertexCount, 0);
	m_directionMask.assign(vertexCount, 0);
	m_bestCos.assign(vertexCount, 0.f);
	m_nearest.assign(vertexCount, 0.f);
}

void MeshCoverage::accumulateView(const CoverageView& view)
{
	using namespace MeshCoverageInternal;

	if (view.depth.empty() || view.depth.type() != CV_32FC1 || view.confidence.size() != view.depth.size()
		|| view.confidence.type() != CV_8UC1 || view.keep.size() != view.depth.size() || view.keep.type() != CV_8UC1
		|| m_mesh.normals.size() != m_mesh.vertices.size())
	{
		return;
	}

	const DepthViewFrame frame= DepthProjection::makeFrame(view.cameraToWorld);
	const int width= view.depth.cols;
	const int height= view.depth.rows;

	// Each vertex writes only its own slots, so the vertices split across
	// the threads with no shared writes
	parallelFor((int)m_mesh.vertices.size(),
				[&](int begin, int end)
				{
					for (int vertex= begin; vertex < end; ++vertex)
					{
						const glm::vec3& point= m_mesh.vertices[vertex];
						float pointDepth= 0.f;
						int u= 0;
						int v= 0;
						if (!DepthProjection::projectToPixel(frame, view.depthIntrinsics, width, height, point,
															 pointDepth, u, v))
						{
							continue;
						}

						const float measured= view.depth.at<float>(v, u);
						const uint8_t confidence= view.confidence.at<uint8_t>(v, u);
						if (view.keep.at<uint8_t>(v, u) == 0 || confidence < 1 || !(measured >= view.depthMin)
							|| !(measured <= view.depthMax))
						{
							continue;
						}

						const glm::vec3& normal= m_mesh.normals[vertex];
						const glm::vec3 toCamera= frame.cameraPosition - point;
						const float distance= glm::length(toCamera);
						if (!(distance > 0.f))
							continue;
						const glm::vec3 direction= toCamera / distance;
						const float facing= glm::dot(normal, direction);
						if (!(facing > 0.f))
							continue;

						if (measured > pointDepth + m_band)
						{
							saturatingIncrement(m_carve[vertex]);
						}
						else if (measured >= pointDepth - m_band)
						{
							saturatingIncrement(m_support[vertex]);
							if (confidence >= 2)
								saturatingIncrement(m_supportHigh[vertex]);
							m_directionMask[vertex]|=
								(uint16_t)(1u << directionBin(normal, direction, facing, m_frontalCos));
							m_bestCos[vertex]= std::max(m_bestCos[vertex], facing);
							m_nearest[vertex]=
								m_nearest[vertex] > 0.f ? std::min(m_nearest[vertex], distance) : distance;
						}
					}
				},
				4096);
}

eSurfaceCoverage MeshCoverage::classify(const MeshCoverageSettings& settings, int support, int carve, int directions,
										float bestCos)
{
	if (support <= 0)
		return eSurfaceCoverage::unseen;

	const float carveShare= (float)carve / (float)(support + carve);
	if (support >= settings.minSupport && directions >= settings.minDirections && bestCos >= settings.minBestCos
		&& carveShare <= settings.maxCarveShare)
	{
		return eSurfaceCoverage::good;
	}
	return eSurfaceCoverage::weak;
}

void MeshCoverage::finish(MeshCoverageResult& outResult) const
{
	const size_t vertexCount= m_mesh.vertices.size();
	outResult.clear();
	outResult.support= m_support;
	outResult.supportHigh= m_supportHigh;
	outResult.carve= m_carve;
	outResult.bestCos= m_bestCos;
	outResult.nearest= m_nearest;
	outResult.directions.resize(vertexCount);
	outResult.coverageClass.resize(vertexCount);
	for (size_t vertex= 0; vertex < vertexCount; ++vertex)
	{
		const int directions= (int)std::bitset<16>(m_directionMask[vertex]).count();
		outResult.directions[vertex]= (uint8_t)directions;
		outResult.coverageClass[vertex]=
			(uint8_t)classify(m_settings, m_support[vertex], m_carve[vertex], directions, m_bestCos[vertex]);
	}

	double classArea[3]= {0.0, 0.0, 0.0};
	const size_t triangleCount= m_mesh.getTriangleCount();
	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		const uint32_t* corners= &m_mesh.indices[triangle * 3];
		const glm::vec3& a= m_mesh.vertices[corners[0]];
		const glm::vec3& b= m_mesh.vertices[corners[1]];
		const glm::vec3& c= m_mesh.vertices[corners[2]];
		const double cornerArea= 0.5 * (double)glm::length(glm::cross(b - a, c - a)) / 3.0;
		for (int corner= 0; corner < 3; ++corner)
			classArea[outResult.coverageClass[corners[corner]]]+= cornerArea;
	}
	const double totalArea= classArea[0] + classArea[1] + classArea[2];
	if (totalArea > 0.0)
	{
		outResult.areaUnseen= (float)(classArea[(int)eSurfaceCoverage::unseen] / totalArea);
		outResult.areaWeak= (float)(classArea[(int)eSurfaceCoverage::weak] / totalArea);
		outResult.areaGood= (float)(classArea[(int)eSurfaceCoverage::good] / totalArea);
	}
}

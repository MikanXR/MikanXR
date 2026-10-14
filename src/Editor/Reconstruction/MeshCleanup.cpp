#include "MeshCleanup.h"

#include "glm/geometric.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

namespace MeshCleanupHelpers
{
// Keep the triangles whose flag is set, preserving their order
static void compactTriangles(TriangleMesh& mesh, const std::vector<uint8_t>& keepTriangle)
{
	const size_t triangleCount= mesh.getTriangleCount();
	size_t writeIndex= 0;
	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		if (!keepTriangle[triangle])
			continue;

		mesh.indices[writeIndex * 3 + 0]= mesh.indices[triangle * 3 + 0];
		mesh.indices[writeIndex * 3 + 1]= mesh.indices[triangle * 3 + 1];
		mesh.indices[writeIndex * 3 + 2]= mesh.indices[triangle * 3 + 2];
		++writeIndex;
	}
	mesh.indices.resize(writeIndex * 3);
}

static uint32_t findRoot(std::vector<uint32_t>& parents, uint32_t vertex)
{
	uint32_t root= vertex;
	while (parents[root] != root)
	{
		root= parents[root];
	}

	// Path compression
	while (parents[vertex] != root)
	{
		const uint32_t next= parents[vertex];
		parents[vertex]= root;
		vertex= next;
	}

	return root;
}

static void unionRoots(std::vector<uint32_t>& parents, std::vector<uint32_t>& ranks, uint32_t a, uint32_t b)
{
	uint32_t rootA= findRoot(parents, a);
	uint32_t rootB= findRoot(parents, b);
	if (rootA == rootB)
		return;

	if (ranks[rootA] < ranks[rootB])
		std::swap(rootA, rootB);
	parents[rootB]= rootA;
	if (ranks[rootA] == ranks[rootB])
		ranks[rootA]++;
}
} // namespace MeshCleanupHelpers

int MeshCleanup::removeDegenerateTriangles(TriangleMesh& mesh)
{
	const size_t triangleCount= mesh.getTriangleCount();
	const size_t vertexCount= mesh.vertices.size();
	std::vector<uint8_t> keepTriangle(triangleCount, 1);
	int removed= 0;

	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		const uint32_t i0= mesh.indices[triangle * 3 + 0];
		const uint32_t i1= mesh.indices[triangle * 3 + 1];
		const uint32_t i2= mesh.indices[triangle * 3 + 2];

		bool bDegenerate= i0 == i1 || i1 == i2 || i0 == i2;
		bDegenerate|= i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount;
		if (!bDegenerate)
		{
			const glm::vec3& a= mesh.vertices[i0];
			const glm::vec3& b= mesh.vertices[i1];
			const glm::vec3& c= mesh.vertices[i2];
			bDegenerate= glm::length(glm::cross(b - a, c - a)) < 1e-12f;
		}

		if (bDegenerate)
		{
			keepTriangle[triangle]= 0;
			++removed;
		}
	}

	if (removed > 0)
	{
		MeshCleanupHelpers::compactTriangles(mesh, keepTriangle);
	}

	return removed;
}

int MeshCleanup::removeDuplicateTriangles(TriangleMesh& mesh)
{
	const size_t triangleCount= mesh.getTriangleCount();

	// Sort the triangles by their sorted index triple, then by original order,
	// so the first occurrence of each triple survives
	struct TriangleKey
	{
		std::array<uint32_t, 3> sortedIndices;
		uint32_t triangle;
	};
	std::vector<TriangleKey> keys(triangleCount);
	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		TriangleKey& key= keys[triangle];
		key.sortedIndices= {mesh.indices[triangle * 3 + 0], mesh.indices[triangle * 3 + 1],
							mesh.indices[triangle * 3 + 2]};
		std::sort(key.sortedIndices.begin(), key.sortedIndices.end());
		key.triangle= (uint32_t)triangle;
	}
	std::sort(keys.begin(), keys.end(),
			  [](const TriangleKey& a, const TriangleKey& b)
			  {
				  if (a.sortedIndices != b.sortedIndices)
					  return a.sortedIndices < b.sortedIndices;
				  return a.triangle < b.triangle;
			  });

	std::vector<uint8_t> keepTriangle(triangleCount, 1);
	int removed= 0;
	for (size_t keyIndex= 1; keyIndex < keys.size(); ++keyIndex)
	{
		if (keys[keyIndex].sortedIndices == keys[keyIndex - 1].sortedIndices)
		{
			keepTriangle[keys[keyIndex].triangle]= 0;
			++removed;
		}
	}

	if (removed > 0)
	{
		MeshCleanupHelpers::compactTriangles(mesh, keepTriangle);
	}

	return removed;
}

int MeshCleanup::removeSmallComponents(TriangleMesh& mesh, int minTriangles)
{
	const size_t triangleCount= mesh.getTriangleCount();
	const size_t vertexCount= mesh.vertices.size();
	if (triangleCount == 0 || minTriangles <= 1)
		return 0;

	// Union-find over vertices: the three vertices of a triangle share a component
	std::vector<uint32_t> parents(vertexCount);
	std::iota(parents.begin(), parents.end(), 0u);
	std::vector<uint32_t> ranks(vertexCount, 0);
	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		const uint32_t i0= mesh.indices[triangle * 3 + 0];
		const uint32_t i1= mesh.indices[triangle * 3 + 1];
		const uint32_t i2= mesh.indices[triangle * 3 + 2];
		MeshCleanupHelpers::unionRoots(parents, ranks, i0, i1);
		MeshCleanupHelpers::unionRoots(parents, ranks, i0, i2);
	}

	// Component size in triangles, counted at each component's root vertex
	std::vector<uint32_t> triangleRoots(triangleCount);
	std::vector<int> componentTriangles(vertexCount, 0);
	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		const uint32_t root= MeshCleanupHelpers::findRoot(parents, mesh.indices[triangle * 3]);
		triangleRoots[triangle]= root;
		componentTriangles[root]++;
	}

	std::vector<uint8_t> keepTriangle(triangleCount, 1);
	int removed= 0;
	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		if (componentTriangles[triangleRoots[triangle]] < minTriangles)
		{
			keepTriangle[triangle]= 0;
			++removed;
		}
	}

	if (removed > 0)
	{
		MeshCleanupHelpers::compactTriangles(mesh, keepTriangle);
	}

	return removed;
}

int MeshCleanup::removeUnreferencedVertices(TriangleMesh& mesh)
{
	const size_t vertexCount= mesh.vertices.size();
	const bool bHasNormals= mesh.normals.size() == vertexCount;

	std::vector<uint8_t> referenced(vertexCount, 0);
	for (const uint32_t index : mesh.indices)
	{
		referenced[index]= 1;
	}

	const uint32_t k_unreferencedVertex= 0xFFFFFFFFu;
	std::vector<uint32_t> remap(vertexCount, k_unreferencedVertex);
	uint32_t writeIndex= 0;
	for (size_t vertex= 0; vertex < vertexCount; ++vertex)
	{
		if (!referenced[vertex])
			continue;

		remap[vertex]= writeIndex;
		mesh.vertices[writeIndex]= mesh.vertices[vertex];
		if (bHasNormals)
		{
			mesh.normals[writeIndex]= mesh.normals[vertex];
		}
		++writeIndex;
	}

	const int removed= (int)(vertexCount - writeIndex);
	if (removed == 0)
		return 0;

	mesh.vertices.resize(writeIndex);
	if (bHasNormals)
	{
		mesh.normals.resize(writeIndex);
	}
	for (uint32_t& index : mesh.indices)
	{
		index= remap[index];
	}

	return removed;
}

void MeshCleanup::computeVertexNormals(TriangleMesh& mesh)
{
	mesh.normals.assign(mesh.vertices.size(), glm::vec3(0.f));

	// The unnormalized face normal has length twice the triangle's area, so
	// summing it weights each face by area
	const size_t triangleCount= mesh.getTriangleCount();
	for (size_t triangle= 0; triangle < triangleCount; ++triangle)
	{
		const uint32_t i0= mesh.indices[triangle * 3 + 0];
		const uint32_t i1= mesh.indices[triangle * 3 + 1];
		const uint32_t i2= mesh.indices[triangle * 3 + 2];
		const glm::vec3 faceNormal=
			glm::cross(mesh.vertices[i1] - mesh.vertices[i0], mesh.vertices[i2] - mesh.vertices[i0]);
		mesh.normals[i0]+= faceNormal;
		mesh.normals[i1]+= faceNormal;
		mesh.normals[i2]+= faceNormal;
	}

	for (glm::vec3& normal : mesh.normals)
	{
		const float length= glm::length(normal);
		if (length > 0.f)
		{
			normal/= length;
		}
	}
}

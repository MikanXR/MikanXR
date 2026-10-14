#pragma once

#include "ReconstructionTypes.h"

// In-place passes over a TriangleMesh. Each returns what it removed.
class MeshCleanup
{
public:
	// A triangle with a repeated index or zero area
	static int removeDegenerateTriangles(TriangleMesh& mesh);
	// Two triangles over the same three vertices, in any order
	static int removeDuplicateTriangles(TriangleMesh& mesh);
	// Connected components (by shared vertices) with fewer than minTriangles
	// triangles. Returns the triangle count removed.
	static int removeSmallComponents(TriangleMesh& mesh, int minTriangles);
	// Vertices no triangle references; indices are remapped
	static int removeUnreferencedVertices(TriangleMesh& mesh);
	// Area-weighted average of the adjacent face normals, normalized
	static void computeVertexNormals(TriangleMesh& mesh);
};

// The one translation unit that compiles the cgltf reader and writer, kept
// out of the unity build so the implementation macros never leak into
// another source file. cgltf.h compiles its implementation every time it is
// included with the macro set, and cgltf_write.h includes it again, so the
// macro comes off between the two.
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION

#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf_write.h"

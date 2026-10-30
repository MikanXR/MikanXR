#include "Constants_OcclusionMatteBuild.h"

// Named per stage: the unity build concatenates the Constants_*.cpp files,
// so file-statics collide by name
static const std::string k_OcclusionMatteBuildMenuStateStringTable[(int)eOcclusionMatteBuildMenuState::COUNT]= {
	"inactive", "verifyTake", "building", "failed", "buildComplete",
};
const std::string* k_OcclusionMatteBuildMenuStateStrings= k_OcclusionMatteBuildMenuStateStringTable;

static const std::string k_OcclusionMatteModelStringTable[(int)eOcclusionMatteModel::COUNT]= {
	"small",
	"tiny",
};
const std::string* k_OcclusionMatteModelStrings= k_OcclusionMatteModelStringTable;

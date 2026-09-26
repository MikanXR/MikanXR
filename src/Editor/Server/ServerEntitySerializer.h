#pragma once

#include "IEntityAccessor.h"
#include "SerializationVisitor.h"

namespace Serialization
{
bool serializeFromEntity(IEntityAccessorConstPtr entityAccessor, void* instance, StructTypeHandle structType,
						 std::string& outErrorMsg);
};

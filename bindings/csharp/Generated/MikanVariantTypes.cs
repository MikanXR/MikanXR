// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanVariantType
	{
		INVALID_TYPE= 0,
		BOOL_TYPE= 1,
		UBYTE_TYPE= 2,
		USHORT_TYPE= 3,
		INT_TYPE= 4,
		LONG_TYPE= 5,
		FLOAT_TYPE= 6,
		DOUBLE_TYPE= 7,
		MK_STRING_TYPE= 8,
		VECTOR2F_TYPE= 9,
		VECTOR3F_TYPE= 10,
		VECTOR4F_TYPE= 11,
		QUATERNIONF_TYPE= 12,
		MATRIX4F_TYPE= 13,
		VECTOR2D_TYPE= 14,
		VECTOR3D_TYPE= 15,
		VECTOR4D_TYPE= 16,
		QUATERNIOND_TYPE= 17,
		BOOL_ARRAY_TYPE= 18,
		UBYTE_ARRAY_TYPE= 19,
		INT_ARRAY_TYPE= 20,
		FLOAT_ARRAY_TYPE= 21,
		STRING_ARRAY_TYPE= 22,
		STRING_MAP_TYPE= 23,
		POLYMORPHIC_OBJECT_TYPE= 24,
	};

	public class MikanBoolArrayValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public List<bool> value;
	};

	public class MikanBoolValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public bool value;
	};

	public class MikanDoubleValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public double value;
	};

	public class MikanFloatArrayValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public List<float> value;
	};

	public class MikanFloatValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public float value;
	};

	public class MikanIntArrayValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public List<int> value;
	};

	public class MikanIntValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public int value;
	};

	public class MikanLongValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public long value;
	};

	public class MikanMatrix4fValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanMatrix4f value;
	};

	public class MikanQuatdValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanQuatd value;
	};

	public class MikanQuatfValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanQuatf value;
	};

	public class MikanStringArrayValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public List<string> value;
	};

	public class MikanStringMapValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public Dictionary<string, string> value;
	};

	public class MikanStringValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public string value;
	};

	public class MikanUByteArrayValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public List<byte> value;
	};

	public class MikanUByteValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public byte value;
	};

	public class MikanUShortValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public ushort value;
	};

	public class MikanVariant
	{
		[MikanFieldOrder(0)] public MikanVariantType value_type;
		[MikanFieldOrder(1)] public PolymorphicObject value_ptr;
	};

	public class MikanVariantBase : PolymorphicStruct
	{
	};

	public class MikanVector2dValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanVector2d value;
	};

	public class MikanVector2fValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanVector2f value;
	};

	public class MikanVector3dValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanVector3d value;
	};

	public class MikanVector3fValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanVector3f value;
	};

	public class MikanVector4dValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanVector4d value;
	};

	public class MikanVector4fValue : MikanVariantBase
	{
		[MikanFieldOrder(0)] public MikanVector4f value;
	};

}

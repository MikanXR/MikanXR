// This file is auto generated. DO NOT EDIT.

import { PolymorphicObject, PolymorphicStruct } from './../PolymorphicObject.js';
import { MikanMatrix4f, MikanQuatd, MikanQuatf, MikanVector2d, MikanVector2f, MikanVector3d, MikanVector3f, MikanVector4d, MikanVector4f } from './MikanMathTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanVariantType {
  INVALID_TYPE = 0,
  BOOL_TYPE = 1,
  UBYTE_TYPE = 2,
  USHORT_TYPE = 3,
  INT_TYPE = 4,
  LONG_TYPE = 5,
  FLOAT_TYPE = 6,
  DOUBLE_TYPE = 7,
  MK_STRING_TYPE = 8,
  VECTOR2F_TYPE = 9,
  VECTOR3F_TYPE = 10,
  VECTOR4F_TYPE = 11,
  QUATERNIONF_TYPE = 12,
  MATRIX4F_TYPE = 13,
  VECTOR2D_TYPE = 14,
  VECTOR3D_TYPE = 15,
  VECTOR4D_TYPE = 16,
  QUATERNIOND_TYPE = 17,
  BOOL_ARRAY_TYPE = 18,
  UBYTE_ARRAY_TYPE = 19,
  INT_ARRAY_TYPE = 20,
  FLOAT_ARRAY_TYPE = 21,
  STRING_ARRAY_TYPE = 22,
  STRING_MAP_TYPE = 23,
  POLYMORPHIC_OBJECT_TYPE = 24
}

export class MikanVariantBase extends PolymorphicStruct {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanBoolArrayValue extends MikanVariantBase {
  value: boolean[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'boolean', isArray: true }
  ];
}

export class MikanBoolValue extends MikanVariantBase {
  value: boolean = false;

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'boolean' }
  ];
}

export class MikanDoubleValue extends MikanVariantBase {
  value: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'double' }
  ];
}

export class MikanFloatArrayValue extends MikanVariantBase {
  value: number[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'float', isArray: true }
  ];
}

export class MikanFloatValue extends MikanVariantBase {
  value: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'float' }
  ];
}

export class MikanIntArrayValue extends MikanVariantBase {
  value: number[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'int32', isArray: true }
  ];
}

export class MikanIntValue extends MikanVariantBase {
  value: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'int32' }
  ];
}

export class MikanLongValue extends MikanVariantBase {
  value: any = null;

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'int64' }
  ];
}

export class MikanMatrix4fValue extends MikanVariantBase {
  value: MikanMatrix4f = new MikanMatrix4f();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanMatrix4f' }
  ];
}

export class MikanQuatdValue extends MikanVariantBase {
  value: MikanQuatd = new MikanQuatd();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanQuatd' }
  ];
}

export class MikanQuatfValue extends MikanVariantBase {
  value: MikanQuatf = new MikanQuatf();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanQuatf' }
  ];
}

export class MikanStringArrayValue extends MikanVariantBase {
  value: string[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'string', isArray: true }
  ];
}

export class MikanStringMapValue extends MikanVariantBase {
  value: Record<string, string> = {};

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'Map', isMap: true, keyType: 'string', valueType: 'string' }
  ];
}

export class MikanStringValue extends MikanVariantBase {
  value: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'string' }
  ];
}

export class MikanUByteArrayValue extends MikanVariantBase {
  value: number[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'uint8', isArray: true }
  ];
}

export class MikanUByteValue extends MikanVariantBase {
  value: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'uint8' }
  ];
}

export class MikanUShortValue extends MikanVariantBase {
  value: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'uint16' }
  ];
}

export class MikanVariant {
  value_type: MikanVariantType = MikanVariantType.INVALID_TYPE;
  value_ptr: PolymorphicObject = new PolymorphicObject();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value_type', type: 'enum:MikanVariantType' },
    { name: 'value_ptr', type: 'PolymorphicObject' }
  ];
}

export class MikanVector2dValue extends MikanVariantBase {
  value: MikanVector2d = new MikanVector2d();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanVector2d' }
  ];
}

export class MikanVector2fValue extends MikanVariantBase {
  value: MikanVector2f = new MikanVector2f();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanVector2f' }
  ];
}

export class MikanVector3dValue extends MikanVariantBase {
  value: MikanVector3d = new MikanVector3d();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanVector3d' }
  ];
}

export class MikanVector3fValue extends MikanVariantBase {
  value: MikanVector3f = new MikanVector3f();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanVector3f' }
  ];
}

export class MikanVector4dValue extends MikanVariantBase {
  value: MikanVector4d = new MikanVector4d();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanVector4d' }
  ];
}

export class MikanVector4fValue extends MikanVariantBase {
  value: MikanVector4f = new MikanVector4f();

  static __serializationMetadata: SerializationField[] = [
    { name: 'value', type: 'MikanVector4f' }
  ];
}


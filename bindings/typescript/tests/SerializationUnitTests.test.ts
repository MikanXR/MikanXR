/**
 * Serialization unit tests for TypeScript bindings
 * Mirrors the C# tests in SerializationUnitTests.cs
 */

import {
  SerializationTestObject,
  SerializationPoint2d,
  SerializationPoint3d,
  buildSerializationTestObject,
  verifySerializationTestObject
} from './SerializationTestClasses';

import {
  serializeToJsonString,
  deserializeFromJsonString,
  TypeRegistry
} from '../Serialization';

import {
  MikanVariant,
  MikanVariantType,
  MikanIntValue,
  MikanFloatValue,
  MikanStringValue,
  MikanVector3fValue,
} from '../types/MikanVariantTypes';
import { MikanVector3f } from '../types/MikanMathTypes';
import { SystemGetValuesResponse } from '../types/MikanPropertyRequests';
import { MikanMarkerSystemValues, MikanMarkerDictionaryType } from '../types/MikanMarkerTypes';

// Register types for polymorphic deserialization
TypeRegistry.register('SerializationPoint2d', SerializationPoint2d);
TypeRegistry.register('SerializationPoint3d', SerializationPoint3d);
TypeRegistry.register('SerializationTestObject', SerializationTestObject);

// Register real types needed for PolymorphicObject tests
TypeRegistry.register('MikanIntValue', MikanIntValue);
TypeRegistry.register('MikanFloatValue', MikanFloatValue);
TypeRegistry.register('MikanStringValue', MikanStringValue);
TypeRegistry.register('MikanVector3fValue', MikanVector3fValue);
TypeRegistry.register('MikanMarkerSystemValues', MikanMarkerSystemValues);
TypeRegistry.register('SystemGetValuesResponse', SystemGetValuesResponse);

describe('SerializationUnitTests', () => {
  describe('JSON Serialization', () => {
    it('should serialize and deserialize test object to/from JSON', () => {
      // Build expected object
      const expected = buildSerializationTestObject();

      // Serialize to JSON string
      const jsonString = serializeToJsonString(expected, SerializationTestObject);

      // Verify JSON was created
      expect(jsonString).toBeDefined();
      expect(jsonString.length).toBeGreaterThan(0);

      // Deserialize from JSON string
      const actual = new SerializationTestObject();
      const canDeserialize = deserializeFromJsonString(
        jsonString,
        actual,
        SerializationTestObject
      );

      // Verify deserialization succeeded
      expect(canDeserialize).toBe(true);

      // Verify the objects match
      const result = verifySerializationTestObject(actual, expected);
      expect(result.success).toBe(true);
      if (!result.success) {
        console.error('Verification failed:', result.message);
      }
    });

    it('should handle empty arrays', () => {
      const testObject = new SerializationTestObject();
      testObject.bool_array = [];
      testObject.int_array = [];
      testObject.point2d_array = [];

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.bool_array.length).toBe(0);
      expect(deserialized.int_array.length).toBe(0);
      expect(deserialized.point2d_array.length).toBe(0);
    });

    it('should handle empty maps', () => {
      const testObject = new SerializationTestObject();
      testObject.int_point_map = {};
      testObject.string_point_map = {};

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(Object.keys(deserialized.int_point_map).length).toBe(0);
      expect(Object.keys(deserialized.string_point_map).length).toBe(0);
    });

    it('should preserve bigint values', () => {
      const testObject = new SerializationTestObject();
      testObject.long_field = 9007199254740991n; // MAX_SAFE_INTEGER as bigint

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.long_field).toBe(9007199254740991n);
    });

    it('should handle negative numbers', () => {
      const testObject = new SerializationTestObject();
      testObject.byte_field = -123;
      testObject.short_field = -1234;
      testObject.int_field = -123456;
      testObject.long_field = -123456789n;
      testObject.float_field = -1.2345;
      testObject.double_field = -1.23456789;

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.byte_field).toBe(-123);
      expect(deserialized.short_field).toBe(-1234);
      expect(deserialized.int_field).toBe(-123456);
      expect(deserialized.long_field).toBe(-123456789n);
      expect(deserialized.float_field).toBeCloseTo(-1.2345);
      expect(deserialized.double_field).toBeCloseTo(-1.23456789);
    });

    it('should handle string with special characters', () => {
      const testObject = new SerializationTestObject();
      testObject.string_field = 'Hello "World"\nNew Line\tTab\\Backslash';

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.string_field).toBe('Hello "World"\nNew Line\tTab\\Backslash');
    });
  });

  describe('Edge Cases', () => {
    it('should handle null polymorphic objects', () => {
      const testObject = new SerializationTestObject();
      testObject.null_ptr_field.setInstance(null as any, '');

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.null_ptr_field.instance).toBeNull();
      expect(deserialized.null_ptr_field.runtimeClassName).toBe('');
    });

    it('should handle boolean edge cases', () => {
      const testObject = new SerializationTestObject();
      testObject.bool_field = false;
      testObject.bool_array = [true, false, true, false];

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.bool_field).toBe(false);
      expect(deserialized.bool_array).toEqual([true, false, true, false]);
    });

    it('should handle zero values', () => {
      const testObject = new SerializationTestObject();
      testObject.byte_field = 0;
      testObject.ubyte_field = 0;
      testObject.short_field = 0;
      testObject.ushort_field = 0;
      testObject.int_field = 0;
      testObject.uint_field = 0;
      testObject.long_field = 0n;
      testObject.float_field = 0.0;
      testObject.double_field = 0.0;

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.byte_field).toBe(0);
      expect(deserialized.ubyte_field).toBe(0);
      expect(deserialized.short_field).toBe(0);
      expect(deserialized.ushort_field).toBe(0);
      expect(deserialized.int_field).toBe(0);
      expect(deserialized.uint_field).toBe(0);
      expect(deserialized.long_field).toBe(0n);
      expect(deserialized.float_field).toBe(0.0);
      expect(deserialized.double_field).toBe(0.0);
    });

    it('should handle empty string', () => {
      const testObject = new SerializationTestObject();
      testObject.string_field = '';

      const jsonString = serializeToJsonString(testObject, SerializationTestObject);
      const deserialized = new SerializationTestObject();
      const success = deserializeFromJsonString(
        jsonString,
        deserialized,
        SerializationTestObject
      );

      expect(success).toBe(true);
      expect(deserialized.string_field).toBe('');
    });
  });

  describe('PolymorphicObject Deserialization', () => {
    // Helpers to build a MikanVariant with a typed value_ptr
    function makeIntVariant(n: number): MikanVariant {
      const v = new MikanVariant();
      v.value_type = MikanVariantType.INT_TYPE;
      const inner = new MikanIntValue();
      inner.value = n;
      v.value_ptr.setInstance(inner, 'MikanIntValue');
      return v;
    }

    function makeFloatVariant(n: number): MikanVariant {
      const v = new MikanVariant();
      v.value_type = MikanVariantType.FLOAT_TYPE;
      const inner = new MikanFloatValue();
      inner.value = n;
      v.value_ptr.setInstance(inner, 'MikanFloatValue');
      return v;
    }

    function makeStringVariant(s: string): MikanVariant {
      const v = new MikanVariant();
      v.value_type = MikanVariantType.MK_STRING_TYPE;
      const inner = new MikanStringValue();
      inner.value = s;
      v.value_ptr.setInstance(inner, 'MikanStringValue');
      return v;
    }

    function makeVector3fVariant(x: number, y: number, z: number): MikanVariant {
      const v = new MikanVariant();
      v.value_type = MikanVariantType.VECTOR3F_TYPE;
      const inner = new MikanVector3fValue();
      inner.value = new MikanVector3f();
      inner.value.x = x;
      inner.value.y = y;
      inner.value.z = z;
      v.value_ptr.setInstance(inner, 'MikanVector3fValue');
      return v;
    }

    describe('JSON', () => {
      it('should deserialize MikanVariant<MikanIntValue> with .instance set', () => {
        const original = makeIntVariant(42);
        const json = serializeToJsonString(original, MikanVariant);
        const result = new MikanVariant();
        expect(deserializeFromJsonString(json, result, MikanVariant)).toBe(true);

        expect(result.value_type).toBe(MikanVariantType.INT_TYPE);
        expect(result.value_ptr.instance).toBeInstanceOf(MikanIntValue);
        expect((result.value_ptr.instance as MikanIntValue).value).toBe(42);
      });

      it('should deserialize MikanVariant<MikanFloatValue> with .instance set', () => {
        const original = makeFloatVariant(3.14);
        const json = serializeToJsonString(original, MikanVariant);
        const result = new MikanVariant();
        expect(deserializeFromJsonString(json, result, MikanVariant)).toBe(true);

        expect(result.value_type).toBe(MikanVariantType.FLOAT_TYPE);
        expect(result.value_ptr.instance).toBeInstanceOf(MikanFloatValue);
        expect((result.value_ptr.instance as MikanFloatValue).value).toBeCloseTo(3.14);
      });

      it('should deserialize MikanVariant<MikanStringValue> with .instance set', () => {
        const original = makeStringVariant('hello world');
        const json = serializeToJsonString(original, MikanVariant);
        const result = new MikanVariant();
        expect(deserializeFromJsonString(json, result, MikanVariant)).toBe(true);

        expect(result.value_type).toBe(MikanVariantType.MK_STRING_TYPE);
        expect(result.value_ptr.instance).toBeInstanceOf(MikanStringValue);
        expect((result.value_ptr.instance as MikanStringValue).value).toBe('hello world');
      });

      it('should deserialize MikanVariant<MikanVector3fValue> with .instance set', () => {
        const original = makeVector3fVariant(1.0, 2.0, 3.0);
        const json = serializeToJsonString(original, MikanVariant);
        const result = new MikanVariant();
        expect(deserializeFromJsonString(json, result, MikanVariant)).toBe(true);

        expect(result.value_type).toBe(MikanVariantType.VECTOR3F_TYPE);
        expect(result.value_ptr.instance).toBeInstanceOf(MikanVector3fValue);
        const vec = (result.value_ptr.instance as MikanVector3fValue).value;
        expect(vec.x).toBeCloseTo(1.0);
        expect(vec.y).toBeCloseTo(2.0);
        expect(vec.z).toBeCloseTo(3.0);
      });

      it('should deserialize SystemGetValuesResponse.valuesObject with .instance set', () => {
        // Simulate the JSON a server would send for a SystemGetValuesResponse
        // containing MikanMarkerSystemValues as the polymorphic payload
        const markerValues = new MikanMarkerSystemValues();
        markerValues.aruco_dictionary_type = MikanMarkerDictionaryType.DICT_5x5;
        markerValues.charuco_rows = 7;
        markerValues.charuco_cols = 5;
        markerValues.charuco_square_length_mm = 25.0;
        markerValues.charuco_marker_length_mm = 18.5;

        const response = new SystemGetValuesResponse();
        response.ownerSystem = 'MarkerObjectSystem';
        response.valuesObject.setInstance(markerValues, 'MikanMarkerSystemValues');

        const json = serializeToJsonString(response, SystemGetValuesResponse);
        const result = new SystemGetValuesResponse();
        expect(deserializeFromJsonString(json, result, SystemGetValuesResponse)).toBe(true);

        // Verify inherited MikanResponse fields were also deserialized
        expect(result.requestId).toBe(response.requestId);
        expect(result.resultCode).toBe(response.resultCode);
        expect(result.ownerSystem).toBe('MarkerObjectSystem');
        expect(result.valuesObject.instance).toBeInstanceOf(MikanMarkerSystemValues);
        const mv = result.valuesObject.instance as MikanMarkerSystemValues;
        expect(mv.aruco_dictionary_type).toBe(MikanMarkerDictionaryType.DICT_5x5);
        expect(mv.charuco_rows).toBe(7);
        expect(mv.charuco_cols).toBe(5);
        expect(mv.charuco_square_length_mm).toBeCloseTo(25.0);
        expect(mv.charuco_marker_length_mm).toBeCloseTo(18.5);
      });
    });
  });

  describe('Type Registry', () => {
    it('should register and retrieve types', () => {
      TypeRegistry.register('TestType', SerializationPoint2d);
      expect(TypeRegistry.has('TestType')).toBe(true);
      expect(TypeRegistry.get('TestType')).toBe(SerializationPoint2d);
    });

    it('should return undefined for unregistered types', () => {
      expect(TypeRegistry.get('NonExistentType')).toBeUndefined();
      expect(TypeRegistry.has('NonExistentType')).toBe(false);
    });
  });
});

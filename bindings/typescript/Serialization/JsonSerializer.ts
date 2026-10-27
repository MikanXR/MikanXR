import {
  ValueAccessor,
  IVisitor,
  visitObject,
  getSerializationMetadata
} from './SerializationUtils.js';
import { PolymorphicObject } from '../PolymorphicObject.js';
import { EnumRegistry } from './EnumRegistry.js';

/**
 * JSON write visitor for serializing objects to JSON
 */
class JsonWriteVisitor implements IVisitor {
  private jsonToken: any;

  constructor(jsonToken: any = {}) {
    this.jsonToken = jsonToken;
  }

  getJsonToken(): any {
    return this.jsonToken;
  }

  visitClass(accessor: ValueAccessor): void {
    const objectValue = accessor.getValueObject();
    const jsonObject: any = {};
    const jsonVisitor = new JsonWriteVisitor(jsonObject);

    visitObject(objectValue, accessor.valueType, jsonVisitor);

    this.setJsonValueFromJsonToken(accessor.valueField, jsonObject);
  }

  visitList(accessor: ValueAccessor): void {
    const list = accessor.getValueObject() as any[];
    const jsonArray: any[] = [];

    if (list) {
      for (const element of list) {
        // Check if element is a complex object with metadata
        const metadata = getSerializationMetadata(element);

        if (metadata && metadata.length > 0) {
          // Complex object - use visitor pattern
          const elementType = element.constructor;
          const elementVisitor = new JsonWriteVisitor({});
          visitObject(element, elementType, elementVisitor);
          jsonArray.push(elementVisitor.getJsonToken());
        } else {
          // Primitive or simple value - add directly
          jsonArray.push(element);
        }
      }
    }

    this.setJsonValueFromJsonToken(accessor.valueField, jsonArray);
  }

  visitDictionary(accessor: ValueAccessor): void {
    const record = accessor.getValueObject() as Record<any, any>;
    const jsonPairArray: any[] = [];

    if (record) {
      for (const key in record) {
        const value = record[key];
        let jsonKey = key;
        let jsonValue = value;

        // Serialize value if it's a complex object
        const valueMetadata = getSerializationMetadata(value);
        if (valueMetadata && valueMetadata.length > 0) {
          const valueVisitor = new JsonWriteVisitor({});
          visitObject(value, value.constructor, valueVisitor);
          jsonValue = valueVisitor.getJsonToken();
        }

        jsonPairArray.push({
          key: jsonKey,
          value: jsonValue
        });
      }
    }

    this.setJsonValueFromJsonToken(accessor.valueField, jsonPairArray);
  }

  visitPolymorphicObject(accessor: ValueAccessor): void {
    const polymorphicObject = accessor.getValueObject() as PolymorphicObject;
    const instance = polymorphicObject?.instance;

    let className = '';
    const jsonRuntimeObject: any = {};

    if (instance) {
      // The name the object was given, since a minified build renames classes
      className = polymorphicObject.runtimeClassName || instance.constructor.name;

      // Serialize the instance
      const jsonVisitor = new JsonWriteVisitor(jsonRuntimeObject);
      visitObject(instance, instance.constructor, jsonVisitor);
    }

    const jsonSerializableObject = {
      class_name: className,
      value: jsonRuntimeObject
    };

    this.setJsonValueFromJsonToken(accessor.valueField, jsonSerializableObject);
  }

  visitEnum(accessor: ValueAccessor): void {
    let enumValue = accessor.getValueObject();
    // The wire carries an enum value by name, as the other runtimes write it. A value of an
    // enum the registry does not know, or one with no name, goes out as its integer.
    if (typeof enumValue === 'number') {
      const fieldMetadata = (accessor as any).fieldMetadata;
      const enumTypeName = fieldMetadata?.type?.startsWith('enum:')
        ? fieldMetadata.type.substring(5)
        : null;
      if (enumTypeName) {
        const enumName = EnumRegistry.intToString(enumTypeName, enumValue);
        if (enumName !== null) {
          enumValue = enumName;
        }
      }
    }
    this.setJsonValueFromJsonToken(accessor.valueField, enumValue);
  }

  visitBool(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<boolean>(accessor);
  }

  visitByte(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  visitUByte(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  visitShort(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  visitUShort(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  visitInt(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  visitUInt(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  // A 64-bit integer stays a bigint in the JSON object, and stringifyJson writes it as a bare
  // integer, which is the only form the editor reads one in
  visitLong(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<bigint>(accessor);
  }

  visitULong(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<bigint>(accessor);
  }

  visitFloat(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  visitDouble(accessor: ValueAccessor): void {
    this.setJsonValueFromAccessor<number>(accessor);
  }

  visitString(accessor: ValueAccessor): void {
    const value = accessor.getValue<string>();
    this.setJsonValueFromJsonToken(accessor.valueField, value ?? '');
  }

  private setJsonValueFromAccessor<T>(accessor: ValueAccessor): void {
    const value = accessor.getValue<T>();
    this.setJsonValueFromJsonToken(accessor.valueField, value);
  }

  private setJsonValueFromJsonToken(sourceField: string | null, token: any): void {
    if (sourceField !== null) {
      this.jsonToken[sourceField] = token;
    } else {
      this.jsonToken = token;
    }
  }
}

/**
 * Serialize an object to JSON string
 */
export function serializeToJsonString(instance: any, instanceType?: any): string {
  const json = serializeToJson(instance, instanceType);
  return stringifyJson(json);
}

/**
 * JSON.stringify, except that a bigint is written as a bare integer rather than refused
 */
export function stringifyJson(value: any): string {
  const jsonString = JSON.stringify(value, (key, fieldValue) => {
    if (typeof fieldValue === 'bigint') {
      return `__BIGINT__${fieldValue.toString()}__BIGINT__`;
    }
    return fieldValue;
  });

  return jsonString.replace(/"__BIGINT__(-?\d+)__BIGINT__"/g, '$1');
}

/**
 * Serialize an object to JSON object
 */
export function serializeToJson(instance: any, instanceType?: any): any {
  const type = instanceType || instance?.constructor;
  const jsonObject: any = {};
  const visitor = new JsonWriteVisitor(jsonObject);

  visitObject(instance, type, visitor);

  return jsonObject;
}

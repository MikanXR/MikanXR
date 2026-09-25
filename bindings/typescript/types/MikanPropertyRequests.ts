// This file is auto generated. DO NOT EDIT.

import { PolymorphicObject, PolymorphicStruct } from './../PolymorphicObject.js';
import { MikanRequest, MikanResponse } from './MikanAPITypes.js';
import { MikanPropertyDescriptor, MikanPropertyValue } from './MikanPropertyTypes.js';
import { MikanVariant } from './MikanVariantTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanPropertyNotifyMode {
  NONE = 0,
  NAME = 1,
  NAME_AND_VALUE = 2
}

export class ComponentGetValuesRequest extends MikanRequest {
  ownerSystem: string = '';
  componentId: number = -1;

  constructor() {
    super();
    this.requestTypeName = 'ComponentGetValuesRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentId', type: 'int32' }
  ];
}

export class ComponentGetValuesResponse extends MikanResponse {
  ownerSystem: string = '';
  componentClassName: string = '';
  valuesObject: PolymorphicObject = new PolymorphicObject();

  constructor() {
    super();
    this.responseTypeName = 'ComponentGetValuesResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentClassName', type: 'string' },
    { name: 'valuesObject', type: 'PolymorphicObject' }
  ];
}

export class ComponentListResponse extends MikanResponse {
  componentIdList: number[] = [];

  constructor() {
    super();
    this.responseTypeName = 'ComponentListResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'componentIdList', type: 'int32', isArray: true }
  ];
}

export class GetComponentListRequest extends MikanRequest {
  ownerSystem: string = '';
  componentClassName: string = '';

  constructor() {
    super();
    this.requestTypeName = 'GetComponentListRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentClassName', type: 'string' }
  ];
}

export class GetPropertyDescriptors extends MikanRequest {
  systemFilter: string = '';
  componentFilter: string = '';
  propertyFilter: string = '';

  constructor() {
    super();
    this.requestTypeName = 'GetPropertyDescriptors';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'systemFilter', type: 'string' },
    { name: 'componentFilter', type: 'string' },
    { name: 'propertyFilter', type: 'string' }
  ];
}

export class PropertyDescriptorResponse extends MikanResponse {
  descriptor_list: MikanPropertyDescriptor[] = [];

  constructor() {
    super();
    this.responseTypeName = 'PropertyDescriptorResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'descriptor_list', type: 'MikanPropertyDescriptor', isArray: true }
  ];
}

export class PropertyGetValueRequest extends MikanRequest {
  ownerSystem: string = '';
  componentId: number = -1;
  fieldName: string = '';

  constructor() {
    super();
    this.requestTypeName = 'PropertyGetValueRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentId', type: 'int32' },
    { name: 'fieldName', type: 'string' }
  ];
}

export class PropertyGetValueResponse extends MikanResponse {
  propertyValue: MikanPropertyValue = new MikanPropertyValue();

  constructor() {
    super();
    this.responseTypeName = 'PropertyGetValueResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'propertyValue', type: 'MikanPropertyValue' }
  ];
}

export class PropertySetValueRequest extends MikanRequest {
  ownerSystem: string = '';
  componentId: number = -1;
  fieldName: string = '';
  fieldValue: MikanVariant = new MikanVariant();

  constructor() {
    super();
    this.requestTypeName = 'PropertySetValueRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentId', type: 'int32' },
    { name: 'fieldName', type: 'string' },
    { name: 'fieldValue', type: 'MikanVariant' }
  ];
}

export class PropertySetValueResponse extends MikanResponse {

  constructor() {
    super();
    this.responseTypeName = 'PropertySetValueResponse';
  }

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class SetPropertyNotifyMode extends MikanRequest {
  systemFilter: string = '';
  componentFilter: string = '';
  propertyFilter: string = '';
  notifyMode: MikanPropertyNotifyMode = MikanPropertyNotifyMode.NONE;

  constructor() {
    super();
    this.requestTypeName = 'SetPropertyNotifyMode';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'systemFilter', type: 'string' },
    { name: 'componentFilter', type: 'string' },
    { name: 'propertyFilter', type: 'string' },
    { name: 'notifyMode', type: 'enum:MikanPropertyNotifyMode' }
  ];
}

export class SystemCreateObjectRequest extends MikanRequest {
  ownerSystem: string = '';
  componentClassName: string = '';
  initParams: PolymorphicObject = new PolymorphicObject();

  constructor() {
    super();
    this.requestTypeName = 'SystemCreateObjectRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentClassName', type: 'string' },
    { name: 'initParams', type: 'PolymorphicObject' }
  ];
}

export class SystemDestroyObjectRequest extends MikanRequest {
  ownerSystem: string = '';
  componentClassName: string = '';
  componentId: number = -1;

  constructor() {
    super();
    this.requestTypeName = 'SystemDestroyObjectRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentClassName', type: 'string' },
    { name: 'componentId', type: 'int32' }
  ];
}

export class SystemGetValuesRequest extends MikanRequest {
  ownerSystem: string = '';

  constructor() {
    super();
    this.requestTypeName = 'SystemGetValuesRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' }
  ];
}

export class SystemGetValuesResponse extends MikanResponse {
  ownerSystem: string = '';
  valuesObject: PolymorphicObject = new PolymorphicObject();

  constructor() {
    super();
    this.responseTypeName = 'SystemGetValuesResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'valuesObject', type: 'PolymorphicObject' }
  ];
}


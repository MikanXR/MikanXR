// This file is auto generated. DO NOT EDIT.

import { MikanRequest, MikanResponse } from './MikanAPITypes.js';
import { MikanFunctionDescriptor } from './MikanFunctionTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class FunctionDescriptorResponse extends MikanResponse {
  descriptor_list: MikanFunctionDescriptor[] = [];

  constructor() {
    super();
    this.responseTypeName = 'FunctionDescriptorResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'descriptor_list', type: 'MikanFunctionDescriptor', isArray: true }
  ];
}

export class GetFunctionListRequest extends MikanRequest {
  systemFilter: string = '';
  componentFilter: string = '';

  constructor() {
    super();
    this.requestTypeName = 'GetFunctionListRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'systemFilter', type: 'string' },
    { name: 'componentFilter', type: 'string' }
  ];
}

export class InvokeComponentFunctionRequest extends MikanRequest {
  ownerSystem: string = '';
  componentId: number = -1;
  functionName: string = '';

  constructor() {
    super();
    this.requestTypeName = 'InvokeComponentFunctionRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'componentId', type: 'int32' },
    { name: 'functionName', type: 'string' }
  ];
}

export class InvokeSystemFunctionRequest extends MikanRequest {
  ownerSystem: string = '';
  functionName: string = '';

  constructor() {
    super();
    this.requestTypeName = 'InvokeSystemFunctionRequest';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'ownerSystem', type: 'string' },
    { name: 'functionName', type: 'string' }
  ];
}


// This file is auto generated. DO NOT EDIT.

import { MikanRequest, MikanResponse } from './../MikanAPITypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class ArucoMarkerImageResponse extends MikanResponse {
  imageData: string = '';

  constructor() {
    super();
    this.responseTypeName = 'ArucoMarkerImageResponse';
  }

  static __serializationMetadata: SerializationField[] = [
    { name: 'imageData', type: 'string' }
  ];
}

export class GetArucoMarkerImageRequest extends MikanRequest {
  markerId: number = 0;
  imageSize: number = 200;

  constructor() {
    super();
    this.requestTypeName = 'GetArucoMarkerImageRequest';
  }

  static readonly responseTypeName: string = 'ArucoMarkerImageResponse';

  static __serializationMetadata: SerializationField[] = [
    { name: 'markerId', type: 'int32' },
    { name: 'imageSize', type: 'int32' }
  ];
}


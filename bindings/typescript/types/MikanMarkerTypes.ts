// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import { MikanSystemValues } from './MikanPropertyTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanMarkerDictionaryType {
  INVALID = -1,
  DICT_4x4 = 0,
  DICT_5x5 = 1,
  DICT_6x6 = 2,
  DICT_7x7 = 3
}

export class MikanMarkerComponentValues extends MikanComponentValues {
  static readonly k_componentClassName = 'MarkerComponent';
  static readonly k_ownerSystemName = 'MarkerObjectSystem';
  aruco_id: number = -1;
  length_mm: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'aruco_id', type: 'int32' },
    { name: 'length_mm', type: 'float' }
  ];
}

export class MikanMarkerSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'MarkerObjectSystem';
  aruco_id_list: number[] = [];
  aruco_dictionary_type: MikanMarkerDictionaryType = MikanMarkerDictionaryType.INVALID;
  charuco_rows: number = 0;
  charuco_cols: number = 0;
  charuco_square_length_mm: number = 0;
  charuco_marker_length_mm: number = 0;
  charuco_dictionary_type: MikanMarkerDictionaryType = MikanMarkerDictionaryType.INVALID;

  static __serializationMetadata: SerializationField[] = [
    { name: 'aruco_id_list', type: 'int32', isArray: true },
    { name: 'aruco_dictionary_type', type: 'enum:MikanMarkerDictionaryType' },
    { name: 'charuco_rows', type: 'int32' },
    { name: 'charuco_cols', type: 'int32' },
    { name: 'charuco_square_length_mm', type: 'float' },
    { name: 'charuco_marker_length_mm', type: 'float' },
    { name: 'charuco_dictionary_type', type: 'enum:MikanMarkerDictionaryType' }
  ];
}


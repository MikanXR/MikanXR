// This file is auto generated. DO NOT EDIT.

import { MikanSystemValues } from './MikanPropertyTypes.js';
import { MikanTransformComponentValues } from './MikanTransformTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanShapeComponentValues extends MikanTransformComponentValues {
  shape_graph_path: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'shape_graph_path', type: 'string' }
  ];
}

export class MikanBoxShapeComponentValues extends MikanShapeComponentValues {
  static readonly k_componentClassName = 'BoxShapeComponent';
  static readonly k_ownerSystemName = 'BoxShapeSystem';
  box_x_size: number = 0;
  box_y_size: number = 0;
  box_z_size: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'box_x_size', type: 'float' },
    { name: 'box_y_size', type: 'float' },
    { name: 'box_z_size', type: 'float' }
  ];
}

export class MikanBoxShapeSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'BoxShapeSystem';

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanModelShapeComponentValues extends MikanShapeComponentValues {
  static readonly k_componentClassName = 'ModelShapeComponent';
  static readonly k_ownerSystemName = 'ModelShapeSystem';
  model_path: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'model_path', type: 'string' }
  ];
}

export class MikanModelShapeSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'ModelShapeSystem';

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanQuadShapeComponentValues extends MikanShapeComponentValues {
  static readonly k_componentClassName = 'QuadShapeComponent';
  static readonly k_ownerSystemName = 'QuadShapeSystem';
  quad_width: number = 0;
  quad_height: number = 0;
  is_double_sided: boolean = false;

  static __serializationMetadata: SerializationField[] = [
    { name: 'quad_width', type: 'float' },
    { name: 'quad_height', type: 'float' },
    { name: 'is_double_sided', type: 'boolean' }
  ];
}

export class MikanQuadShapeSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'QuadShapeSystem';

  static __serializationMetadata: SerializationField[] = [
  ];
}


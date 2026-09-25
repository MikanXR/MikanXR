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

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanModelShapeComponentValues extends MikanShapeComponentValues {
  model_path: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'model_path', type: 'string' }
  ];
}

export class MikanModelShapeSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanQuadShapeComponentValues extends MikanShapeComponentValues {
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

  static __serializationMetadata: SerializationField[] = [
  ];
}


// This file is auto generated. DO NOT EDIT.

import { MikanSystemValues } from './MikanPropertyTypes.js';
import { MikanTransformComponentValues } from './MikanTransformTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanStencilCullMode {
  NONE = 0,
  Z_Axis = 1,
  Y_Axis = 2,
  X_Axis = 3
}

export class MikanStencilComponentValues extends MikanTransformComponentValues {
  is_disabled: boolean = false;
  cull_mode: MikanStencilCullMode = MikanStencilCullMode.NONE;

  static __serializationMetadata: SerializationField[] = [
    { name: 'is_disabled', type: 'boolean' },
    { name: 'cull_mode', type: 'enum:MikanStencilCullMode' }
  ];
}

export class MikanBoxStencilComponentValues extends MikanStencilComponentValues {
  static readonly k_componentClassName = 'BoxStencilComponent';
  static readonly k_ownerSystemName = 'BoxStencilSystem';
  box_x_size: number = 0;
  box_y_size: number = 0;
  box_z_size: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'box_x_size', type: 'float' },
    { name: 'box_y_size', type: 'float' },
    { name: 'box_z_size', type: 'float' }
  ];
}

export class MikanBoxStencilSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'BoxStencilSystem';

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanModelStencilComponentValues extends MikanStencilComponentValues {
  static readonly k_componentClassName = 'ModelStencilComponent';
  static readonly k_ownerSystemName = 'ModelStencilSystem';
  model_path: string = '';
  geometry_url: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'model_path', type: 'string' },
    { name: 'geometry_url', type: 'string' }
  ];
}

export class MikanModelStencilSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'ModelStencilSystem';

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanQuadStencilComponentValues extends MikanStencilComponentValues {
  static readonly k_componentClassName = 'QuadStencilComponent';
  static readonly k_ownerSystemName = 'QuadStencilSystem';
  quad_width: number = 0;
  quad_height: number = 0;
  is_double_sided: boolean = false;

  static __serializationMetadata: SerializationField[] = [
    { name: 'quad_width', type: 'float' },
    { name: 'quad_height', type: 'float' },
    { name: 'is_double_sided', type: 'boolean' }
  ];
}

export class MikanQuadStencilSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'QuadStencilSystem';

  static __serializationMetadata: SerializationField[] = [
  ];
}


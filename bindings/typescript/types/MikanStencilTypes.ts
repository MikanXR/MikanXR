// This file is auto generated. DO NOT EDIT.

import { MikanVector2f, MikanVector3f } from './MikanMathTypes.js';
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

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanModelStencilComponentValues extends MikanStencilComponentValues {
  model_path: string = '';

  static __serializationMetadata: SerializationField[] = [
    { name: 'model_path', type: 'string' }
  ];
}

export class MikanModelStencilSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanQuadStencilComponentValues extends MikanStencilComponentValues {
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

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanStencilModelRenderGeometry {
  meshes: MikanTriagulatedMesh[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'meshes', type: 'MikanTriagulatedMesh', isArray: true }
  ];
}

export class MikanTriagulatedMesh {
  vertices: MikanVector3f[] = [];
  normals: MikanVector3f[] = [];
  texels: MikanVector2f[] = [];
  indices: number[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'vertices', type: 'MikanVector3f', isArray: true },
    { name: 'normals', type: 'MikanVector3f', isArray: true },
    { name: 'texels', type: 'MikanVector2f', isArray: true },
    { name: 'indices', type: 'int32', isArray: true }
  ];
}


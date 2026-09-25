// This file is auto generated. DO NOT EDIT.

import { MikanSystemValues } from './MikanPropertyTypes.js';
import { MikanTransformComponentValues } from './MikanTransformTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanSceneComponentValues extends MikanTransformComponentValues {
  display_compositor_id: number = -1;
  force_render: boolean = false;

  static __serializationMetadata: SerializationField[] = [
    { name: 'display_compositor_id', type: 'int32' },
    { name: 'force_render', type: 'boolean' }
  ];
}

export class MikanSceneSystemValues extends MikanSystemValues {
  current_scene_id: number = -1;

  static __serializationMetadata: SerializationField[] = [
    { name: 'current_scene_id', type: 'int32' }
  ];
}


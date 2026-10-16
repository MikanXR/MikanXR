// This file is auto generated. DO NOT EDIT.

import { MikanSystemValues } from './MikanPropertyTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export class MikanEditorSystemValues extends MikanSystemValues {
  static readonly k_systemName = 'EditorObjectSystem';
  render_origin: boolean = false;
  render_anchors: boolean = false;
  render_quad_stencils: boolean = false;
  render_box_stencils: boolean = false;
  render_model_stencils: boolean = false;
  camera_speed: number = 0;
  selected_language: string = '';
  available_language_list: string[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'render_origin', type: 'boolean' },
    { name: 'render_anchors', type: 'boolean' },
    { name: 'render_quad_stencils', type: 'boolean' },
    { name: 'render_box_stencils', type: 'boolean' },
    { name: 'render_model_stencils', type: 'boolean' },
    { name: 'camera_speed', type: 'float' },
    { name: 'selected_language', type: 'string' },
    { name: 'available_language_list', type: 'string', isArray: true }
  ];
}


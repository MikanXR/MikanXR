// This file is auto generated. DO NOT EDIT.

import { MikanComponentValues } from './MikanComponentTypes.js';
import { MikanVector2f, MikanVector3f } from './MikanMathTypes.js';
import { MikanSystemValues } from './MikanPropertyTypes.js';
import { MikanTransformComponentValues } from './MikanTransformTypes.js';
import type { SerializationField } from './SerializationTypes.js';

export enum MikanDMXBufferFormat {
  DMXUncompressed = 0,
  DMXRLEEncoded = 1
}

export enum MikanDMXScrollDirection {
  Left = 0,
  Right = 1,
  Up = 2,
  Down = 3
}

export enum MikanDMXSequenceContentSource {
  Script = 0,
  ScrollBitmap = 1,
  ScrollText = 2,
  PlayAnimation = 3
}

export enum MikanPixelGridOrigin {
  UpperLeft = 0,
  UpperRight = 1,
  LowerLeft = 2,
  LowerRight = 3
}

export class MikanDMXData {
  server_time_seconds: number = 0;
  universes: MikanUniverseDMXData[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'server_time_seconds', type: 'double' },
    { name: 'universes', type: 'MikanUniverseDMXData', isArray: true }
  ];
}

export class MikanDMXFixtureComponentValues extends MikanTransformComponentValues {
  stage_id: number = -1;
  dmx_universe: number = 1;
  dmx_start_channel: number = 1;
  dmx_channel_count: number = 3;
  is_disabled: boolean = false;
  max_wattage: number = 0.300000012;
  lumens_per_watt: number = 83;

  static __serializationMetadata: SerializationField[] = [
    { name: 'stage_id', type: 'int32' },
    { name: 'dmx_universe', type: 'uint16' },
    { name: 'dmx_start_channel', type: 'uint16' },
    { name: 'dmx_channel_count', type: 'uint16' },
    { name: 'is_disabled', type: 'boolean' },
    { name: 'max_wattage', type: 'float' },
    { name: 'lumens_per_watt', type: 'float' }
  ];
}

export class MikanDMXFixtureGroupComponentValues extends MikanComponentValues {
  stage_id: number = -1;
  fixture_ids: number[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'stage_id', type: 'int32' },
    { name: 'fixture_ids', type: 'int32', isArray: true }
  ];
}

export class MikanDMXFixtureGroupSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanDMXObjectSystemValues extends MikanSystemValues {
  network_interface_ip: string = '';
  dmx_priority: number = 100;
  transmit_rate_hz: number = 44;

  static __serializationMetadata: SerializationField[] = [
    { name: 'network_interface_ip', type: 'string' },
    { name: 'dmx_priority', type: 'uint8' },
    { name: 'transmit_rate_hz', type: 'float' }
  ];
}

export class MikanDMXPresetComponentValues extends MikanComponentValues {
  group_id: number = -1;
  fixture_ids: number[] = [];
  channel_counts: number[] = [];
  channel_data: number[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'group_id', type: 'int32' },
    { name: 'fixture_ids', type: 'int32', isArray: true },
    { name: 'channel_counts', type: 'int32', isArray: true },
    { name: 'channel_data', type: 'uint8', isArray: true }
  ];
}

export class MikanDMXPresetSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanDMXSequenceComponentValues extends MikanComponentValues {
  group_id: number = -1;
  script_component_id: number = -1;
  duration_seconds: number = 10;
  loop: boolean = true;
  playback_state: number = 0;
  time_since_start: number = 0;
  content_source: MikanDMXSequenceContentSource = MikanDMXSequenceContentSource.Script;
  content_path: string = '';
  scroll_text: string = '';
  font_path: string = '';
  text_pixel_height: number = 0;
  foreground_color: MikanVector3f = new MikanVector3f();
  background_color: MikanVector3f = new MikanVector3f();
  scroll_direction: MikanDMXScrollDirection = MikanDMXScrollDirection.Left;
  scroll_speed: number = 8;
  sprite_frame_width: number = 0;
  sprite_frame_height: number = 0;
  sprite_fps: number = 10;
  playback_speed_scale: number = 1;
  brightness: number = 1;

  static __serializationMetadata: SerializationField[] = [
    { name: 'group_id', type: 'int32' },
    { name: 'script_component_id', type: 'int32' },
    { name: 'duration_seconds', type: 'float' },
    { name: 'loop', type: 'boolean' },
    { name: 'playback_state', type: 'int32' },
    { name: 'time_since_start', type: 'float' },
    { name: 'content_source', type: 'enum:MikanDMXSequenceContentSource' },
    { name: 'content_path', type: 'string' },
    { name: 'scroll_text', type: 'string' },
    { name: 'font_path', type: 'string' },
    { name: 'text_pixel_height', type: 'int32' },
    { name: 'foreground_color', type: 'MikanVector3f' },
    { name: 'background_color', type: 'MikanVector3f' },
    { name: 'scroll_direction', type: 'enum:MikanDMXScrollDirection' },
    { name: 'scroll_speed', type: 'float' },
    { name: 'sprite_frame_width', type: 'int32' },
    { name: 'sprite_frame_height', type: 'int32' },
    { name: 'sprite_fps', type: 'float' },
    { name: 'playback_speed_scale', type: 'float' },
    { name: 'brightness', type: 'float' }
  ];
}

export class MikanDMXSequenceSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanLightEnvironmentComponentValues extends MikanTransformComponentValues {
  sh_coefficients: number[] = [];
  exposure_scale: number = 1;
  directionality: number = 0;
  key_light_direction: MikanVector3f = new MikanVector3f();

  static __serializationMetadata: SerializationField[] = [
    { name: 'sh_coefficients', type: 'float', isArray: true },
    { name: 'exposure_scale', type: 'float' },
    { name: 'directionality', type: 'float' },
    { name: 'key_light_direction', type: 'MikanVector3f' }
  ];
}

export class MikanLightEnvironmentSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanRGBPixelGridComponentValues extends MikanDMXFixtureComponentValues {
  grid_columns: number = 8;
  grid_rows: number = 8;
  pixel_size_mm: MikanVector3f = new MikanVector3f();
  pixel_separation_mm: MikanVector2f = new MikanVector2f();
  origin_pixel: MikanPixelGridOrigin = MikanPixelGridOrigin.UpperLeft;
  zig_zag: boolean = false;

  static __serializationMetadata: SerializationField[] = [
    { name: 'grid_columns', type: 'int32' },
    { name: 'grid_rows', type: 'int32' },
    { name: 'pixel_size_mm', type: 'MikanVector3f' },
    { name: 'pixel_separation_mm', type: 'MikanVector2f' },
    { name: 'origin_pixel', type: 'enum:MikanPixelGridOrigin' },
    { name: 'zig_zag', type: 'boolean' }
  ];
}

export class MikanRGBPixelGridSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanRGBSpotLightComponentValues extends MikanDMXFixtureComponentValues {
  cone_angle_degrees: number = 0;
  cone_range_meters: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'cone_angle_degrees', type: 'float' },
    { name: 'cone_range_meters', type: 'float' }
  ];
}

export class MikanRGBSpotLightSystemValues extends MikanSystemValues {

  static __serializationMetadata: SerializationField[] = [
  ];
}

export class MikanUniverseDMXData {
  dmx_universe_id: number = 0;
  buffer_format: MikanDMXBufferFormat = MikanDMXBufferFormat.DMXUncompressed;
  buffer_data: number[] = [];

  static __serializationMetadata: SerializationField[] = [
    { name: 'dmx_universe_id', type: 'uint16' },
    { name: 'buffer_format', type: 'enum:MikanDMXBufferFormat' },
    { name: 'buffer_data', type: 'uint8', isArray: true }
  ];
}


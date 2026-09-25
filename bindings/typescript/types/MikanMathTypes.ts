// This file is auto generated. DO NOT EDIT.

import type { SerializationField } from './SerializationTypes.js';

export class MikanMatrix3d {
  x0: number = 0;
  x1: number = 0;
  x2: number = 0;
  y0: number = 0;
  y1: number = 0;
  y2: number = 0;
  z0: number = 0;
  z1: number = 0;
  z2: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x0', type: 'double' },
    { name: 'x1', type: 'double' },
    { name: 'x2', type: 'double' },
    { name: 'y0', type: 'double' },
    { name: 'y1', type: 'double' },
    { name: 'y2', type: 'double' },
    { name: 'z0', type: 'double' },
    { name: 'z1', type: 'double' },
    { name: 'z2', type: 'double' }
  ];
}

export class MikanMatrix4d {
  x0: number = 0;
  x1: number = 0;
  x2: number = 0;
  x3: number = 0;
  y0: number = 0;
  y1: number = 0;
  y2: number = 0;
  y3: number = 0;
  z0: number = 0;
  z1: number = 0;
  z2: number = 0;
  z3: number = 0;
  w0: number = 0;
  w1: number = 0;
  w2: number = 0;
  w3: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x0', type: 'double' },
    { name: 'x1', type: 'double' },
    { name: 'x2', type: 'double' },
    { name: 'x3', type: 'double' },
    { name: 'y0', type: 'double' },
    { name: 'y1', type: 'double' },
    { name: 'y2', type: 'double' },
    { name: 'y3', type: 'double' },
    { name: 'z0', type: 'double' },
    { name: 'z1', type: 'double' },
    { name: 'z2', type: 'double' },
    { name: 'z3', type: 'double' },
    { name: 'w0', type: 'double' },
    { name: 'w1', type: 'double' },
    { name: 'w2', type: 'double' },
    { name: 'w3', type: 'double' }
  ];
}

export class MikanMatrix4f {
  x0: number = 0;
  x1: number = 0;
  x2: number = 0;
  x3: number = 0;
  y0: number = 0;
  y1: number = 0;
  y2: number = 0;
  y3: number = 0;
  z0: number = 0;
  z1: number = 0;
  z2: number = 0;
  z3: number = 0;
  w0: number = 0;
  w1: number = 0;
  w2: number = 0;
  w3: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x0', type: 'float' },
    { name: 'x1', type: 'float' },
    { name: 'x2', type: 'float' },
    { name: 'x3', type: 'float' },
    { name: 'y0', type: 'float' },
    { name: 'y1', type: 'float' },
    { name: 'y2', type: 'float' },
    { name: 'y3', type: 'float' },
    { name: 'z0', type: 'float' },
    { name: 'z1', type: 'float' },
    { name: 'z2', type: 'float' },
    { name: 'z3', type: 'float' },
    { name: 'w0', type: 'float' },
    { name: 'w1', type: 'float' },
    { name: 'w2', type: 'float' },
    { name: 'w3', type: 'float' }
  ];
}

export class MikanMatrix4x3d {
  x0: number = 0;
  x1: number = 0;
  x2: number = 0;
  x3: number = 0;
  y0: number = 0;
  y1: number = 0;
  y2: number = 0;
  y3: number = 0;
  z0: number = 0;
  z1: number = 0;
  z2: number = 0;
  z3: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x0', type: 'double' },
    { name: 'x1', type: 'double' },
    { name: 'x2', type: 'double' },
    { name: 'x3', type: 'double' },
    { name: 'y0', type: 'double' },
    { name: 'y1', type: 'double' },
    { name: 'y2', type: 'double' },
    { name: 'y3', type: 'double' },
    { name: 'z0', type: 'double' },
    { name: 'z1', type: 'double' },
    { name: 'z2', type: 'double' },
    { name: 'z3', type: 'double' }
  ];
}

export class MikanQuatd {
  w: number = 0;
  x: number = 0;
  y: number = 0;
  z: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'w', type: 'double' },
    { name: 'x', type: 'double' },
    { name: 'y', type: 'double' },
    { name: 'z', type: 'double' }
  ];
}

export class MikanQuatf {
  w: number = 0;
  x: number = 0;
  y: number = 0;
  z: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'w', type: 'float' },
    { name: 'x', type: 'float' },
    { name: 'y', type: 'float' },
    { name: 'z', type: 'float' }
  ];
}

export class MikanRotator3f {
  x_angle: number = 0;
  y_angle: number = 0;
  z_angle: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x_angle', type: 'float' },
    { name: 'y_angle', type: 'float' },
    { name: 'z_angle', type: 'float' }
  ];
}

export class MikanTransform {
  scale: MikanVector3f = new MikanVector3f();
  rotation: MikanQuatf = new MikanQuatf();
  position: MikanVector3f = new MikanVector3f();

  static __serializationMetadata: SerializationField[] = [
    { name: 'scale', type: 'MikanVector3f' },
    { name: 'rotation', type: 'MikanQuatf' },
    { name: 'position', type: 'MikanVector3f' }
  ];
}

export class MikanVector2d {
  x: number = 0;
  y: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x', type: 'double' },
    { name: 'y', type: 'double' }
  ];
}

export class MikanVector2f {
  x: number = 0;
  y: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x', type: 'float' },
    { name: 'y', type: 'float' }
  ];
}

export class MikanVector2i {
  x: number = 0;
  y: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x', type: 'int32' },
    { name: 'y', type: 'int32' }
  ];
}

export class MikanVector3d {
  x: number = 0;
  y: number = 0;
  z: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x', type: 'double' },
    { name: 'y', type: 'double' },
    { name: 'z', type: 'double' }
  ];
}

export class MikanVector3f {
  x: number = 0;
  y: number = 0;
  z: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x', type: 'float' },
    { name: 'y', type: 'float' },
    { name: 'z', type: 'float' }
  ];
}

export class MikanVector4d {
  x: number = 0;
  y: number = 0;
  z: number = 0;
  w: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x', type: 'double' },
    { name: 'y', type: 'double' },
    { name: 'z', type: 'double' },
    { name: 'w', type: 'double' }
  ];
}

export class MikanVector4f {
  x: number = 0;
  y: number = 0;
  z: number = 0;
  w: number = 0;

  static __serializationMetadata: SerializationField[] = [
    { name: 'x', type: 'float' },
    { name: 'y', type: 'float' },
    { name: 'z', type: 'float' },
    { name: 'w', type: 'float' }
  ];
}


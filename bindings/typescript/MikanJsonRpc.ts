import { stringifyJson } from './Serialization/JsonSerializer.js';

/**
 * The JSON-RPC 2.0 envelope every websocket text frame between the editor and a client travels
 * in. A request names a method, carries the request class's fields as its params and carries an
 * integer id. A response carries the id it answers and exactly one of result or error. An event
 * is a notification from the editor: a method and params with no id.
 */
export const JSON_RPC_VERSION = '2.0';

// What a frame from the editor is. The editor sends no requests, so a frame naming a method is
// an event.
export enum MikanJsonRpcFrameKind {
  Invalid,
  Event,
  Response
}

export function formatJsonRpcRequest(method: string, params: object, id: number): string {
  return stringifyJson({ jsonrpc: JSON_RPC_VERSION, method, params, id });
}

// The frame as an object, or null when it is not a JSON object. A batch (an array) is not
// supported, so it is not an object either.
export function parseJsonRpcFrame(utf8Frame: string): any | null {
  try {
    const frame = JSON.parse(utf8Frame);
    return frame !== null && typeof frame === 'object' && !Array.isArray(frame) ? frame : null;
  } catch {
    return null;
  }
}

export function classifyJsonRpcFrame(frame: any | null): MikanJsonRpcFrameKind {
  if (!frame || frame.jsonrpc !== JSON_RPC_VERSION) {
    return MikanJsonRpcFrameKind.Invalid;
  }

  const hasId = 'id' in frame;
  if ('method' in frame) {
    return typeof frame.method === 'string' && !hasId
      ? MikanJsonRpcFrameKind.Event
      : MikanJsonRpcFrameKind.Invalid;
  }

  const hasResult = 'result' in frame;
  const hasError = 'error' in frame;

  return hasId && hasResult !== hasError ? MikanJsonRpcFrameKind.Response : MikanJsonRpcFrameKind.Invalid;
}

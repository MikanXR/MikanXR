/**
 * Mikan API unit tests for TypeScript bindings
 * Mirrors the C++ tests in mikan_api_unit_tests.cpp
 */

import {
  serializeToJsonString,
  TypeRegistry
} from '../Serialization';

import {
  MikanRequest,
  MikanResponse,
  MikanEvent
} from '../MikanAPITypes';

import { MikanAPIResult } from '../types/MikanAPITypes';
import { MikanClientGraphicsApi, MikanDisconnectCode } from '../types/MikanCoreConstants';
import { MikanConnectedEvent, MikanDisconnectedEvent } from '../types/MikanClientEvents';
import { InitClientRequest } from '../types/MikanClientRequests';
import { PublishCameraRenderTargetTextures } from '../types/MikanCameraRequests';

import {
  GetAppStageInfo,
  MikanAppStageInfoResponse,
  MikanRemoteControlCommand,
  PopAppStage,
} from '../types/MikanRemoteControlRequest';

import * as generatedTypes from '../types/index';
import { registerAllTypes } from '../types/TypeRegistration';
import { registerAllEnums } from '../types/EnumRegistration';
import { MikanRequestManager } from '../MikanRequestManager';
import { MikanEventManager } from '../MikanEventManager';
import { MikanJsonRpcFrameKind, classifyJsonRpcFrame, parseJsonRpcFrame } from '../MikanJsonRpc';

// Populate the TypeRegistry before any tests run.
// This is the TS equivalent of TypeRegistry::buildFromRfkDatabase().
beforeAll(() => {
  registerAllTypes();
  registerAllEnums();
});

describe('MikanAPIUnitTests', () => {

  // ---- TypeRegistry tests -------------------------------------------------
  // Mirrors: mikan_api_test_type_registry_build,
  //          mikan_api_test_type_registry_lookup,
  //          mikan_api_test_type_registry_unknown_returns_null

  describe('TypeRegistry', () => {
    it('should contain core MikanRequest / MikanResponse / MikanEvent after registration', () => {
      expect(TypeRegistry.has('MikanRequest')).toBe(true);
      expect(TypeRegistry.has('MikanResponse')).toBe(true);
      expect(TypeRegistry.has('MikanEvent')).toBe(true);
    });

    it('should contain known concrete request type MikanRemoteControlCommand', () => {
      expect(TypeRegistry.has('MikanRemoteControlCommand')).toBe(true);
    });

    it('should contain known response type MikanRemoteControlCommandResult', () => {
      expect(TypeRegistry.has('MikanRemoteControlCommandResult')).toBe(true);
    });

    it('should contain known event type MikanDisconnectedEvent', () => {
      expect(TypeRegistry.has('MikanDisconnectedEvent')).toBe(true);
    });

    it('retrieved constructor for MikanRemoteControlCommand should produce a correctly named instance', () => {
      const Ctor = TypeRegistry.get('MikanRemoteControlCommand');
      expect(Ctor).toBeDefined();

      const instance = new Ctor!();
      expect(instance.constructor.name).toBe('MikanRemoteControlCommand');
    });

    it('should map MikanResponse to the correct constructor', () => {
      const Ctor = TypeRegistry.get('MikanResponse');
      expect(Ctor).toBe(MikanResponse);
    });

    it('should return undefined for unknown type names', () => {
      expect(TypeRegistry.get('ThisTypeDoesNotExist')).toBeUndefined();
      expect(TypeRegistry.has('ThisTypeDoesNotExist')).toBe(false);
    });
  });

  // ---- Constructor name tests ---------------------------------------------
  // Mirrors: mikan_api_test_type_name_set_on_construct

  describe('type name on constructed objects', () => {
    it('MikanRequest.name should be "MikanRequest"', () => {
      expect(MikanRequest.name).toBe('MikanRequest');
    });

    it('MikanResponse.name should be "MikanResponse"', () => {
      expect(MikanResponse.name).toBe('MikanResponse');
    });

    it('MikanEvent.name should be "MikanEvent"', () => {
      expect(MikanEvent.name).toBe('MikanEvent');
    });

    it('MikanRemoteControlCommand.name should be the derived name, not the base', () => {
      expect(MikanRemoteControlCommand.name).toBe('MikanRemoteControlCommand');
    });

    it('the constructor should stamp the concrete type name the request is sent as', () => {
      expect(new MikanRequest().requestTypeName).toBe('MikanRequest');
      expect(new MikanResponse().responseTypeName).toBe('MikanResponse');
      expect(new MikanEvent().eventTypeName).toBe('MikanEvent');
      expect(new MikanRemoteControlCommand().requestTypeName).toBe('MikanRemoteControlCommand');
    });
  });

  // ---- Response type tests ------------------------------------------------

  describe('response type named by each request', () => {
    it('MikanRequest should default to the plain MikanResponse', () => {
      expect(MikanRequest.responseTypeName).toBe('MikanResponse');
    });

    it('every registered request should name a registered response', () => {
      const requestTypeNames = registeredRequestTypeNames();
      expect(requestTypeNames.length).toBeGreaterThan(0);

      for (const typeName of requestTypeNames) {
        const RequestCtor = TypeRegistry.get(typeName) as typeof MikanRequest;
        const ResponseCtor = TypeRegistry.get(RequestCtor.responseTypeName);
        expect(ResponseCtor).toBeDefined();
        expect(new ResponseCtor!()).toBeInstanceOf(MikanResponse);
      }
    });
  });

  // ---- Frame classification tests ----------------------------------------

  describe('frame classification', () => {
    it('should classify a notification as an event and an id with a result or error as a response', () => {
      expect(classifyJsonRpcFrame(parseJsonRpcFrame('{"jsonrpc":"2.0","method":"MikanConnectedEvent","params":{}}')))
        .toBe(MikanJsonRpcFrameKind.Event);
      expect(classifyJsonRpcFrame(parseJsonRpcFrame('{"jsonrpc":"2.0","id":3,"result":{}}')))
        .toBe(MikanJsonRpcFrameKind.Response);
      expect(classifyJsonRpcFrame(parseJsonRpcFrame(
        '{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}}')))
        .toBe(MikanJsonRpcFrameKind.Response);
    });

    it('should reject a request, a batch, a missing version and a frame with both result and error', () => {
      expect(classifyJsonRpcFrame(parseJsonRpcFrame('{"jsonrpc":"2.0","method":"X","id":1}')))
        .toBe(MikanJsonRpcFrameKind.Invalid);
      expect(classifyJsonRpcFrame(parseJsonRpcFrame('[{"jsonrpc":"2.0","id":1,"result":{}}]')))
        .toBe(MikanJsonRpcFrameKind.Invalid);
      expect(classifyJsonRpcFrame(parseJsonRpcFrame('{"id":1,"result":{}}')))
        .toBe(MikanJsonRpcFrameKind.Invalid);
      expect(classifyJsonRpcFrame(parseJsonRpcFrame('{"jsonrpc":"2.0","id":1,"result":{},"error":{}}')))
        .toBe(MikanJsonRpcFrameKind.Invalid);
      expect(classifyJsonRpcFrame(parseJsonRpcFrame('not json'))).toBe(MikanJsonRpcFrameKind.Invalid);
    });
  });

  // ---- Envelope tests -----------------------------------------------------
  // Mirrors: mikan_api_test_request_envelope, mikan_api_test_response_round_trip,
  //          mikan_api_test_error_response_result_code,
  //          mikan_api_test_event_notification_round_trip

  describe('request envelope', () => {
    it("should send a JSON-RPC request whose params are the request's own fields", () => {
      const { requestManager, sentFrames } = makeRequestManager();

      const cmd = new MikanRemoteControlCommand();
      cmd.command = 'test_command';
      cmd.parameters = ['param1', 'param2'];
      requestManager.sendRequest(cmd);

      const frame = JSON.parse(sentFrames[0]);
      expect(frame.jsonrpc).toBe('2.0');
      expect(frame.method).toBe('MikanRemoteControlCommand');
      expect(frame.id).toBe(cmd.requestId);
      expect(frame.params).toEqual({ command: 'test_command', parameters: ['param1', 'param2'] });
    });

    it('should send enums by name and 64-bit integers as bare integers', () => {
      const { requestManager, sentFrames } = makeRequestManager();

      const initRequest = new InitClientRequest();
      initRequest.clientInfo.graphicsAPI = MikanClientGraphicsApi.Direct3D11;
      requestManager.sendRequest(initRequest);
      expect(JSON.parse(sentFrames[0]).params.clientInfo.graphicsAPI).toBe('Direct3D11');

      const publishRequest = new PublishCameraRenderTargetTextures();
      publishRequest.frame_index = 9007199254740991n;
      requestManager.sendRequest(publishRequest);
      expect(sentFrames[1]).toContain('"frame_index":9007199254740991');
    });
  });

  describe('response envelope', () => {
    it("should read a result into the request's response type", async () => {
      const { requestManager } = makeRequestManager();

      const request = new GetAppStageInfo();
      const future = requestManager.sendRequest(request);

      const expected = new MikanAppStageInfoResponse();
      expected.app_stage_info.app_state_name = 'MainMenu';
      requestManager.handleResponse(JSON.stringify({
        jsonrpc: '2.0',
        id: request.requestId,
        result: JSON.parse(serializeToJsonString(expected))
      }));

      const response = await future.getResponse();
      expect(response).toBeInstanceOf(MikanAppStageInfoResponse);
      expect((response as MikanAppStageInfoResponse).app_stage_info.app_state_name).toBe('MainMenu');
      expect(response.requestId).toBe(request.requestId);
      expect(response.resultCode).toBe(MikanAPIResult.Success);
      expect(response.responseTypeName).toBe('MikanAppStageInfoResponse');
    });

    it('should read an empty result into the plain MikanResponse', async () => {
      const { requestManager } = makeRequestManager();

      const request = new PopAppStage();
      const future = requestManager.sendRequest(request);
      requestManager.handleResponse(`{"jsonrpc":"2.0","id":${request.requestId},"result":{}}`);

      const response = await future.getResponse();
      expect(response.constructor).toBe(MikanResponse);
      expect(response.resultCode).toBe(MikanAPIResult.Success);
      expect(response.requestId).toBe(request.requestId);
    });

    it("should read an error into the plain MikanResponse with the error data's result code", async () => {
      const { requestManager } = makeRequestManager();

      const request = new GetAppStageInfo();
      const future = requestManager.sendRequest(request);
      requestManager.handleResponse(
        `{"jsonrpc":"2.0","id":${request.requestId},` +
        '"error":{"code":-32601,"message":"UnknownFunction","data":{"resultCode":"UnknownFunction"}}}');

      const response = await future.getResponse();
      expect(response.constructor).toBe(MikanResponse);
      expect(response.resultCode).toBe(MikanAPIResult.UnknownFunction);
      expect(response.requestId).toBe(request.requestId);
    });
  });

  describe('event envelope', () => {
    it('should read a notification into the event type its method names', () => {
      const eventManager = new MikanEventManager();
      const received: MikanEvent[] = [];
      eventManager.onConnected((event) => received.push(event));

      const expected = new MikanConnectedEvent();
      expected.isClientCompatible = true;
      expected.httpPort = 8090;
      eventManager.handleEventMessage(JSON.stringify({
        jsonrpc: '2.0',
        method: 'MikanConnectedEvent',
        params: JSON.parse(serializeToJsonString(expected))
      }));

      expect(received.length).toBe(1);
      expect(received[0]).toBeInstanceOf(MikanConnectedEvent);
      expect(received[0].eventTypeName).toBe('MikanConnectedEvent');
      expect((received[0] as MikanConnectedEvent).httpPort).toBe(8090);
      expect((received[0] as MikanConnectedEvent).isClientCompatible).toBe(true);
    });

    it('should turn the disconnect string into MikanDisconnectedEvent', () => {
      const eventManager = new MikanEventManager();
      const received: MikanDisconnectedEvent[] = [];
      eventManager.onDisconnected((event) => received.push(event));

      eventManager.handleEventMessage('disconnect:1000:bye');

      expect(received.length).toBe(1);
      expect(received[0].eventTypeName).toBe('MikanDisconnectedEvent');
      expect(received[0].code).toBe(MikanDisconnectCode.Normal);
      expect(received[0].reason).toBe('bye');
    });
  });
});

function makeRequestManager(): { requestManager: MikanRequestManager; sentFrames: string[] } {
  const requestManager = new MikanRequestManager();
  const sentFrames: string[] = [];
  requestManager.setSendFunction((message: string) => sentFrames.push(message));

  return { requestManager, sentFrames };
}

function registeredRequestTypeNames(): string[] {
  const requestTypeNames: string[] = [];
  for (const exported of Object.values(generatedTypes)) {
    if (typeof exported === 'function' && exported.prototype instanceof MikanRequest) {
      requestTypeNames.push(exported.name);
    }
  }

  return requestTypeNames;
}

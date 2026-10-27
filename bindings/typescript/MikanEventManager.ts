import { EventEmitter } from 'events';
import { MikanEvent } from './MikanAPITypes.js';
import {
  MikanDisconnectedEvent,
  MikanDisconnectCode,
  MikanPropertyUpdateEvent,
} from './types/index.js';
import {
  MikanAppStageChangedEvent,
} from './types/MikanRemoteControlEvents.js';
import { deserializeFromJson, TypeRegistry } from './Serialization/index.js';
import { MikanJsonRpcFrameKind, classifyJsonRpcFrame, parseJsonRpcFrame } from './MikanJsonRpc.js';

export class MikanEventManager extends EventEmitter {
  private static readonly WEBSOCKET_DISCONNECT_EVENT = 'disconnect';

  constructor() {
    super();
  }

  public handleEventMessage(message: string): void {
    const event = this.parseEvent(message);

    if (event) {
      // Emit the specific event type
      this.emit(event.eventTypeName, event);

      // Also emit a generic 'event' for all events
      this.emit('event', event);
    }
  }

  private parseEvent(message: string): MikanEvent | null {
    try {
      // Handle special disconnect event format
      if (message.startsWith(MikanEventManager.WEBSOCKET_DISCONNECT_EVENT)) {
        return this.parseDisconnectEvent(message);
      }

      // An event is a notification from the editor whose method is the event class's name
      const frame = parseJsonRpcFrame(message);
      if (classifyJsonRpcFrame(frame) !== MikanJsonRpcFrameKind.Event) {
        console.error('Not an event notification:', message);
        return null;
      }

      const eventTypeName: string = frame.method;
      // A notification may leave out params, which reads as an event with no fields
      const params = frame.params !== null && typeof frame.params === 'object' ? frame.params : {};

      // Use TypeRegistry for proper deserialization when the event type is known
      const eventType = TypeRegistry.get(eventTypeName);
      if (eventType) {
        const event = new eventType() as MikanEvent;
        if (!deserializeFromJson(params, event, eventType)) {
          return null;
        }
        event.eventTypeName = eventTypeName;
        return event;
      }

      // Fallback: shallow-copy the params for event types newer than these bindings
      const event = new MikanEvent();
      Object.assign(event, params);
      event.eventTypeName = eventTypeName;

      return event;
    } catch (error) {
      console.error('Failed to parse event:', error);
      return null;
    }
  }

  private parseDisconnectEvent(message: string): MikanDisconnectedEvent {
    let disconnectCode = MikanDisconnectCode.Normal;
    let disconnectReason = '';

    const tokens = message.split(':');
    if (tokens.length >= 3) {
      disconnectCode = parseInt(tokens[1]) as MikanDisconnectCode;
      disconnectReason = tokens[2];
    }

    const event = new MikanDisconnectedEvent();
    event.code = disconnectCode;
    event.reason = disconnectReason;

    return event;
  }

  // Typed event listeners
  public onConnected(listener: (event: MikanEvent) => void): this {
    return this.on('MikanConnectedEvent', listener);
  }

  public onDisconnected(listener: (event: MikanDisconnectedEvent) => void): this {
    return this.on('MikanDisconnectedEvent', listener);
  }

  public onAnchorListUpdate(listener: (event: MikanEvent) => void): this {
    return this.on('MikanAnchorListUpdateEvent', listener);
  }

  public onAnchorNameUpdate(listener: (event: any) => void): this {
    return this.on('MikanAnchorNameUpdateEvent', listener);
  }

  public onAnchorPoseUpdate(listener: (event: any) => void): this {
    return this.on('MikanAnchorPoseUpdateEvent', listener);
  }

  public onPropertyUpdate(listener: (event: MikanPropertyUpdateEvent) => void): this {
    return this.on('MikanPropertyUpdateEvent', listener);
  }

  public offPropertyUpdate(listener: (event: MikanPropertyUpdateEvent) => void): this {
    return this.off('MikanPropertyUpdateEvent', listener);
  }

  public onAppStageChanged(listener: (event: MikanAppStageChangedEvent) => void): this {
    return this.on('MikanAppStageChangedEvent', listener);
  }

  public onAnyEvent(listener: (event: MikanEvent) => void): this {
    return this.on('event', listener);
  }
}

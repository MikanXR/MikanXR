import { MikanRequestManager } from './MikanRequestManager.js';
import { MikanEventManager } from './MikanEventManager.js';
import { MikanResponseFuture } from './MikanResponseFuture.js';
import { MikanRequest, MikanEvent } from './MikanAPITypes.js';
import { MikanJsonRpcFrameKind, classifyJsonRpcFrame, parseJsonRpcFrame } from './MikanJsonRpc.js';
import {
  MikanAPIResult,
  MikanLogLevel,
  MikanClientInfo,
  MikanClientGraphicsApi,
  MikanConstants,
  MikanConnectedEvent,
  MikanDisconnectedEvent,
  MikanPropertyUpdateEvent
} from './types/index.js';

export type MikanLogCallback = (level: MikanLogLevel, message: string) => void;

/**
 * Builds the HTTP base URL for a server from a connect host and the httpPort
 * reported by MikanConnectedEvent. Returns an empty string when httpPort is zero.
 */
export function composeHttpBaseUrl(host: string, httpPort: number): string {
  if (!httpPort) {
    return '';
  }

  let normalizedHost = host.trim().replace(/^(wss?|https?):\/\//i, '');
  normalizedHost = normalizedHost.replace(/\/+$/, '');
  normalizedHost = normalizedHost.replace(/:\d+$/, '');
  if (normalizedHost === '') {
    normalizedHost = '127.0.0.1';
  }

  return `http://${normalizedHost}:${httpPort}`;
}

export interface MikanClientOptions {
  host?: string;
  port?: string;
  autoReconnect?: boolean;
  reconnectInterval?: number;
}

export class MikanClient {
  private ws: WebSocket | null = null;
  private requestManager: MikanRequestManager;
  private eventManager: MikanEventManager;
  private logCallback: MikanLogCallback | null = null;
  private isInitialized: boolean = false;
  private isConnected: boolean = false;
  private clientId: string = '';
  private options: MikanClientOptions;
  private httpPort: number = 0;

  constructor(options: MikanClientOptions = {}) {
    this.options = {
      host: options.host || 'localhost',
      port: options.port || '8080',
      autoReconnect: options.autoReconnect ?? false,
      reconnectInterval: options.reconnectInterval ?? 5000
    };

    this.requestManager = new MikanRequestManager();
    this.eventManager = new MikanEventManager();
    this.eventManager.onConnected((event: MikanEvent) => {
      this.httpPort = (event as MikanConnectedEvent).httpPort ?? 0;
    });
  }

  /**
   * Base URL of the server's HTTP endpoint, empty until a MikanConnectedEvent
   * with a nonzero httpPort arrives. Join with a component's geometry_url.
   */
  public get httpBaseUrl(): string {
    return composeHttpBaseUrl(this.options.host ?? '', this.httpPort);
  }

  // Initialization
  public initialize(minLogLevel: MikanLogLevel = MikanLogLevel.Info): MikanAPIResult {
    if (this.isInitialized) {
      return MikanAPIResult.Success;
    }

    this.clientId = this.generateClientId();
    this.isInitialized = true;

    this.log(MikanLogLevel.Info, `MikanClient initialized with ID: ${this.clientId}`);

    return MikanAPIResult.Success;
  }

  public getIsInitialized(): boolean {
    return this.isInitialized;
  }

  public shutdown(): void {
    if (this.isConnected) {
      this.disconnect();
    }

    this.isInitialized = false;
    this.clientId = '';
  }

  // Client Info
  public getClientAPIVersion(): number {
    return MikanConstants.ClientAPIVersion;
  }

  public getClientUniqueID(): string {
    return this.clientId;
  }

  public createClientInfo(
    engineName: string = 'Electron',
    engineVersion: string = '1.0.0',
    applicationName: string = 'MikanXR',
    applicationVersion: string = '1.0.0',
    xrDeviceName: string = 'Unknown'
  ): MikanClientInfo {
    return {
      clientId: this.clientId,
      engineName,
      engineVersion,
      applicationName,
      applicationVersion,
      xrDeviceName,
      graphicsAPI: MikanClientGraphicsApi.UNKNOWN,
      supportsRGB24: false,
      supportsRGBA32: false,
      supportsBGRA32: false,
      supportsDepth: false
    };
  }

  // Connection
  public async connect(): Promise<MikanAPIResult> {
    if (!this.isInitialized) {
      return MikanAPIResult.Uninitialized;
    }

    if (this.isConnected) {
      return MikanAPIResult.AlreadyConnected;
    }

    return new Promise((resolve) => {
      try {
        const url = `ws://${this.options.host}:${this.options.port}`;
        const protocol = `Mikan-${MikanConstants.ClientAPIVersion}`;
        this.log(MikanLogLevel.Info, `Connecting to ${url} with protocol ${protocol}`);

        this.ws = new WebSocket(url, protocol);

        this.ws.onopen = () => {
          this.isConnected = true;
          this.requestManager.setSendFunction((message: string) => {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
              this.ws.send(message);
            }
          });

          this.log(MikanLogLevel.Info, 'Connected to Mikan server');
          resolve(MikanAPIResult.Success);
        };

        this.ws.onmessage = (event) => {
          this.handleMessage(event.data);
        };

        this.ws.onerror = (error) => {
          this.log(MikanLogLevel.Error, `WebSocket error: ${error}`);
          if (!this.isConnected) {
            resolve(MikanAPIResult.SocketError);
          }
        };

        this.ws.onclose = (event) => {
          this.log(MikanLogLevel.Info, `Disconnected from Mikan server: ${event.code} ${event.reason}`);
          this.isConnected = false;
          this.httpPort = 0;
          this.requestManager.setSendFunction(null);
          this.requestManager.cancelAllRequests();

          // Emit disconnect event
          const disconnectMessage = `disconnect:${event.code}:${event.reason}`;
          this.eventManager.handleEventMessage(disconnectMessage);

          if (!this.isConnected) {
            resolve(MikanAPIResult.SocketError);
          }
        };
      } catch (error) {
        this.log(MikanLogLevel.Error, `Failed to connect: ${error}`);
        resolve(MikanAPIResult.SocketError);
      }
    });
  }

  public getIsConnected(): boolean {
    return this.isConnected && this.ws !== null && this.ws.readyState === WebSocket.OPEN;
  }

  public disconnect(): MikanAPIResult {
    if (!this.isConnected || !this.ws) {
      return MikanAPIResult.NotConnected;
    }

    this.ws.close(1000, 'Client disconnect');
    this.ws = null;
    this.isConnected = false;
    this.httpPort = 0;

    return MikanAPIResult.Success;
  }

  // Messaging
  public sendRequest(request: MikanRequest): MikanResponseFuture {
    return this.requestManager.sendRequest(request);
  }

  private handleMessage(data: string | ArrayBuffer | Blob): void {
    if (typeof data !== 'string') {
      this.log(MikanLogLevel.Warning, 'Dropping unexpected binary websocket frame');
      return;
    }

    // A notification (a method with no id) is an event, and a frame with an id and a result or
    // an error is a response. The disconnect event is raised by onclose, never received.
    switch (classifyJsonRpcFrame(parseJsonRpcFrame(data))) {
      case MikanJsonRpcFrameKind.Event:
        this.eventManager.handleEventMessage(data);
        break;
      case MikanJsonRpcFrameKind.Response:
        this.requestManager.handleResponse(data);
        break;
      default:
        this.log(MikanLogLevel.Warning, `Unknown message type: ${data}`);
        break;
    }
  }

  // Event Manager Access
  public get events(): MikanEventManager {
    return this.eventManager;
  }

  // Event listener convenience methods (delegate to event manager)
  public onConnected(listener: (event: MikanEvent) => void): this {
    this.eventManager.onConnected(listener);
    return this;
  }

  public onDisconnected(listener: (event: MikanDisconnectedEvent) => void): this {
    this.eventManager.onDisconnected(listener);
    return this;
  }

  public onAnchorListUpdate(listener: (event: MikanEvent) => void): this {
    this.eventManager.onAnchorListUpdate(listener);
    return this;
  }

  public onAnchorNameUpdate(listener: (event: any) => void): this {
    this.eventManager.onAnchorNameUpdate(listener);
    return this;
  }

  public onAnchorPoseUpdate(listener: (event: any) => void): this {
    this.eventManager.onAnchorPoseUpdate(listener);
    return this;
  }

  public onPropertyUpdate(listener: (event: MikanPropertyUpdateEvent) => void): this {
    this.eventManager.onPropertyUpdate(listener);
    return this;
  }

  public offPropertyUpdate(listener: (event: MikanPropertyUpdateEvent) => void): this {
    this.eventManager.offPropertyUpdate(listener);
    return this;
  }

  public onAppStageChanged(listener: (event: import('./types/MikanRemoteControlEvents.js').MikanAppStageChangedEvent) => void): this {
    this.eventManager.onAppStageChanged(listener);
    return this;
  }

  public onAnyEvent(listener: (event: MikanEvent) => void): this {
    this.eventManager.onAnyEvent(listener);
    return this;
  }

  // Logging
  public setLogCallback(callback: MikanLogCallback): void {
    this.logCallback = callback;
  }

  private log(level: MikanLogLevel, message: string): void {
    if (this.logCallback) {
      this.logCallback(level, message);
    } else {
      console.log(`[Mikan ${MikanLogLevel[level]}] ${message}`);
    }
  }

  // Utility
  private generateClientId(): string {
    return `electron-client-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  }
}

using System;
using System.Collections.Generic;
using MikanXR;

namespace Mikan
{
	public class TestMikanClient : IDisposable
	{
        private TestGraphicsContext _graphicsContext;
		private MikanAPI _mikanAPI;
		private bool _mikanInitialized = false;
		private int m_lastProcessedCamera= -1;
		private bool _shutdownRequested = false;
		private float _mikanReconnectTimeout = 0f;

		public MikanAPI MikanAPI => _mikanAPI;
        public bool IsShutdownRequested => _shutdownRequested;

		public TestMikanClient(TestGraphicsContext graphicsContext, bool useManagedTransport)
		{
			_graphicsContext = graphicsContext;
			_mikanAPI = useManagedTransport
				? new MikanAPI(new MikanWebSocketTransport(), null)
				: new MikanAPI();
		}

		public bool Initialize(string clientName)
		{
			_mikanAPI.SetLogCallback(OnMikanLog);

			if (_mikanAPI.Initialize(clientName, MikanLogLevel.Info) != MikanAPIResult.Success)
			{
				Console.WriteLine("ERROR: Failed to initialize Mikan API");
				return false;
			}

			// Only a writer that owns the shared texture library wants the device
			if (_mikanAPI.RenderTargetWriter != null &&
				_mikanAPI.SetGraphicsDeviceInterface(
					_graphicsContext.GetGraphicsApi(),
					_graphicsContext.GetGraphicsDeviceInterface()) != MikanAPIResult.Success)
			{
				Console.WriteLine("ERROR: Failed to set graphics device interface");
				return false;
			}

			_mikanInitialized = true;

            // Create a default camera render target to render to when we aren't connected to Mikan
            _graphicsContext.GetOrAddCameraRenderTarget(-1);

            return true;
		}

		public void Dispose()
		{
			// Clean up all render targets
			_graphicsContext.RemoveAllCameraRenderTargets(_mikanAPI);

			// Shut down the mikan client connection
			if (_mikanInitialized)
			{
				// If we are currently connected, gracefully cleanup on the server
				if (_mikanAPI.GetIsConnected())
				{
					_mikanAPI.SendRequest(new DisposeClientRequest()).AwaitResponse();
				}

				// Disconnect and shutdown
				_mikanAPI.Dispose();
				_mikanInitialized = false;
			}
		}

		private void OnMikanLog(MikanLogLevel logLevel, string logMessage)
		{
			switch (logLevel)
			{
				case MikanLogLevel.Debug:
					Console.WriteLine($"DEBUG: {logMessage}");
					break;
				case MikanLogLevel.Info:
					Console.WriteLine($"INFO: {logMessage}");
					break;
				case MikanLogLevel.Warning:
					Console.WriteLine($"WARNING: {logMessage}");
					break;
				case MikanLogLevel.Error:
					Console.WriteLine($"ERROR: {logMessage}");
					break;
				case MikanLogLevel.Fatal:
					Console.WriteLine($"FATAL: {logMessage}");
					break;
				default:
					Console.WriteLine($"UNKNOWN: {logMessage}");
					break;
			}
		}

		public void Update(float deltaSeconds)
		{
			if (_mikanAPI.GetIsConnected())
			{
				// Process all pending Mikan events
				while (_mikanAPI.FetchNextEvent(out MikanEvent nextEvent) == MikanAPIResult.Success)
				{
					if (nextEvent is MikanConnectedEvent)
					{
						HandleMikanConnected();
					}
					else if (nextEvent is MikanDisconnectedEvent disconnectEvent)
					{
						HandleMikanDisconnected(disconnectEvent);
					}
					else if (nextEvent is MikanCameraNewFrameEvent newFrameEvent)
					{
						handleCameraNewFrameEvent(newFrameEvent);
						break; // Process one frame per update
					}
					else if (nextEvent is MikanPropertyUpdateEvent propertyUpdateEvent)
					{
						HandlePropertyUpdateEvent(propertyUpdateEvent);
					}
					else
					{
						HandleMikanEvent(nextEvent);
					}
				}
			}
			else
			{
				// Try to reconnect after timeout
				if (_mikanReconnectTimeout <= 0f)
				{
					if (_mikanAPI.Connect() != MikanAPIResult.Success || !_mikanAPI.GetIsConnected())
					{
						// Timeout before trying to reconnect again
						_mikanReconnectTimeout = 1f;
					}
				}
				else
				{
					_mikanReconnectTimeout -= deltaSeconds;
				}
			}

            // If we aren't connected to Mikan, 
            // send a fake camera frame event with default values to render the default camera target
            if (!_mikanAPI.GetIsConnected())
            {
                handleCameraNewFrameEvent(makeFakeCameraNewFrameEvent());
            }
        }

		// Component Events
		private void HandleComponentListChanged<T>(string ownerSystemName, string componentClassName) where T : PolymorphicStruct
		{
			// Fetch the list of components from Mikan
			var listRequest = new GetComponentListRequest()
			{
				ownerSystem = ownerSystemName,
				componentClassName = componentClassName
			};

			var listResponse = _mikanAPI.SendRequest(listRequest).FetchResponse();
			if (listResponse.resultCode == MikanAPIResult.Success)
			{
				var componentList = listResponse as ComponentListResponse;
				int componentCount = componentList.componentIdList.Count;

				Console.WriteLine($"INFO: {componentClassName} Count: {componentCount}");

				for (int index = 0; index < componentCount; ++index)
				{
					int componentId = componentList.componentIdList[index];

					// TODO: Handle component creation and deletion
					var componentRequest = new ComponentGetValuesRequest()
					{
						ownerSystem = ownerSystemName,
						componentId = componentId
					};

					var response = _mikanAPI.SendRequest(componentRequest).FetchResponse();
					if (response.resultCode == MikanAPIResult.Success)
					{
						var componentValuesResponse = response as ComponentGetValuesResponse;
						var typedComponentValues = componentValuesResponse.valuesObject.Instance as T;

						// Log component values (simplified logging since we don't have TestLogUtils)
						if (typedComponentValues != null)
						{
							Console.WriteLine($"INFO:   Component {componentId}: {typedComponentValues.GetType().Name}");
						}

						// Model geometry comes back as a binary response, the one path a text-only
						// transport would miss, so every model stencil is asked for its mesh
						if (typedComponentValues is MikanModelStencilComponentValues)
						{
							FetchModelStencilGeometry(componentId);
						}
					}
				}
			}
		}

		private void FetchModelStencilGeometry(int stencilId)
		{
			var response = _mikanAPI.SendRequest(new GetModelStencilRenderGeometry() { stencilId = stencilId }).FetchResponse();
			if (response.resultCode == MikanAPIResult.Success &&
				response is MikanStencilModelRenderGeometryResponse geometryResponse)
			{
				int meshCount = geometryResponse.render_geometry.meshes?.Count ?? 0;
				int vertexCount = 0;
				for (int meshIndex = 0; meshIndex < meshCount; ++meshIndex)
				{
					vertexCount += geometryResponse.render_geometry.meshes[meshIndex].vertices?.Count ?? 0;
				}

				Console.WriteLine($"INFO:   Model stencil {stencilId} geometry: {meshCount} meshes, {vertexCount} vertices");
			}
			else
			{
				Console.WriteLine($"ERROR: Model stencil {stencilId} geometry request failed (error_code: {(int)response.resultCode})");
			}
		}

		private void HandleMikanConnected()
		{
			Console.WriteLine("INFO: Connected to Mikan");

			// Initialize client info
			var clientInfo = _mikanAPI.AllocateClientInfo();
			clientInfo.engineName = "MikanXR Test";
			clientInfo.engineVersion = "1.0";
			clientInfo.applicationName = "MikanXR Client Test C#";
			clientInfo.applicationVersion = "1.0";
			clientInfo.graphicsAPI = MikanClientGraphicsApi.Direct3D11;
			clientInfo.supportsRGBA32 = true;
			clientInfo.supportsDepth = true;

			var initClientRequest = new InitClientRequest()
			{
				clientInfo = clientInfo
			};

			_mikanAPI.SendRequest(initClientRequest).AwaitResponse();

			// Fetch all Mikan Components
			// TODO: Update client API code generator to add static strings for system and component class names to avoid hardcoding strings here
			HandleComponentListChanged<MikanAnchorComponentValues>("AnchorObjectSystem", "AnchorComponent");
			HandleComponentListChanged<MikanCameraComponentValues>("CameraObjectSystem", "CameraComponent");
			HandleComponentListChanged<MikanCompositorComponentValues>("CompositorObjectSystem", "CompositorComponent");
			HandleComponentListChanged<MikanMarkerComponentValues>("MarkerObjectSystem", "MarkerComponent");
			HandleComponentListChanged<MikanSceneComponentValues>("SceneObjectSystem", "SceneComponent");
			HandleComponentListChanged<MikanStageComponentValues>("StageObjectSystem", "StageComponent");
			HandleComponentListChanged<MikanQuadStencilComponentValues>("QuadStencilSystem", "QuadStencilComponent");
			HandleComponentListChanged<MikanBoxStencilComponentValues>("BoxStencilSystem", "BoxStencilComponent");
			HandleComponentListChanged<MikanModelStencilComponentValues>("ModelStencilSystem", "ModelStencilComponent");
			HandleComponentListChanged<MikanTrackingMountComponentValues>("TrackingMountObjectSystem", "TrackingMountComponent");
			HandleComponentListChanged<MikanMarkerTrackingVolumeComponentValues>("MarkerTrackingVolumeSystem", "MarkerTrackingVolumeComponent");
			HandleComponentListChanged<MikanVRTrackingVolumeComponentValues>("VRTrackingVolumeSystem", "VRTrackingVolumeComponent");
			HandleComponentListChanged<MikanClientTextureSourceValues>("ClientTextureSourceSystem", "ClientTextureSourceComponent");
			HandleComponentListChanged<MikanSpoutTextureSourceValues>("SpoutTextureSourceSystem", "SpoutTextureSourceComponent");
			HandleComponentListChanged<MikanNetworkVideoSourceValues>("NetworkVideoSourceSystem", "NetworkVideoSourceComponent");
			HandleComponentListChanged<MikanUSBVideoSourceValues>("USBVideoSourceSystem", "USBVideoSourceComponent");
			HandleComponentListChanged<MikanVRDeviceComponentValues>("VRObjectSystem", "VRDeviceComponent");
		}

		private void HandleMikanDisconnected(MikanDisconnectedEvent disconnectEvent)
		{
			string reason = disconnectEvent.reason;
			Console.WriteLine($"INFO: Disconnected from Mikan (reason: {reason})");

			if (disconnectEvent.code == MikanDisconnectCode.IncompatibleVersion)
			{
				// The server has disconnected us because we are using an incompatible version
				// Shutdown since connection is never going to work				
				_shutdownRequested = true;
				Console.WriteLine($"ERROR: Shutting down due to incompatible client");
			}
		}
        public static double DegreesToRadians(double degrees)
        {
            return degrees * Math.PI / 180.0;
        }

        private MikanCameraNewFrameEvent makeFakeCameraNewFrameEvent()
        {
            const float kDefaultHFov = 60.0f;
            const float kDefaultZNear = 0.1f;
            const float kDefaultZFar = 20.0f;

            System.Drawing.Size windowSize = _graphicsContext.WindowSize;

            double aspectRatio = (double)windowSize.Height / (double)windowSize.Width;
            double c_x = (double)windowSize.Width / 2.0;
            double c_y = (double)windowSize.Height / 2.0;
            double hfov = kDefaultHFov;
            double vfov = hfov * aspectRatio;
            double f_x = c_x / Math.Tan(DegreesToRadians(hfov / 2.0));
            double f_y = c_y / Math.Tan(DegreesToRadians(vfov / 2.0));

            MikanCameraNewFrameEvent fakeNewFrameEvent = new MikanCameraNewFrameEvent()
			{
				camera_id = -1,
				camera_forward = new MikanVector3f() { x = 0.0f, y = 0.0f, z = -1.0f },
				camera_up = new MikanVector3f() { x = 0.0f, y = 1.0f, z = 0.0f },
				camera_position = new MikanVector3f() { x = 0.0f, y = 0.0f, z = 0.0f },
				pixel_size = new MikanVector2i() { x = windowSize.Width, y = windowSize.Height },
				focal_length = new MikanVector2d() { x = f_x, y = f_y },
				principal_point = new MikanVector2d() { x = c_x, y = c_y },
				z_bounds = new MikanVector2d() { x = kDefaultZNear, y = kDefaultZFar },
				frame = 0
            };

			return fakeNewFrameEvent;
        }

        private void handleCameraNewFrameEvent(MikanCameraNewFrameEvent newFrameEvent)
		{
			int cameraId = newFrameEvent.camera_id;

			// Get or create render target for this camera
			TestCameraRenderTarget renderTarget = 
				_graphicsContext.GetOrAddCameraRenderTarget(cameraId);

			// Process the frame and render to the camera's render target
			if (renderTarget != null)
			{
				bool bProcessedFrame = 
					renderTarget.ProcessCameraNewFrameEvent(
						_mikanAPI, newFrameEvent);

				if (bProcessedFrame)
				{
					m_lastProcessedCamera = newFrameEvent.camera_id;
				}
			}
		}

		private void HandleMikanEvent(MikanEvent mikanEvent)
		{
			string eventTypeName = mikanEvent.eventTypeName;
			Console.WriteLine($"Received Event: {eventTypeName}");			
		}

		// Property Update Event Handlers
		private void HandlePropertyUpdateEvent(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			MikanPropertyValue propertyValue = propertyUpdateEvent.propertyValue;
			string systemName = propertyValue.ownerSystem;
			string fieldName = propertyValue.fieldName;

			if (propertyValue.componentId != (int)MikanConstants.InvalidMikanID)
			{
				string componentClass = propertyValue.ownerComponentClass;
				var variantStringValue = propertyValue.fieldValue.value_ptr.Instance as MikanStringValue;
                string componentName = variantStringValue.value ?? string.Empty;

                Console.WriteLine($"INFO: Component Field Changed: " +
					$"systemClass: {systemName}, " +
					$"componentClass: {componentClass}, " +
					$"componentName: {componentName}, " +
					$"fieldName: {fieldName}");
			}
			else
			{
				Console.WriteLine($"INFO: System Field Changed: " +
					$"systemClass: {systemName}, " +
					$"fieldName: {fieldName}");
			}

			// Route to appropriate handler based on system name
			if (systemName == "MikanAnchorComponent")
			{
				HandleAnchorPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == "MikanBoxStencilComponent")
			{
				HandleBoxStencilPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == "MikanModelStencilComponent")
			{
				HandleModelStencilPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == "MikanQuadStencilComponent")
			{
				HandleQuadStencilPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == "MikanVRDeviceComponent")
			{
				HandleVRDevicePropertyUpdate(propertyUpdateEvent);
			}
		}

		private void HandleComponentPropertyUpdate(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			if (propertyUpdateEvent.propertyValue.fieldName == "component_name")
			{
				HandleComponentNameChanged(propertyUpdateEvent);
			}
		}

		private void HandleComponentNameChanged(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string componentClass = propertyUpdateEvent.propertyValue.ownerComponentClass;
			var variantStringValue = propertyUpdateEvent.propertyValue.fieldValue.value_ptr.Instance as MikanStringValue;
            string componentName = variantStringValue.value ?? string.Empty;
            int componentId = propertyUpdateEvent.propertyValue.componentId;

			Console.WriteLine($"INFO: Component(class: {componentClass}, id: {componentId}), Name Change: {componentName}");
		}

		// Transform Component Events
		private void HandleTransformPropertyUpdate(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string fieldName = propertyUpdateEvent.propertyValue.fieldName;

			if (fieldName == "relative_scale")
			{
				HandleTransformScaleChanged(propertyUpdateEvent);
			}
			else if (fieldName == "relative_rotation")
			{
				HandleTransformOrientationChanged(propertyUpdateEvent);
			}
			else if (fieldName == "relative_position")
			{
				HandleTransformPositionChanged(propertyUpdateEvent);
			}
			else
			{
				HandleComponentPropertyUpdate(propertyUpdateEvent);
			}
		}

		private void HandleTransformScaleChanged(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string componentClass = propertyUpdateEvent.propertyValue.ownerComponentClass;
			int componentId = propertyUpdateEvent.propertyValue.componentId;
            var variantValue = propertyUpdateEvent.propertyValue.fieldValue.value_ptr.Instance as MikanVector3fValue;
            MikanVector3f scale = variantValue.value ?? new MikanVector3f();

			Console.WriteLine($"INFO: Component(class: {componentClass}, id: {componentId}), " +
				$"Scale Change: {scale.x}, {scale.y}, {scale.z}");
		}

		private void HandleTransformOrientationChanged(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string componentClass = propertyUpdateEvent.propertyValue.ownerComponentClass;
			int componentId = propertyUpdateEvent.propertyValue.componentId;
            var variantValue = propertyUpdateEvent.propertyValue.fieldValue.value_ptr.Instance as MikanVector3fValue;
            MikanVector3f orientation = variantValue.value ?? new MikanVector3f();

			Console.WriteLine($"INFO: Component(class: {componentClass}, id: {componentId}), " +
				$"Orientation Change: {orientation.x}, {orientation.y}, {orientation.z}");
		}

		private void HandleTransformPositionChanged(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string componentClass = propertyUpdateEvent.propertyValue.ownerComponentClass;
			int componentId = propertyUpdateEvent.propertyValue.componentId;
            var variantValue = propertyUpdateEvent.propertyValue.fieldValue.value_ptr.Instance as MikanVector3fValue;
            MikanVector3f position = variantValue.value ?? new MikanVector3f();

			Console.WriteLine($"INFO: Component(class: {componentClass}, id: {componentId}), " +
				$"Position Change: {position.x}, {position.y}, {position.z}");
		}

		// VR Device Events
		private void HandleVRDevicePropertyUpdate(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string fieldName = propertyUpdateEvent.propertyValue.fieldName;

			if (fieldName == "VRDeviceComponentIdList")
			{
				Console.WriteLine("INFO: VR Device component list changed");
			}
			else
			{
				HandleTransformPropertyUpdate(propertyUpdateEvent);
			}
		}

		// Anchor Events
		private void HandleAnchorPropertyUpdate(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string fieldName = propertyUpdateEvent.propertyValue.fieldName;

			if (fieldName == "AnchorComponentIdList")
			{
				Console.WriteLine("INFO: Anchor component list changed");
			}
			else
			{
				HandleTransformPropertyUpdate(propertyUpdateEvent);
			}
		}

		// Box Stencil Events
		private void HandleBoxStencilPropertyUpdate(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string fieldName = propertyUpdateEvent.propertyValue.fieldName;

			if (fieldName == "BoxStencilComponentIdList")
			{
				Console.WriteLine("INFO: Box Stencil component list changed");
			}
			else
			{
				HandleTransformPropertyUpdate(propertyUpdateEvent);
			}
		}

		// Quad Stencil Events
		private void HandleQuadStencilPropertyUpdate(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string fieldName = propertyUpdateEvent.propertyValue.fieldName;

			if (fieldName == "QuadStencilComponentIdList")
			{
				Console.WriteLine("INFO: Quad Stencil component list changed");
			}
			else
			{
				HandleTransformPropertyUpdate(propertyUpdateEvent);
			}
		}

		// Model Stencil Events
		private void HandleModelStencilPropertyUpdate(MikanPropertyUpdateEvent propertyUpdateEvent)
		{
			string fieldName = propertyUpdateEvent.propertyValue.fieldName;

			if (fieldName == "ModelStencilComponentIdList")
			{
				Console.WriteLine("INFO: Model Stencil component list changed");
			}
			else
			{
				HandleTransformPropertyUpdate(propertyUpdateEvent);
			}
		}
	}
}

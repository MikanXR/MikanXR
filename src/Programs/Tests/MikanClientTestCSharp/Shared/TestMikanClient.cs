using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Text;
using System.Web.Script.Serialization;
using MikanXR;

namespace Mikan
{
	public class TestMikanClient : IDisposable
	{
        private TestGraphicsContext _graphicsContext;
		private MikanAPI _mikanAPI;
		private readonly HttpClient _httpClient = new HttpClient();
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

			_httpClient.Dispose();
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

						// Model geometry is served as GLB over HTTP, so every model stencil
						// with a geometry_url has its mesh fetched and checked
						if (typedComponentValues is MikanModelStencilComponentValues modelStencilValues)
						{
							FetchModelStencilGeometry(componentId, modelStencilValues.geometry_url);
						}
					}
				}
			}
		}

		private void FetchModelStencilGeometry(int stencilId, string geometryUrl)
		{
			if (string.IsNullOrEmpty(geometryUrl))
			{
				Console.WriteLine($"INFO:   Model stencil {stencilId} has no geometry");
				return;
			}

			string baseUrl = _mikanAPI.HttpBaseUrl;
			if (string.IsNullOrEmpty(baseUrl))
			{
				Console.WriteLine($"ERROR: Model stencil {stencilId} geometry skipped, no HTTP base url");
				return;
			}

			try
			{
				byte[] glbBytes = _httpClient.GetByteArrayAsync(baseUrl + geometryUrl).GetAwaiter().GetResult();

				if (TryParseGlb(glbBytes, out int meshCount, out int imageCount, out string error))
				{
					Console.WriteLine($"INFO:   Model stencil {stencilId} geometry: {glbBytes.Length} bytes, {meshCount} meshes, {imageCount} images");
				}
				else
				{
					Console.WriteLine($"ERROR: Model stencil {stencilId} geometry is not a valid GLB ({error})");
				}
			}
			catch (Exception e)
			{
				Console.WriteLine($"ERROR: Model stencil {stencilId} geometry request failed ({e.Message})");
			}
		}

		// Checks the GLB header and parses the JSON chunk, reporting its mesh and image counts
		private static bool TryParseGlb(byte[] glb, out int meshCount, out int imageCount, out string error)
		{
			const uint kGlbMagic = 0x46546C67; // "glTF"
			const uint kJsonChunkType = 0x4E4F534A; // "JSON"

			meshCount = 0;
			imageCount = 0;
			error = "";

			if (glb.Length < 20)
			{
				error = "too short";
				return false;
			}

			using (var reader = new BinaryReader(new MemoryStream(glb)))
			{
				if (reader.ReadUInt32() != kGlbMagic)
				{
					error = "bad magic";
					return false;
				}

				uint version = reader.ReadUInt32();
				if (version != 2)
				{
					error = $"unsupported version {version}";
					return false;
				}

				reader.ReadUInt32(); // total length

				uint chunkLength = reader.ReadUInt32();
				if (reader.ReadUInt32() != kJsonChunkType)
				{
					error = "first chunk is not JSON";
					return false;
				}

				if (chunkLength > glb.Length - 20)
				{
					error = "JSON chunk overruns the file";
					return false;
				}

				string json = Encoding.UTF8.GetString(reader.ReadBytes((int)chunkLength));
				var root = new JavaScriptSerializer().DeserializeObject(json) as Dictionary<string, object>;
				if (root == null)
				{
					error = "JSON chunk is not an object";
					return false;
				}

				meshCount = CountArray(root, "meshes");
				imageCount = CountArray(root, "images");
				if (meshCount == 0)
				{
					error = "no meshes";
					return false;
				}
			}

			return true;
		}

		private static int CountArray(Dictionary<string, object> root, string key)
		{
			return root.TryGetValue(key, out object value) && value is System.Collections.ICollection array ? array.Count : 0;
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

			// Fetch all Mikan Components, addressed by the names the bindings generate
			HandleComponentListChanged<MikanAnchorComponentValues>(MikanAnchorComponentValues.k_ownerSystemName, MikanAnchorComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanCameraComponentValues>(MikanCameraComponentValues.k_ownerSystemName, MikanCameraComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanCompositorComponentValues>(MikanCompositorComponentValues.k_ownerSystemName, MikanCompositorComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanMarkerComponentValues>(MikanMarkerComponentValues.k_ownerSystemName, MikanMarkerComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanSceneComponentValues>(MikanSceneComponentValues.k_ownerSystemName, MikanSceneComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanStageComponentValues>(MikanStageComponentValues.k_ownerSystemName, MikanStageComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanQuadStencilComponentValues>(MikanQuadStencilComponentValues.k_ownerSystemName, MikanQuadStencilComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanBoxStencilComponentValues>(MikanBoxStencilComponentValues.k_ownerSystemName, MikanBoxStencilComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanModelStencilComponentValues>(MikanModelStencilComponentValues.k_ownerSystemName, MikanModelStencilComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanTrackingMountComponentValues>(MikanTrackingMountComponentValues.k_ownerSystemName, MikanTrackingMountComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanMarkerTrackingVolumeComponentValues>(MikanMarkerTrackingVolumeComponentValues.k_ownerSystemName, MikanMarkerTrackingVolumeComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanVRTrackingVolumeComponentValues>(MikanVRTrackingVolumeComponentValues.k_ownerSystemName, MikanVRTrackingVolumeComponentValues.k_componentClassName);
			HandleComponentListChanged<MikanClientTextureSourceValues>(MikanClientTextureSourceValues.k_ownerSystemName, MikanClientTextureSourceValues.k_componentClassName);
			HandleComponentListChanged<MikanSpoutTextureSourceValues>(MikanSpoutTextureSourceValues.k_ownerSystemName, MikanSpoutTextureSourceValues.k_componentClassName);
			HandleComponentListChanged<MikanNetworkVideoSourceValues>(MikanNetworkVideoSourceValues.k_ownerSystemName, MikanNetworkVideoSourceValues.k_componentClassName);
			HandleComponentListChanged<MikanUSBVideoSourceValues>(MikanUSBVideoSourceValues.k_ownerSystemName, MikanUSBVideoSourceValues.k_componentClassName);
			HandleComponentListChanged<MikanVRDeviceComponentValues>(MikanVRDeviceComponentValues.k_ownerSystemName, MikanVRDeviceComponentValues.k_componentClassName);
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
			if (systemName == MikanAnchorComponentValues.k_ownerSystemName)
			{
				HandleAnchorPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == MikanBoxStencilComponentValues.k_ownerSystemName)
			{
				HandleBoxStencilPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == MikanModelStencilComponentValues.k_ownerSystemName)
			{
				HandleModelStencilPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == MikanQuadStencilComponentValues.k_ownerSystemName)
			{
				HandleQuadStencilPropertyUpdate(propertyUpdateEvent);
			}
			else if (systemName == MikanVRDeviceComponentValues.k_ownerSystemName)
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

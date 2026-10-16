using System;
using System.Diagnostics;
using SharpDX;
using SharpDX.Windows;
using MikanXR;

namespace Mikan
{
	public enum TestRenderMode
	{
		Color,
		DepthNormalize,
        PackedDepth
    }

	public class TestApp : IDisposable
	{
        private const string CLIENT_NAME = "TestMikanClient";

        private RenderForm _renderForm;
		private TestGraphicsContext _graphicsContext;
		private TestMikanClient _mikanClient;
		private Stopwatch _clock;
		private long _lastUpdateTimestamp = 0;
		private float _timeSeconds = 0f;
		private bool _shutdownRequested = false;
		public RenderForm RenderForm => _renderForm;

		// Application settings
		private const int WindowWidth = 1280;
		private const int WindowHeight = 720;

		// Render settings
		private TestRenderMode _renderMode = TestRenderMode.Color;
		private Vector3 _cubeOffset = new Vector3(0, 0, 10);

		public MikanAPI MikanAPI => _mikanClient?.MikanAPI;
        public TestRenderMode RenderMode => _renderMode;
		public Vector3 CubeOffset => _cubeOffset;
		public float TimeSeconds => _timeSeconds;

		// --managed runs the client over MikanWebSocketTransport instead of MikanClientCore.dll,
		// which exercises the managed transport against a live editor. That transport carries
		// no shared texture writer, so the run renders locally and publishes nothing.
		private bool _useManagedTransport = false;

		public TestApp(string[] args)
		{
			_clock = new Stopwatch();

			foreach (string arg in args)
			{
				if (arg == "--managed")
				{
					_useManagedTransport = true;
				}
			}
		}

		public bool Initialize()
		{
			try
			{
				// Create the render window
				_renderForm = new RenderForm("Mikan Client Test D3D11/C#");
				_renderForm.ClientSize = new System.Drawing.Size(WindowWidth, WindowHeight);
				_renderForm.AllowUserResizing = false;
				_renderForm.KeyPress += OnKeyPress;

				// Create graphics context (DirectX)
				_graphicsContext = new TestGraphicsContext_DX(this);
				if (!_graphicsContext.Create(WindowWidth, WindowHeight))
				{
					Console.WriteLine("ERROR: Failed to create graphics context");
					return false;
				}

				// Create Mikan client
				_mikanClient = new TestMikanClient(_graphicsContext, _useManagedTransport);
				if (!_mikanClient.Initialize(CLIENT_NAME))
				{
					Console.WriteLine("ERROR: Failed to initialize Mikan client");
					return false;
				}

				return true;
			}
			catch (Exception e)
			{
				Console.WriteLine($"ERROR: Failed to initialize: {e.Message}");
				return false;
			}
		}

		public void Run()
		{
			_clock.Start();

			// Start render loop
			// Does not return until close event is received
			RenderLoop.Run(_renderForm, UpdateLoop);

			_clock.Stop();
		}

		public void Dispose()
		{
			_mikanClient?.Dispose();
			_graphicsContext?.Dispose();
			_renderForm?.Dispose();
		}

		public void RequestShutdown()
		{
			_shutdownRequested = true;
		}

		private void UpdateLoop()
		{
			if (_shutdownRequested)
			{
				_renderForm.Close();
				return;
			}

			// Update timekeeping
			long now = _clock.ElapsedMilliseconds;
			float deltaMilliseconds = now - _lastUpdateTimestamp;
			float deltaSecondsClamped = Math.Min(deltaMilliseconds / 1000.0f, 0.1f);
			_timeSeconds += deltaSecondsClamped;
			_lastUpdateTimestamp = now;

			// Update Mikan client
			_mikanClient?.Update(deltaSecondsClamped);

			// Render the main target
			_graphicsContext?.RenderMainTarget();
		}

		private void OnKeyPress(object sender, System.Windows.Forms.KeyPressEventArgs e)
		{
			switch (e.KeyChar)
			{
				case '1':
					_renderMode = TestRenderMode.Color;
					Console.WriteLine("Render Mode: Color");
					break;
				case '2':
					_renderMode = TestRenderMode.DepthNormalize;
					Console.WriteLine("Render Mode: Depth Normalize");
					break;
                case '3':
                    _renderMode = TestRenderMode.PackedDepth;
                    Console.WriteLine("Render Mode: Packed Depth");
                    break;
                case 'w':
					_cubeOffset.Z += 0.1f;
					break;
				case 's':
					_cubeOffset.Z -= 0.1f;
					break;
				case 'a':
					_cubeOffset.X -= 0.1f;
					break;
				case 'd':
					_cubeOffset.X += 0.1f;
					break;
				case 'q':
					_cubeOffset.Y += 0.1f;
					break;
				case 'e':
					_cubeOffset.Y -= 0.1f;
					break;
			}
		}
	}
}

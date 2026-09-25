using System;
using System.Runtime.InteropServices;
using System.Text;

namespace MikanXR
{
	// Field-for-field mirror of MikanRenderTargetDescriptor in MikanCoreTypes.h, which the core
	// C API reads and writes directly. Sequential layout means every field has to be present and
	// in the same order: a missing one shifts everything after it, and since
	// Mikan_GetCameraRenderTargetDescriptor takes this as an out parameter, a struct that is
	// short also gets written past its end. Keep this in step with that header.
	[StructLayout(LayoutKind.Sequential)]
	public struct MikanRenderTargetDescriptor_Native
	{
		public MikanColorBufferType color_buffer_type;
		public MikanDepthBufferType depth_buffer_type;
		public MikanShadowBufferType shadow_buffer_type;
		// Size of the color buffer
		public uint width;
		public uint height;
		// Size of the depth and shadow buffers. 0 means same as width/height.
		public uint aux_width;
		public uint aux_height;
		public MikanClientGraphicsApi graphicsAPI;
	};

	public class MikanCoreNative
	{
		[global::System.Runtime.InteropServices.UnmanagedFunctionPointer(global::System.Runtime.InteropServices.CallingConvention.Cdecl)]
		public delegate void NativeLogCallback(
			int log_level,
			[global::System.Runtime.InteropServices.MarshalAs(global::System.Runtime.InteropServices.UnmanagedType.LPStr)]
			string log_message);

		[global::System.Runtime.InteropServices.UnmanagedFunctionPointer(global::System.Runtime.InteropServices.CallingConvention.Cdecl)]
		public delegate void NativeTextResponseCallback(
			int request_id,
			[global::System.Runtime.InteropServices.MarshalAs(global::System.Runtime.InteropServices.UnmanagedType.LPUTF8Str)]
			string utf8_response_string,
			IntPtr userdata);

		[global::System.Runtime.InteropServices.UnmanagedFunctionPointer(global::System.Runtime.InteropServices.CallingConvention.Cdecl)]
		public delegate void NativeBinaryResponseCallback(
			IntPtr buffer,
			UIntPtr buffer_size,
			IntPtr userdata);

		[DllImport("MikanClientCore.dll", CharSet = CharSet.Ansi)]
		public static extern int Mikan_Initialize(
			string client_name,
			MikanLogLevel min_log_level,
			NativeLogCallback log_callback,
			out IntPtr out_context);

		[DllImport("MikanClientCore.dll")]
		public static extern IntPtr Mikan_GetClientName(IntPtr context);
		public static string GetClientName(IntPtr context)
		{
			return Marshal.PtrToStringAnsi(Mikan_GetClientName(context));
		}

		[DllImport("MikanClientCore.dll")]
		public static extern bool Mikan_GetIsInitialized(IntPtr context);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_SetGraphicsDeviceInterface(
			IntPtr context,
			MikanClientGraphicsApi api, 
			IntPtr graphicsDeviceInterface);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_GetGraphicsDeviceInterface(
			IntPtr context,
			MikanClientGraphicsApi api,
			out IntPtr outGraphicsDeviceInterface);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_SetGraphicsCommandQueueInterface(
			IntPtr context,
			MikanClientGraphicsApi api,
			IntPtr graphicsCommandQueueInterface);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_GetGraphicsCommandQueueInterface(
			IntPtr context,
			MikanClientGraphicsApi api,
			out IntPtr outGraphicsCommandQueueInterface);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_GetCameraRenderTargetDescriptor(
			IntPtr context,
			int camera_id,
			out MikanRenderTargetDescriptor_Native out_descriptor);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_AllocateCameraRenderTargetTextures(
			IntPtr context,
			int camera_id,
			ref MikanRenderTargetDescriptor_Native descriptor);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_FreeCameraRenderTargetTextures(
			IntPtr context,
			int camera_id);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_WriteCameraColorRenderTargetTexture(
			IntPtr context,
			int camera_id,
			IntPtr color_texture);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_WriteCameraDepthRenderTargetTexture(
			IntPtr context,
			int camera_id,
			IntPtr depth_texture,
			float z_near,
			float z_far);

		[DllImport("MikanClientCore.dll")]
		public static extern IntPtr Mikan_GetCameraPackDepthTextureResourcePtr(
			IntPtr context,
			int camera_id);

		[DllImport("MikanClientCore.dll", CharSet = CharSet.Ansi)]
		public static extern int Mikan_Connect(
			IntPtr context,
			string host, 
			string port);

		[DllImport("MikanClientCore.dll", CharSet = CharSet.Ansi)]
		public static extern int Mikan_Disconnect(
			IntPtr context,
			UInt16 code,
			string reason);

		[DllImport("MikanClientCore.dll")]
		public static extern bool Mikan_GetIsConnected(IntPtr context);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_FetchNextEvent(
			IntPtr context,
			UIntPtr utf8_buffer_size,
			[MarshalAs(UnmanagedType.LPUTF8Str)] 
			StringBuilder out_utf8_buffer,
			out UIntPtr out_utf8_bytes_written);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_SendRequestJSON(
			IntPtr context,
			[MarshalAs(UnmanagedType.LPUTF8Str)]
			string utf8_request_json);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_SetTextResponseCallback(
			IntPtr context,
			NativeTextResponseCallback callback, 
			IntPtr callback_userdata);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_SetBinaryResponseCallback(
			IntPtr context,
			NativeBinaryResponseCallback callback,
			IntPtr callback_userdata);

		[DllImport("MikanClientCore.dll")]
		public static extern int Mikan_Shutdown(IntPtr context);
	}
}
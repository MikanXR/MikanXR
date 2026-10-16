// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanClientGraphicsApi
	{
		UNKNOWN= -1,
		Direct3D9= 0,
		Direct3D11= 1,
		Direct3D12= 2,
		OpenGL= 3,
		Metal= 4,
		Vulkan= 5,
	};

	public enum MikanColorBufferType
	{
		NOCOLOR= 0,
		RGB24= 1,
		RGBA32= 2,
		BGRA32= 3,
		RGBA16F= 4,
	};

	public enum MikanConstants
	{
		InvalidMikanID= -1,
		ClientAPIVersion= 0,
	};

	public enum MikanCoreResult
	{
		Success= 0,
		GeneralError= 1,
		Uninitialized= 2,
		NullParam= 3,
		InvalidParam= 4,
		RequestFailed= 5,
		NotConnected= 6,
		AlreadyConnected= 7,
		SocketError= 8,
		Timeout= 9,
		Canceled= 10,
		NoData= 11,
		BufferTooSmall= 12,
		UnknownClient= 13,
		UnknownFunction= 14,
		MalformedParameters= 15,
		MalformedResponse= 16,
	};

	public enum MikanDepthBufferType
	{
		NODEPTH= 0,
		FLOAT_DEVICE_DEPTH= 1,
		FLOAT_SCENE_DEPTH= 2,
		PACK_DEPTH_RGBA= 3,
	};

	public enum MikanDisconnectCode
	{
		Normal= 1000,
		ProtocolError= 1002,
		NoStatus= 1005,
		AbnormalClose= 1006,
		InvalidFramePayload= 1007,
		InternalError= 1011,
		IncompatibleVersion= 2000,
	};

	public enum MikanLogLevel
	{
		Trace= 0,
		Debug= 1,
		Info= 2,
		Warning= 3,
		Error= 4,
		Fatal= 5,
	};

	public enum MikanShadowBufferType
	{
		NOSHADOW= 0,
		RGB24= 1,
		RGBA32= 2,
		BGRA32= 3,
		RGBA16F= 4,
	};

}

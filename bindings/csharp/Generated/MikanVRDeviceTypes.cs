// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanVRDeviceApi
	{
		INVALID= 0,
		STEAM_VR= 1,
	};

	public enum MikanVRDeviceType
	{
		INVALID= 0,
		HMD= 1,
		CONTROLLER= 2,
		TRACKER= 3,
	};

	public class MikanVRDeviceComponentValues : MikanTransformComponentValues
	{
		public const string k_componentClassName = "VRDeviceComponent";
		public const string k_ownerSystemName = "VRObjectSystem";

		[MikanFieldOrder(0)] public MikanVRDeviceApi vr_device_api;
		[MikanFieldOrder(1)] public MikanVRDeviceType vr_device_type;
		[MikanFieldOrder(2)] public int vr_device_index;
		[MikanFieldOrder(3)] public string vr_device_path;
		[MikanFieldOrder(4)] public List<string> socket_names;
	};

	public class MikanVRObjectSystemValues : MikanSystemValues
	{
		public const string k_systemName = "VRObjectSystem";

		[MikanFieldOrder(0)] public List<string> vr_device_path_list;
	};

}

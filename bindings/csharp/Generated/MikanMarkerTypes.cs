// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanMarkerDictionaryType
	{
		INVALID= -1,
		DICT_4x4= 0,
		DICT_5x5= 1,
		DICT_6x6= 2,
		DICT_7x7= 3,
	};

	public class MikanMarkerComponentValues : MikanComponentValues
	{
		[MikanFieldOrder(0)] public int aruco_id;
		[MikanFieldOrder(1)] public float length_mm;
	};

	public class MikanMarkerSystemValues : MikanSystemValues
	{
		[MikanFieldOrder(0)] public List<int> aruco_id_list;
		[MikanFieldOrder(1)] public MikanMarkerDictionaryType aruco_dictionary_type;
		[MikanFieldOrder(2)] public int charuco_rows;
		[MikanFieldOrder(3)] public int charuco_cols;
		[MikanFieldOrder(4)] public float charuco_square_length_mm;
		[MikanFieldOrder(5)] public float charuco_marker_length_mm;
		[MikanFieldOrder(6)] public MikanMarkerDictionaryType charuco_dictionary_type;
	};

}

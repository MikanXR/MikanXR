using System;
using System.Collections.Generic;

namespace MikanXR
{
	// Creates wire objects by their type name: events and responses by the name they stamp into
	// themselves, polymorphic values by their class_name. The generated half registers every
	// generated class. A hand-written polymorphic type registers itself before it is read.
	public static partial class MikanTypeRegistry
	{
		private static readonly Dictionary<string, Func<MikanSerializableObject>> s_factories =
			new Dictionary<string, Func<MikanSerializableObject>>();

		static MikanTypeRegistry()
		{
			RegisterGeneratedTypes();
		}

		static partial void RegisterGeneratedTypes();

		public static void Register(string typeName, Func<MikanSerializableObject> factory)
		{
			lock (s_factories)
			{
				s_factories[typeName] = factory;
			}
		}

		// A new instance of the named type, or null when no type has that name
		public static MikanSerializableObject Create(string typeName)
		{
			Func<MikanSerializableObject> factory;
			lock (s_factories)
			{
				if (!s_factories.TryGetValue(typeName, out factory))
				{
					return null;
				}
			}

			return factory();
		}

		public static List<string> GetTypeNames()
		{
			lock (s_factories)
			{
				return new List<string>(s_factories.Keys);
			}
		}
	}
}

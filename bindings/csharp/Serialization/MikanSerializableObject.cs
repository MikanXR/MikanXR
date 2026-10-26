using Newtonsoft.Json.Linq;

namespace MikanXR
{
	// The root of every class that crosses the wire. The generated classes override these with
	// their own fields, calling the base first so parent fields come first, which is the wire
	// order. Reading and writing fields this way rather than through reflection lets the runtime
	// compile into hosts that forbid System.Reflection.
	public abstract class MikanSerializableObject
	{
		// Adds this object's fields to json, parents first
		public virtual void WriteFields(JObject json)
		{
		}

		// Fills this object's fields from json, parents first. Throws when a field is missing or
		// holds the wrong kind of value.
		public virtual void ReadFields(JObject json)
		{
		}

		// Reads a field by its wire name, boxed. False when no field has that name.
		public virtual bool TryGetField(string fieldName, out object value)
		{
			value = null;
			return false;
		}

		// Writes a field by its wire name, converting the value to the field's type where it can.
		// False when no field has that name or the value cannot become the field's type.
		public virtual bool TrySetField(string fieldName, object value)
		{
			return false;
		}
	}
}

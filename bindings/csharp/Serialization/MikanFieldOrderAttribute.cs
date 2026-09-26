using System;

namespace MikanXR
{
	/// <summary>
	/// The position of a field in its type's binary wire layout.
	/// </summary>
	/// <remarks>
	/// A binary payload is the concatenation of a type's fields in one fixed order, so the order
	/// this runtime walks them in has to match the order the editor wrote them in. .NET does not
	/// guarantee that Type.GetFields() returns fields in declaration order (see the Remarks on
	/// https://learn.microsoft.com/en-us/dotnet/api/system.type.getfields), so the generated
	/// bindings state the order outright rather than leaving the runtime to infer it. The value
	/// is the field's index within its own class, matching the C++ side's
	/// Serialization::getStructFieldsInWireOrder; inherited fields are ordered by the same rule
	/// within the class that declares them, and base classes come first.
	/// </remarks>
	[AttributeUsage(AttributeTargets.Field, AllowMultiple = false, Inherited = false)]
	public sealed class MikanFieldOrderAttribute : Attribute
	{
		public MikanFieldOrderAttribute(int order)
		{
			Order = order;
		}

		public int Order { get; }
	}
}

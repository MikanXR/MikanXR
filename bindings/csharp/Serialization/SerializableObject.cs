namespace MikanXR
{
	// A struct a PolymorphicObject can hold. Each generated subclass names itself, so the wire's
	// class_name never comes from reflecting on the instance's type.
	public class PolymorphicStruct : MikanSerializableObject
	{
		public virtual string RuntimeClassName => nameof(PolymorphicStruct);
	}

	public class PolymorphicObject
	{
		private string _runtimeClassName = "";
		public string RuntimeClassName => _runtimeClassName;

		private PolymorphicStruct _instance;
		public PolymorphicStruct Instance => _instance;

		public PolymorphicObject()
		{
		}

		public PolymorphicObject(PolymorphicStruct instance)
		{
			setInstance(instance);
		}

		public void setInstance(PolymorphicStruct instance)
		{
			_runtimeClassName = instance.RuntimeClassName;
			_instance = instance;
		}
	}
}

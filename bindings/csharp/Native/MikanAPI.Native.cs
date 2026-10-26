namespace MikanXR
{
	public partial class MikanAPI
	{
		// The native client over MikanClientCore.dll, with its own shared texture writer
		public MikanAPI()
			: this(new MikanNativeTransport(), null)
		{
			_renderTargetAPI.Writer= new MikanNativeRenderTargetWriter((MikanNativeTransport)_transport);
		}
	}
}

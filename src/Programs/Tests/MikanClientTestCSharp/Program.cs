using System;

namespace Mikan
{
	class Program
	{
		[STAThread]
		static void Main(string[] args)
		{
			using (var app = new TestApp(args))
			{
				if (app.Initialize())
				{
					app.Run();
				}
				else
				{
					Console.WriteLine("ERROR: Failed to initialize application");
				}
			}
		}
	}
}

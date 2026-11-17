using System.IO;
using UnrealBuildTool;

public class JiweSDK : ModuleRules
{
	public JiweSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",
			"Json",
			"Sockets",
			"Networking"
		});

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Adds the login redirect (deep link) to AndroidManifest.xml and GameActivity.java.
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "JiweSDK_UPL_Android.xml"));
		}
	}
}

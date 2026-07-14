using System.IO;
using UnrealBuildTool;

public class LibVlc : ModuleRules
{
    public LibVlc(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string thirdPartyPath = Path.Combine(ModuleDirectory, "vlc");
	    string sdkDirectory = Path.Combine(thirdPartyPath, "Win64", "sdk");
        string libPath = Path.Combine(sdkDirectory, "lib");
        
        // Include files
        PublicSystemIncludePaths.Add(Path.Combine(sdkDirectory, "include"));
        PublicSystemIncludePaths.Add(Path.Combine(sdkDirectory, "include", "vlc"));
        PublicSystemIncludePaths.Add(Path.Combine(sdkDirectory, "include", "vlc", "plugins"));
        
        // Library files
        PublicAdditionalLibraries.Add(Path.Combine(libPath, "libvlc.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(libPath, "libvlccore.lib"));

        // delay load
        //PublicDelayLoadDLLs.Add(Path.Combine(thirdPartyPath, "libvlc.dll"));
        //PublicDelayLoadDLLs.Add(Path.Combine(thirdPartyPath, "libvlccore.dll"));
        
        // Stage *.dll files
        string dllPath = Path.Combine(thirdPartyPath, "Win64");
        foreach (string dllFile in Directory.EnumerateFiles(dllPath, "*.dll", SearchOption.AllDirectories))
        {
            string relativeFilePath = Path.GetRelativePath(dllPath, dllFile);
            RuntimeDependencies.Add("$(BinaryOutputDir)/" + relativeFilePath, dllFile);
        }
    }
}
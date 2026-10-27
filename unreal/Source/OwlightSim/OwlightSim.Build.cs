using UnrealBuildTool;
using System.IO;

// The simulation core (repo sim/), compiled as its own module. The sources under sim/ are copied
// in by unreal/sync_sim.py and are not committed here; the repo's sim/ is the source of truth.
public class OwlightSim : ModuleRules
{
	public OwlightSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "sim", "include"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "sim", "src"));

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		// The C interface is exported from this module's DLL in modular builds.
		PublicDefinitions.Add("OWLIGHT_SHARED=1");
		PrivateDefinitions.Add("OWLIGHT_BUILD=1");

		// The core is plain C++; keep the engine's macros out of it.
		bEnableExceptions = false;
		UndefinedIdentifierWarningLevel = WarningLevel.Off;
		ShadowVariableWarningLevel = WarningLevel.Off;
	}
}

# PLANCK ([Nexus link](https://www.nexusmods.com/skyrimspecialedition/mods/66025))

This mod is also complicated.

## Reproducible Windows build

The repository does not contain Havok or SKSEVR SDK content. To build the patched
PLANCK submodule on Windows, provide your own `hk2010_2_0_r1.7z` archive and use
the official SKSEVR 2.0.12 source archive. The preparation script verifies these
SHA256 values before extracting only the source trees needed by this project:

- Havok 2010.2: `7349946401a820784fc86aa13bc667def6c409ed938865b01c8e6c3d86692555`
- SKSEVR 2.0.12: `f03df5d8663f2c9a781f830fb0809c63a9a0e3b626d6d1a96e38493f81a3c9ad`

Install 7-Zip and Visual Studio Build Tools with MSBuild and the C++ desktop
workload (including the project’s `v145` toolset). Choose a dependency directory
outside this repository; SDK content is deliberately never extracted into git.

```bat
BuildPlanckSkyrimTogetherVR-Windows.bat ^
  -HavokArchive "D:\Downloads\hk2010_2_0_r1.7z" ^
  -DependencyRoot "D:\SkyrimBuildDependencies\planck" ^
  -Configuration Release
```

When SKSEVR is not already present in `DependencyRoot`, the wrapper downloads
only the official archive from `https://skse.silverlock.org/beta/sksevr_2_00_12.7z`.
It never downloads Havok. Preparation is idempotent: it verifies both archives,
then reuses complete extracted trees. To prepare without building, run:

```powershell
.\Tools\SkyrimVR\prepare_planck_dependencies.ps1 `
  -HavokArchive 'D:\Downloads\hk2010_2_0_r1.7z' `
  -DependencyRoot 'D:\SkyrimBuildDependencies\planck'
```

The resulting DLL is reported as `PLANCK_ARTIFACT=...`. No game files are copied
unless you explicitly pass `-SkyrimVRPath 'D:\Steam\steamapps\common\SkyrimVR'`.
The project receives `Havok2010Source`, `SKSEVRSourceRoot`, and
`SKSECommonSourceRoot` as MSBuild properties, and validates representative headers
before compilation.


PLANCK has a C++ API that can be used by other mods.\
To use it, copy `src/planckinterface001.cpp` and `include/planckinterface001.h` into your project.\
Then, do something like this in your SKSE plugin in `PostPostLoad` or later (this is important - if you try and get the interface before `PostPostLoad`, such as in `PostLoad`, it will not work).

```cpp
#include "planckinterface001.h"

...

void OnSKSEMessage(SKSEMessagingInterface::Message* msg)
{
  if (msg) {
    if (msg->type == SKSEMessagingInterface::kMessage_PostPostLoad) {
      // Get the PLANCK plugin API
      PlanckPluginAPI::GetPlanckInterface001(g_pluginHandle, g_messaging);
      if (g_planckInterface) {
        _MESSAGE("Got planck interface!");
        unsigned int planckVersion = g_planckInterface->GetBuildNumber();
      }
    }
  }
}
```
To receive hit events with extended info provided by PLANCK, you do not need to fetch the interface (though there is no harm in it).\
Planck hit events are simply extensions to regular hit events. When receiving a hit event within a regular hit event handler, check if it's a hit from planck and then simply treat it as a planck hit event, like so.
```cpp
class HitEventHandler : public BSTEventSink <TESHitEvent>
{
public:
  virtual EventResult ReceiveEvent(TESHitEvent *evn, EventDispatcher<TESHitEvent> *dispatcher)
  {
    PlayerCharacter *player = *g_thePlayer;
    if (evn->caster == player) {
      if (PlanckPluginAPI::IsPlanckHit(evn)) {
        PlanckPluginAPI::PlanckHitEvent *extendedEvent = (PlanckPluginAPI::PlanckHitEvent *)evn;
        _MESSAGE("%s", extendedEvent->extendedHitData.nodeName);
      }
    }
    return kEvent_Continue;
  }
};
```

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#pragma once

#define AUTO_VAR(_var, _val) auto _var = _val

typedef unsigned __int64 __uint64;

#ifdef _WINDOWS64
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <malloc.h>
#include <tchar.h>
// TODO: reference additional headers your program requires here
#include <d3d11.h>
#endif





#include <unordered_map>
#include <unordered_set>
#include <sal.h>
#include <vector>

#include <memory>

#include <list>
#include <map>
#include <set>
#include <queue>
#include <deque>
#include <algorithm>
#include <math.h>
#include <limits>
#include <string>
#include <sstream>
#include <iostream>
#include <exception>

#include <assert.h>

#include "extraX64.h"

#include "Definitions.h"
#include "Class.h"
#include "Exceptions.h"
#include "Mth.h"
#include "StringHelpers.h"
#include "ArrayWithLength.h"
#include "Random.h"
#include "TilePos.h"
#include "ChunkPos.h"
#include "compression.h"
#include "PerformanceTimer.h"


#ifdef _FINAL_BUILD
#define printf BREAKTHECOMPILE
#define wprintf BREAKTHECOMPILE
#undef OutputDebugString
#define OutputDebugString BREAKTHECOMPILE
#define OutputDebugStringA BREAKTHECOMPILE
#define OutputDebugStringW BREAKTHECOMPILE
#endif


void MemSect(int sect);

#include "..\Minecraft.Client\Windows64\4JLibs\inc\4J_Profile.h"
#include "..\Minecraft.Client\Windows64\4JLibs\inc\4J_Render.h"
#include "..\Minecraft.Client\Windows64\4JLibs\inc\4J_Storage.h"
#include "..\Minecraft.Client\Windows64\4JLibs\inc\4J_Input.h"

#include "..\Minecraft.Client\Common\Network\GameNetworkManager.h"

// #ifdef _XBOX
#include "..\Minecraft.Client\Common\UI\UIEnums.h"
#include "..\Minecraft.Client\Common\App_defines.h"
#include "..\Minecraft.Client\Common\App_enums.h"
#include "..\Minecraft.Client\Common\Tutorial\TutorialEnum.h"
#include "..\Minecraft.Client\Common\App_structs.h"
//#endif

#include "..\Minecraft.Client\Common\Consoles_App.h"
#include "..\Minecraft.Client\Common\Minecraft_Macros.h"
#include "..\Minecraft.Client\Common\Colours\ColourTable.h"

#include "..\Minecraft.Client\Common\BuildVer.h"

#include "..\Minecraft.Client\Windows64\Windows64_App.h"
#include "..\Minecraft.Client\Windows64Media\strings.h"
#include "..\Minecraft.Client\Windows64\Sentient\SentientTelemetryCommon.h"
#include "..\Minecraft.Client\Windows64\Sentient\MinecraftTelemetry.h"


#include "..\Minecraft.Client\Common\DLC\DLCSkinFile.h"
#include "..\Minecraft.Client\Common\Console_Awards_enum.h"
#include "..\Minecraft.Client\Common\Potion_Macros.h"
#include "..\Minecraft.Client\Common\Console_Debug_enum.h"
#include "..\Minecraft.Client\Common\GameRules\ConsoleGameRulesConstants.h"
#include "..\Minecraft.Client\Common\GameRules\ConsoleGameRules.h"
#include "..\Minecraft.Client\Common\Telemetry\TelemetryManager.h"

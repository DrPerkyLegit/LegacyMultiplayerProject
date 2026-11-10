// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

//#include <xtl.h>
//#include <xboxmath.h>

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __LOC__ __FILE__ "("__STR1__(__LINE__)") : 4J Warning Msg: "

// use  - #pragma message(__LOC__"Need to do something here")

// #ifndef _XBOX
// #ifdef _CONTENT_PACKAGE
// #define TO_BE_IMPLEMENTED
// #endif
// #endif

#define AUTO_VAR(_var, _val) auto _var = _val
#include <unordered_map>
#include <unordered_set>
#include <vector>
typedef unsigned __int64 __uint64;

#ifdef  _WINDOWS64
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <malloc.h>
#include <tchar.h>
// TODO: reference additional headers your program requires here
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX; 

#define HRESULT_SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)

#endif






#include "extraX64.h"


// C RunTime Header Files
#include <stdlib.h>

#include <memory>

#include <list>
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>
#include <exception>

#include <assert.h>


#include "..\Minecraft.World\Definitions.h"
#include "..\Minecraft.World\class.h"
#include "..\Minecraft.World\ArrayWithLength.h"
#include "..\Minecraft.World\SharedConstants.h"
#include "..\Minecraft.World\Random.h"
#include "..\Minecraft.World\compression.h"
#include "..\Minecraft.World\PerformanceTimer.h"

	#include "Windows64\4JLibs\inc\4J_Input.h"	
	#include "Windows64\4JLibs\inc\4J_Profile.h"
	#include "Windows64\4JLibs\inc\4J_Render.h"
	#include "Windows64\4JLibs\inc\4J_Storage.h"

	#include "Windows64\Windows64_KeyboardInput.h"

#include "Textures.h"
#include "Font.h"
#include "ClientConstants.h"
#include "Gui.h"
#include "Screen.h"
#include "ScreenSizeCalculator.h"
#include "Minecraft.h"
#include "MemoryTracker.h"
#include "stubs.h"
#include "BufferedImage.h"

#include "Common\Network\GameNetworkManager.h"


#include "Common\UI\UIEnums.h"
#include "Common\UI\UIStructs.h"
// #ifdef _XBOX
#include "Common\App_defines.h"
#include "Common\App_enums.h"
#include "Common\Tutorial\TutorialEnum.h"
#include "Common\App_structs.h"
//#endif

#include "Common\Consoles_App.h"
#include "Common\Minecraft_Macros.h"
#include "Common\BuildVer.h"

	#include "Windows64\Sentient\MinecraftTelemetry.h"
	#include "Windows64Media\strings.h"
	#include "Windows64\Windows64_App.h"
	#include "Windows64\Sentient\DynamicConfigurations.h"
	#include "Windows64\Sentient\SentientTelemetryCommon.h"
	#include "Windows64\GameConfig\Minecraft.spa.h"
	#include "Windows64\XML\ATGXmlParser.h"	
	#include "Windows64\Social\SocialManager.h"
	#include "Common\Audio\SoundEngine.h"
	#include "Windows64\Iggy\include\iggy.h"
	#include "Windows64\Iggy\gdraw\gdraw_d3d11.h"
	#include "Windows64\Windows64_UIController.h"

#include "Common\ConsoleGameMode.h"
#include "Common\Console_Debug_enum.h"
#include "Common\Console_Awards_enum.h"
#include "Common\Tutorial\TutorialMode.h"
#include "Common\Tutorial\Tutorial.h"
#include "Common\Tutorial\FullTutorialMode.h"
#include "Common\Trial\TrialMode.h"
#include "Common\GameRules\ConsoleGameRules.h"
#include "Common\GameRules\ConsoleSchematicFile.h"
#include "Common\Colours\ColourTable.h"
#include "Common\DLC\DLCSkinFile.h"
#include "Common\DLC\DLCManager.h"
#include "Common\DLC\DLCPack.h"
#include "Common\Telemetry\TelemetryManager.h"

#include "extraX64client.h"



#ifdef _FINAL_BUILD
#define printf BREAKTHECOMPILE
#define wprintf BREAKTHECOMPILE
#undef OutputDebugString
#define OutputDebugString BREAKTHECOMPILE
#define OutputDebugStringA BREAKTHECOMPILE
#define OutputDebugStringW BREAKTHECOMPILE
#endif

void MemSect(int sect);

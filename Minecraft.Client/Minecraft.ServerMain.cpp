#include "stdafx.h"

#include "../Minecraft.World/TilePos.h"
#include "Common/Consoles_App.h"
#include "Common/Network/GameNetworkManager.h"
#include "Minecraft.h"
#include "MinecraftServer.h"
#include "Tesselator.h"
#include "Windows64/Windows64_App.h"
#include "Windows64/Windows64_Directories.h"

#include "../Minecraft.World/AABB.h"
#include "../Minecraft.World/IntCache.h"
#include "../Minecraft.World/Level.h"
#include "../Minecraft.World/OldChunkStorage.h"
#include "../Minecraft.World/LevelSettings.h"
#include "../Minecraft.World/Tile.h"
#include "../Minecraft.World/Vec3.h"
#include "../Minecraft.World/compression.h"

static volatile LONG g_shutdownRequested = FALSE;

static BOOL WINAPI HandleConsoleControl(DWORD controlType)
{
	if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT || controlType == CTRL_CLOSE_EVENT || controlType == CTRL_SHUTDOWN_EVENT)
	{
		InterlockedExchange(&g_shutdownRequested, TRUE);
		return TRUE;
	}

	return FALSE;
}

static bool SetServerWorkingDirectory()
{
	wchar_t executablePath[MAX_PATH] = {};
	DWORD pathLength = GetModuleFileNameW(NULL, executablePath, MAX_PATH);
	if (pathLength == 0 || pathLength >= MAX_PATH)
	{
		return false;
	}

	wchar_t* directoryEnd = wcsrchr(executablePath, L'\\');
	if (directoryEnd != NULL)
	{
		*(directoryEnd + 1) = L'\0';
		return SetCurrentDirectoryW(executablePath) != FALSE;
	}
	return false;
}

static void StartHostedWorld()
{
	app.ClearTerrainFeaturePosition();
	StorageManager.ResetSaveData();
	StorageManager.SetSaveTitle(L"TestWorld");

	app.SetGameHostOption(eGameHostOption_Difficulty, 0);
	app.SetGameHostOption(eGameHostOption_FriendsOfFriends, 0);
	app.SetGameHostOption(eGameHostOption_Gamertags, 1);
	app.SetGameHostOption(eGameHostOption_BedrockFog, 1);
	app.SetGameHostOption(eGameHostOption_GameType, GameType::CREATIVE->getId());
	app.SetGameHostOption(eGameHostOption_LevelType, 0);
	app.SetGameHostOption(eGameHostOption_Structures, 1);
	app.SetGameHostOption(eGameHostOption_BonusChest, 0);
	app.SetGameHostOption(eGameHostOption_PvP, 1);
	app.SetGameHostOption(eGameHostOption_TrustPlayers, 1);
	app.SetGameHostOption(eGameHostOption_FireSpreads, 1);
	app.SetGameHostOption(eGameHostOption_TNT, 1);
	app.SetGameHostOption(eGameHostOption_HostCanFly, 1);
	app.SetGameHostOption(eGameHostOption_HostCanChangeHunger, 1);
	app.SetGameHostOption(eGameHostOption_HostCanBeInvisible, 1);

	NetworkGameInitData* initData = new NetworkGameInitData();
	initData->seed = 0;
	initData->saveData = NULL;
	initData->settings = app.GetGameHostOption(eGameHostOption_All);

	g_NetworkManager.FakeLocalPlayerJoined(true);
	app.SetAutosaveTimerTime();
	C4JThread* startThread = new C4JThread(&CGameNetworkManager::RunNetworkGameThreadProc, (LPVOID)initData, "RunNetworkGame");
	startThread->Run();
}

int main(int argc, char** argv)
{
	if (argc > 1)
	{
		if (_stricmp(argv[1], "-help") == 0 || _stricmp(argv[1], "--help") == 0 || _stricmp(argv[1], "-h") == 0)
		{
			printf("Minecraft.Server\nStarts the default hosted legacy world.\nPress Ctrl+C to stop.\n");
			return 0;
		}

		fprintf(stderr, "Unknown argument: %s\n", argv[1]);
		fprintf(stderr, "Usage: Minecraft.Server.exe [-help]\n");
		return 1;
	}

	if (!SetServerWorkingDirectory())
	{
		fprintf(stderr, "Unable to set the working directory to the executable folder.\n");
		return 1;
	}
	if (!EnsureWindows64GameHddDirectory())
	{
		fprintf(stderr, "Unable to create the Windows64\\GameHDD directory.\n");
		return 1;
	}
	SetConsoleCtrlHandler(HandleConsoleControl, TRUE);

	printf("Loading media...\n");
	app.loadMediaArchive();
	printf("Loading strings...\n");
	app.loadStringTable();
	printf("Initializing profile and network...\n");
	InputManager.Initialise(1, 3, MINECRAFT_ACTION_MAX, ACTION_MAX_MENU);
	DWORD profileSettings[] = { 0, 0, 0, 0, 0 };

	ProfileManager.Initialise(
		TITLEID_MINECRAFT,
		app.m_dwOfferID,
		PROFILE_VERSION_10,
		5,
		4,
		profileSettings,
		app.GAME_DEFINED_PROFILE_DATA_BYTES * XUSER_MAX_COUNT,
		&app.uiGameDefinedDataChangedBitmask);
	ProfileManager.SetDefaultOptionsCallback(&CConsoleMinecraftApp::DefaultOptionsCallback, (LPVOID)&app);
	ProfileManager.SetDebugFullOverride(true);

	g_NetworkManager.Initialise();

	printf("Initializing game core...\n");
	Tesselator::CreateNewThreadStorage(1024 * 1024);
	AABB::CreateNewThreadStorage();
	Vec3::CreateNewThreadStorage();
	IntCache::CreateNewThreadStorage();
	Compression::CreateNewThreadStorage();
	OldChunkStorage::CreateNewThreadStorage();
	Level::enableLightingCache();
	Tile::CreateNewThreadStorage();

	Minecraft::main();
	printf("Starting hosted world...\n");
	printf("Initializing hosted game settings...\n");
	app.InitGameSettings();
	MinecraftServer::resetFlags();
	app.SetTutorialMode(false);
	app.SetCorruptSaveDeleted(false);

	printf("Opening hosted network session...\n");
	g_NetworkManager.HostGame(0, true, false, MINECRAFT_NET_MAX_PLAYERS, 0);
	printf("Launching hosted world...\n");
	StartHostedWorld();

	printf("Dedicated server started. Press Ctrl+C to stop.\n");
	while (InterlockedCompareExchange(&g_shutdownRequested, FALSE, FALSE) == FALSE && !MinecraftServer::serverHalted())
	{
		g_NetworkManager.DoWork();
		ProfileManager.Tick();
		StorageManager.Tick();
		Sleep(10);
	}

	MinecraftServer* server = MinecraftServer::getInstance();
	if (server != NULL)
	{
		server->setSaveOnExit(true);
	}
	MinecraftServer::HaltServer();
	while (g_NetworkManager.ServerStoppedValid())
	{
		g_NetworkManager.ServerStoppedWait();
		break;
	}
	g_NetworkManager.Terminate();
	return 0;
}
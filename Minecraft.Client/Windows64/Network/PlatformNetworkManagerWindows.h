#pragma once

#include <winsock2.h>
#include <mutex>
#include <vector>

#include "..\..\Common\Network\PlatformNetworkManagerInterface.h"
#include "..\..\Common\Network\SessionInfo.h"
#include "NetworkPlayerWindows.h"

class CPlatformNetworkManagerWindows : public CPlatformNetworkManager {
	friend class CGameNetworkManager;
public:
	CPlatformNetworkManagerWindows();
	virtual ~CPlatformNetworkManagerWindows();

	virtual bool Initialise(CGameNetworkManager* pGameNetworkManager, int flagIndexSize);
	virtual void Terminate();
	virtual int GetJoiningReadyPercentage();
	virtual int CorrectErrorIDS(int IDS);
	virtual void DoWork();
	virtual int GetPlayerCount();
	virtual int GetOnlinePlayerCount();
	virtual int GetLocalPlayerMask(int playerIndex);
	virtual bool AddLocalPlayerByUserIndex(int userIndex);
	virtual bool RemoveLocalPlayerByUserIndex(int userIndex);
	virtual INetworkPlayer* GetLocalPlayerByUserIndex(int userIndex);
	virtual INetworkPlayer* GetPlayerByIndex(int playerIndex);
	virtual INetworkPlayer* GetPlayerByXuid(PlayerUID xuid);
	virtual INetworkPlayer* GetPlayerBySmallId(unsigned char smallId);
	virtual bool ShouldMessageForFullSession();
	virtual INetworkPlayer* GetHostPlayer();
	virtual bool IsHost();
	virtual bool JoinGameFromInviteInfo(int userIndex, int userMask, const INVITE_INFO* pInviteInfo);
	virtual bool LeaveGame(bool bMigrateHost);
	virtual bool IsInSession();
	virtual bool IsInGameplay();
	virtual bool IsReadyToPlayOrIdle();
	virtual bool IsInStatsEnabledSession();
	virtual bool SessionHasSpace(unsigned int spaceRequired = 1);
	virtual void SendInviteGUI(int quadrant);
	virtual bool IsAddingPlayer();
	virtual void HostGame(int localUsersMask, bool bOnlineGame, bool bIsPrivate, unsigned char publicSlots = MINECRAFT_NET_MAX_PLAYERS, unsigned char privateSlots = 0);
	virtual int JoinGame(FriendSessionInfo* searchResult, int localUsersMask, int primaryUserIndex);
	virtual bool SetLocalGame(bool isLocal);
	virtual bool IsLocalGame();
	virtual void SetPrivateGame(bool isPrivate);
	virtual bool IsPrivateGame();
	virtual bool IsLeavingGame();
	virtual void ResetLeavingGame();
	virtual void RegisterPlayerChangedCallback(int iPad, void (*callback)(void* callbackParam, INetworkPlayer* pPlayer, bool leaving), void* callbackParam);
	virtual void UnRegisterPlayerChangedCallback(int iPad, void (*callback)(void* callbackParam, INetworkPlayer* pPlayer, bool leaving), void* callbackParam);
	virtual void HandleSignInChange();
	virtual bool _RunNetworkGame();
	virtual void UpdateAndSetGameSessionData(INetworkPlayer* pNetworkPlayerLeaving = NULL);
	virtual void SystemFlagSet(INetworkPlayer* pNetworkPlayer, int index);
	virtual bool SystemFlagGet(INetworkPlayer* pNetworkPlayer, int index);
	virtual std::wstring GatherStats();
	virtual std::wstring GatherRTTStats();
	virtual std::vector<FriendSessionInfo*>* GetSessionList(int iPad, int localPlayers, bool partyOnly);
	virtual bool GetGameSessionInfo(int iPad, SessionID sessionId, FriendSessionInfo* foundSession);
	virtual void SetSessionsUpdatedCallback(void (*SessionsUpdatedCallback)(LPVOID pParam), LPVOID pSearchParam);
	virtual void GetFullFriendSessionInfo(FriendSessionInfo* foundSession, void (*FriendSessionUpdatedFn)(bool success, void* pParam), void* pParam);
	virtual void ForceFriendsSessionRefresh();
	virtual void FakeLocalPlayerJoined();

private:
	struct PendingClient {
		SOCKET socket;
		unsigned long remoteAddress;
		DWORD acceptedAt;
	};

	virtual bool _LeaveGame(bool bMigrateHost, bool bLeaveRoom);
	virtual void _HostGame(int usersMask, unsigned char publicSlots = MINECRAFT_NET_MAX_PLAYERS, unsigned char privateSlots = 0);
	virtual bool _StartGame();
	virtual bool RemoveLocalPlayer(INetworkPlayer* pNetworkPlayer);
	virtual void SetSessionTexturePackParentId(int id);
	virtual void SetSessionSubTexturePackId(int id);
	virtual void Notify(int ID, ULONG_PTR Param);

	bool StartListener();
	void CloseListener();
	void ProcessDiscoveryRequests();
	void ProcessPendingClients();
	NetworkPlayerWindows* CreateLocalPlayer(int userIndex);
	NetworkPlayerWindows* AddRemotePlayer(SOCKET socket, unsigned long remoteAddress, bool isHost);
	void RemoveRemotePlayer(NetworkPlayerWindows* player);
	void NotifyPlayerChanged(NetworkPlayerWindows* player, bool leaving);
	void SetSessionLabel(FriendSessionInfo* sessionInfo, const wchar_t* label, SessionID sessionId);

	CGameNetworkManager* m_gameNetworkManager;
	SOCKET m_listenSocket;
	SOCKET m_discoverySocket;
	bool m_winsockStarted;
	bool m_isHost;
	bool m_inSession;
	bool m_inGameplay;
	bool m_isLocalGame;
	bool m_isPrivateGame;
	bool m_isLeaving;
	unsigned char m_nextSmallId;
	int m_flagIndexSize;
	GameSessionData m_sessionData;
	NetworkPlayerWindows* m_localPlayers[XUSER_MAX_COUNT];
	NetworkPlayerWindows* m_hostPlayer;
	std::vector<NetworkPlayerWindows*> m_players;
	std::vector<PendingClient> m_pendingClients;
	std::mutex m_playersMutex;
	void (*m_playerChangedCallback[XUSER_MAX_COUNT])(void* callbackParam, INetworkPlayer* pPlayer, bool leaving);
	void* m_playerChangedCallbackParam[XUSER_MAX_COUNT];
	void (*m_sessionsUpdatedCallback)(LPVOID pParam);
	LPVOID m_searchParam;
};
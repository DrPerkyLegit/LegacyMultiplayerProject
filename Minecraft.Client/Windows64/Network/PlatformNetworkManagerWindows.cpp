#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#include "PlatformNetworkManagerWindows.h"
#include "..\..\Common\Network\GameNetworkManager.h"
#include "..\..\Minecraft.h"
#include "..\..\User.h"
#include "..\..\..\Minecraft.World\StringHelpers.h"
#include "..\..\..\Minecraft.World\Socket.h"
#include "..\..\Common\Network\ExternalServerList.h"

namespace {
	const unsigned short WINDOWS_NETWORK_PORT = 25565;
	const unsigned short WINDOWS_DISCOVERY_PORT = 25566;
	const SessionID LOCAL_SESSION_ID = 0x4C4F43414C;
	const SessionID LAN_SESSION_PREFIX = 0x4C41000000000000ULL;
	const SessionID LAN_SESSION_PREFIX_MASK = 0xFFFF000000000000ULL;
	const unsigned long LAN_DISCOVERY_MAGIC = 0x4C4D5043;
	const unsigned char LAN_DISCOVERY_QUERY = 1;
	const unsigned char LAN_DISCOVERY_RESPONSE = 2;
	const unsigned char TCP_REQUEST_SERVER_INFO = 1;
	const unsigned char TCP_REQUEST_JOIN_GAME = 2;
	const DWORD TCP_REQUEST_TIMEOUT_MS = 1000;
	const DWORD TCP_JOIN_TIMEOUT_MS = 1500;
	const DWORD PENDING_CLIENT_TIMEOUT_MS = 5000;

	bool SetNonBlocking(SOCKET socket) {
		u_long enabled = 1;
		return ioctlsocket(socket, FIONBIO, &enabled) == 0;
	}

	bool ConnectWithTimeout(SOCKET socket, const sockaddr_in& address, DWORD timeoutMs = 5000) {
		if (!SetNonBlocking(socket)) return false;
		if (connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0) return true;

		int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS && error != WSAEALREADY) return false;

		fd_set writableSockets;
		FD_ZERO(&writableSockets);
		FD_SET(socket, &writableSockets);
		timeval timeout;
		timeout.tv_sec = timeoutMs / 1000;
		timeout.tv_usec = (timeoutMs % 1000) * 1000;
		int ready = select(0, nullptr, &writableSockets, nullptr, &timeout);
		if (ready == 0) {
			WSASetLastError(WSAETIMEDOUT);
			return false;
		}

		if (ready == SOCKET_ERROR) return false;

		int socketError = 0;
		int errorSize = sizeof(socketError);
		if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &errorSize) == SOCKET_ERROR) return false;

		if (socketError != 0) {
			WSASetLastError(socketError);
			return false;
		}
		return true;
	}

#pragma pack(push, 1)
	struct LanDiscoveryPacket {
		unsigned long magic;
		unsigned char type;
	};

	struct ServerDataPacket {
		unsigned long magic;
		unsigned char type;
		unsigned char playerCount;
		unsigned char maxPlayers;
		unsigned char ready;
		unsigned short netVersion;
		unsigned short gamePort;
		unsigned long hostSettings;
		unsigned long texturePackParentId;
		unsigned char subTexturePackId;
		wchar_t displayName[32];
		wchar_t playerNames[MINECRAFT_NET_MAX_PLAYERS][XUSER_NAME_SIZE];
	};

	void ApplyServerData(FriendSessionInfo* sessionInfo, const ServerDataPacket& serverData) {
		sessionInfo->data.netVersion = serverData.netVersion;
		sessionInfo->data.m_uiGameHostSettings = serverData.hostSettings;
		sessionInfo->data.texturePackParentId = serverData.texturePackParentId;
		sessionInfo->data.subTexturePackId = serverData.subTexturePackId;
		sessionInfo->data.isReadyToJoin = serverData.ready != 0;
		sessionInfo->serverPlayerNames.clear();

		unsigned int playerCount = serverData.playerCount;
		if (playerCount > MINECRAFT_NET_MAX_PLAYERS) playerCount = MINECRAFT_NET_MAX_PLAYERS;
		for (unsigned int index = 0; index < playerCount; ++index) {
			size_t nameLength = 0;
			while (nameLength < XUSER_NAME_SIZE && serverData.playerNames[index][nameLength] != L'\0') {
				++nameLength;
			}
			if (nameLength > 0) {
				sessionInfo->serverPlayerNames.push_back(std::wstring(serverData.playerNames[index], nameLength));
			}
		}
	}

	bool WaitForSocket(SOCKET socket, bool writable, DWORD startTime, DWORD timeoutMs) {
		DWORD elapsed = GetTickCount() - startTime;
		if (elapsed >= timeoutMs) return false;

		fd_set sockets;
		FD_ZERO(&sockets);
		FD_SET(socket, &sockets);
		timeval timeout;
		timeout.tv_sec = (timeoutMs - elapsed) / 1000;
		timeout.tv_usec = ((timeoutMs - elapsed) % 1000) * 1000;
		int result = writable
			? select(0, nullptr, &sockets, nullptr, &timeout)
			: select(0, &sockets, nullptr, nullptr, &timeout);
		return result > 0;
	}

	bool SendAllWithTimeout(SOCKET socket, const char* data, int length, DWORD timeoutMs) {
		int sentTotal = 0;
		DWORD startTime = GetTickCount();
		while (sentTotal < length) {
			if (!WaitForSocket(socket, true, startTime, timeoutMs)) return false;
			int sent = send(socket, data + sentTotal, length - sentTotal, 0);
			if (sent > 0) {
				sentTotal += sent;
				continue;
			}
			int error = WSAGetLastError();
			if (sent == SOCKET_ERROR && (error == WSAEWOULDBLOCK || error == WSAEINTR)) continue;
			return false;
		}
		return true;
	}

	bool ReceiveAllWithTimeout(SOCKET socket, char* data, int length, DWORD timeoutMs) {
		int receivedTotal = 0;
		DWORD startTime = GetTickCount();
		while (receivedTotal < length) {
			if (!WaitForSocket(socket, false, startTime, timeoutMs)) return false;
			int received = recv(socket, data + receivedTotal, length - receivedTotal, 0);
			if (received > 0) {
				receivedTotal += received;
				continue;
			}
			int error = WSAGetLastError();
			if (received == SOCKET_ERROR && (error == WSAEWOULDBLOCK || error == WSAEINTR)) continue;
			return false;
		}
		return true;
	}

	bool QueryServerData(const sockaddr_in& address, ServerDataPacket& serverData) {
		SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket == INVALID_SOCKET) return false;
		if (!ConnectWithTimeout(socket, address, TCP_REQUEST_TIMEOUT_MS)) {
			closesocket(socket);
			return false;
		}

		const char request = TCP_REQUEST_SERVER_INFO;
		bool success = SendAllWithTimeout(socket, &request, sizeof(request), TCP_REQUEST_TIMEOUT_MS) &&
			ReceiveAllWithTimeout(socket, reinterpret_cast<char*>(&serverData), sizeof(serverData), TCP_REQUEST_TIMEOUT_MS);
		closesocket(socket);
		if (!success || serverData.magic != LAN_DISCOVERY_MAGIC || serverData.type != TCP_REQUEST_SERVER_INFO)
			return false;

		serverData.displayName[_countof(serverData.displayName) - 1] = L'\0';
		return true;
	}

#pragma pack(pop)

	SessionID MakeLanSessionId(unsigned long address, unsigned short port) {
		return LAN_SESSION_PREFIX | (static_cast<SessionID>(ntohl(address)) << 16) | static_cast<SessionID>(port);
	}

	bool IsLanSessionId(SessionID sessionId) {
		return (sessionId & LAN_SESSION_PREFIX_MASK) == LAN_SESSION_PREFIX;
	}

	bool GetLanAddress(SessionID sessionId, sockaddr_in* address) {
		if (address == nullptr || !IsLanSessionId(sessionId)) return false;

		unsigned long hostAddress = static_cast<unsigned long>((sessionId >> 16) & 0xFFFFFFFFULL);
		unsigned short port = static_cast<unsigned short>(sessionId & 0xFFFF);
		if (hostAddress == 0 || port == 0) return false;

		memset(address, 0, sizeof(*address));
		address->sin_family = AF_INET;
		address->sin_addr.s_addr = htonl(hostAddress);
		address->sin_port = htons(port);
		return true;
	}
}

CPlatformNetworkManagerWindows::CPlatformNetworkManagerWindows() {
	m_gameNetworkManager = nullptr;
	m_hostPlayer = nullptr;
	m_sessionsUpdatedCallback = nullptr;
	m_searchParam = nullptr;

	m_winsockStarted = false;
	m_isHost = false;
	m_inSession = false;
	m_inGameplay = false;
	m_isLocalGame = true;
	m_isPrivateGame = false;
	m_isLeaving = false;

	m_nextSmallId = 0;
	m_flagIndexSize = 0;

	m_listenSocket = INVALID_SOCKET;
	m_discoverySocket = INVALID_SOCKET;


	memset(m_localPlayers, 0, sizeof(m_localPlayers));
	memset(m_playerChangedCallback, 0, sizeof(m_playerChangedCallback));
	memset(m_playerChangedCallbackParam, 0, sizeof(m_playerChangedCallbackParam));
}

CPlatformNetworkManagerWindows::~CPlatformNetworkManagerWindows() {
	Terminate();
}

bool CPlatformNetworkManagerWindows::Initialise(CGameNetworkManager* pGameNetworkManager, int flagIndexSize) {
	m_gameNetworkManager = pGameNetworkManager;
	m_flagIndexSize = flagIndexSize;

	WSADATA data;
	int result = WSAStartup(MAKEWORD(2, 2), &data);
	if (result != 0) {
		app.DebugPrintf("Winsock startup failed: %d", result);
		return false;
	}

	m_winsockStarted = true;
	return true;
}

void CPlatformNetworkManagerWindows::Terminate() {
	CloseListener();

	std::vector<NetworkPlayerWindows*> players;
	bool hostIsServer = m_isHost;
	{
		std::lock_guard<std::mutex> lock(m_playersMutex);
		players.swap(m_players);
		memset(m_localPlayers, 0, sizeof(m_localPlayers));
		m_hostPlayer = nullptr;
		m_inSession = false;
		m_inGameplay = false;
		m_isHost = false;
	}

	for (std::vector<NetworkPlayerWindows*>::iterator it = players.begin(); it != players.end(); ++it) {
		(*it)->CloseTransport(hostIsServer);
		delete* it;
	}

	if (m_winsockStarted) {
		WSACleanup();
		m_winsockStarted = false;
	}
}

int CPlatformNetworkManagerWindows::GetJoiningReadyPercentage() { return 100; }
int CPlatformNetworkManagerWindows::CorrectErrorIDS(int IDS) { return IDS; }

bool CPlatformNetworkManagerWindows::StartListener() {
	if (m_listenSocket != INVALID_SOCKET && m_discoverySocket != INVALID_SOCKET) return true;
	CloseListener();

	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET) {
		app.DebugPrintf("Creating the Windows game listener failed: %d", WSAGetLastError());
		return false;
	}

	sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(WINDOWS_NETWORK_PORT);

	if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR || listen(listener, SOMAXCONN) == SOCKET_ERROR || !SetNonBlocking(listener)) {
		app.DebugPrintf("Starting the Windows game listener failed: %d", WSAGetLastError());
		closesocket(listener);
		return false;
	}

	SOCKET discovery = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (discovery == INVALID_SOCKET) {
		app.DebugPrintf("Creating the Windows LAN discovery socket failed: %d", WSAGetLastError());
		closesocket(listener);
		return false;
	}

	sockaddr_in discoveryAddress;
	memset(&discoveryAddress, 0, sizeof(discoveryAddress));
	discoveryAddress.sin_family = AF_INET;
	discoveryAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	discoveryAddress.sin_port = htons(WINDOWS_DISCOVERY_PORT);
	if (::bind(discovery, reinterpret_cast<sockaddr*>(&discoveryAddress), sizeof(discoveryAddress)) == SOCKET_ERROR || !SetNonBlocking(discovery)) {
		app.DebugPrintf("Starting the Windows LAN discovery listener failed: %d", WSAGetLastError());
		closesocket(discovery);
		closesocket(listener);
		return false;
	}

	m_listenSocket = listener;
	m_discoverySocket = discovery;
	app.DebugPrintf("Windows LAN host listening on TCP %u and UDP %u.", WINDOWS_NETWORK_PORT, WINDOWS_DISCOVERY_PORT);
	return true;
}

void CPlatformNetworkManagerWindows::CloseListener() {
	SOCKET listener = m_listenSocket;
	SOCKET discovery = m_discoverySocket;
	m_listenSocket = INVALID_SOCKET;
	m_discoverySocket = INVALID_SOCKET;

	if (listener != INVALID_SOCKET) closesocket(listener);
	if (discovery != INVALID_SOCKET) closesocket(discovery);
	for (std::vector<PendingClient>::iterator it = m_pendingClients.begin(); it != m_pendingClients.end(); ++it) {
		closesocket(it->socket);
	}
	m_pendingClients.clear();
}

void CPlatformNetworkManagerWindows::ProcessDiscoveryRequests() {
	while (true) {
		LanDiscoveryPacket request = {};
		sockaddr_in remoteAddress;
		int addressLength = sizeof(remoteAddress);
		int received = recvfrom(m_discoverySocket, reinterpret_cast<char*>(&request), sizeof(request), 0, reinterpret_cast<sockaddr*>(&remoteAddress), &addressLength);
		if (received == SOCKET_ERROR) {
			int error = WSAGetLastError();
			if (error != WSAEWOULDBLOCK && error != WSAEINTR) {
				app.DebugPrintf("Receiving a Windows LAN discovery request failed: %d", error);
			}
			break;
		}

		if (received != sizeof(request) || request.magic != LAN_DISCOVERY_MAGIC || request.type != LAN_DISCOVERY_QUERY || !m_isHost || !m_inSession) continue;
		app.DebugPrintf("Received a Windows LAN discovery query.");

		LanDiscoveryPacket response = {};
		response.magic = LAN_DISCOVERY_MAGIC;
		response.type = LAN_DISCOVERY_RESPONSE;

		int sent = sendto(m_discoverySocket, reinterpret_cast<const char*>(&response), sizeof(response), 0, reinterpret_cast<sockaddr*>(&remoteAddress), addressLength);
		if (sent == SOCKET_ERROR) {
			app.DebugPrintf("Sending a Windows LAN discovery response failed: %d", WSAGetLastError());
		}
	}
}

void CPlatformNetworkManagerWindows::ProcessPendingClients() {
	for (std::vector<PendingClient>::iterator it = m_pendingClients.begin(); it != m_pendingClients.end();) {
		char request = 0;
		int received = recv(it->socket, &request, sizeof(request), 0);
		if (received == SOCKET_ERROR) {
			int error = WSAGetLastError();
			if ((error == WSAEWOULDBLOCK || error == WSAEINTR) &&
				GetTickCount() - it->acceptedAt < PENDING_CLIENT_TIMEOUT_MS) {
				++it;
				continue;
			}
			if (error != WSAEWOULDBLOCK && error != WSAEINTR) {
				app.DebugPrintf("Receiving a Windows game connection request failed: %d", error);
			}
			closesocket(it->socket);
			it = m_pendingClients.erase(it);
			continue;
		}

		if (received == 0) {
			closesocket(it->socket);
			it = m_pendingClients.erase(it);
			continue;
		}

		PendingClient pending = *it;
		it = m_pendingClients.erase(it);
		if (request == TCP_REQUEST_SERVER_INFO && received == sizeof(request)) {
			ServerDataPacket response = {};
			response.magic = LAN_DISCOVERY_MAGIC;
			response.type = TCP_REQUEST_SERVER_INFO;
			response.maxPlayers = static_cast<unsigned char>(MINECRAFT_NET_MAX_PLAYERS);
			response.ready = m_inSession ? 1 : 0;
			response.netVersion = MINECRAFT_NET_VERSION;
			response.gamePort = WINDOWS_NETWORK_PORT;
			response.hostSettings = app.GetGameHostOption(eGameHostOption_All);
			response.texturePackParentId = m_sessionData.texturePackParentId;
			response.subTexturePackId = m_sessionData.subTexturePackId;

			{
				std::lock_guard<std::mutex> lock(m_playersMutex);
				for (std::vector<NetworkPlayerWindows*>::const_iterator it = m_players.begin();
					it != m_players.end() && response.playerCount < MINECRAFT_NET_MAX_PLAYERS; ++it) {
					if (!(*it)->IsConnected()) continue;
					const wchar_t* playerName = (*it)->GetOnlineName();
					if (playerName != nullptr) {
						wcsncpy_s(response.playerNames[response.playerCount], XUSER_NAME_SIZE, playerName, _TRUNCATE);
					}
					++response.playerCount;
				}
			}

			const wchar_t* displayName = m_hostPlayer != nullptr ? m_hostPlayer->GetOnlineName() : nullptr;
			if (displayName == nullptr || displayName[0] == L'\0') displayName = L"LAN Game";
			wcsncpy_s(response.displayName, _countof(response.displayName), displayName, _TRUNCATE);

			if (!SendAllWithTimeout(pending.socket, reinterpret_cast<const char*>(&response), sizeof(response), TCP_REQUEST_TIMEOUT_MS)) {
				app.DebugPrintf("Sending Windows server data failed.");
			}
			closesocket(pending.socket);
		} else if (request == TCP_REQUEST_JOIN_GAME && received == sizeof(request)) {
			if (SessionHasSpace()) {
				AddRemotePlayer(pending.socket, pending.remoteAddress, false);
			} else {
				closesocket(pending.socket);
			}
		} else {
			closesocket(pending.socket);
		}
	}
}

void CPlatformNetworkManagerWindows::DoWork() {
	if (!m_winsockStarted) return;

	if (m_isHost && m_listenSocket != INVALID_SOCKET) {
		if (m_discoverySocket != INVALID_SOCKET) ProcessDiscoveryRequests();
		ProcessPendingClients();

		while (true) {
			sockaddr_in remoteAddress;
			int addressLength = sizeof(remoteAddress);
			SOCKET client = accept(m_listenSocket, reinterpret_cast<sockaddr*>(&remoteAddress), &addressLength);
			if (client == INVALID_SOCKET) {
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK && error != WSAEINTR) {
					app.DebugPrintf("Accepting a local game connection failed: %d", error);
				}
				break;
			}

			if (!SetNonBlocking(client)) {
				closesocket(client);
				continue;
			}

			BOOL noDelay = TRUE;
			setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

			if (m_pendingClients.size() >= 32) {
				closesocket(client);
				continue;
			}
			PendingClient pending = { client, remoteAddress.sin_addr.s_addr, GetTickCount() };
			m_pendingClients.push_back(pending);
		}
		ProcessPendingClients();
	}

	std::vector<NetworkPlayerWindows*> players;
	{
		std::lock_guard<std::mutex> lock(m_playersMutex);
		players = m_players;
	}

	for (std::vector<NetworkPlayerWindows*>::iterator it = players.begin(); it != players.end(); ++it) {
		NetworkPlayerWindows* player = *it;
		if (!player->IsConnected() || player->GetTransportSocket() == INVALID_SOCKET) continue;

		if (!player->ReceivePendingData(m_isHost) || !player->FlushSendQueue()) RemoveRemotePlayer(player);
	}
}

int CPlatformNetworkManagerWindows::GetPlayerCount() {
	int count = 0;
	std::lock_guard<std::mutex> lock(m_playersMutex);
	for (std::vector<NetworkPlayerWindows*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->IsConnected()) ++count;
	}
	return count;
}

int CPlatformNetworkManagerWindows::GetOnlinePlayerCount() {
	int count = 0;
	std::lock_guard<std::mutex> lock(m_playersMutex);
	for (std::vector<NetworkPlayerWindows*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->IsConnected() && !(*it)->IsLocal()) ++count;
	}
	return count;
}

int CPlatformNetworkManagerWindows::GetLocalPlayerMask(int playerIndex) {
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return 0;
	return 1 << playerIndex;
}

NetworkPlayerWindows* CPlatformNetworkManagerWindows::CreateLocalPlayer(int userIndex) {
	if (userIndex < 0 || userIndex >= XUSER_MAX_COUNT) return nullptr;
	if (m_localPlayers[userIndex] != nullptr) {
		NetworkPlayerWindows* player = m_localPlayers[userIndex];
		if (!player->IsConnected()) {
			player->ResetForSession(m_isHost, m_isHost ? 0 : static_cast<unsigned char>(userIndex + 1));
		} else {
			player->SetHost(m_isHost);
			player->SetSmallId(m_isHost ? 0 : static_cast<unsigned char>(userIndex + 1));
		}
		return player;
	}

	std::wstring name = L"Player";
	Minecraft* minecraft = Minecraft::GetInstance();
	if (minecraft != nullptr && minecraft->user != nullptr && !minecraft->user->name.empty()) {
		name = minecraft->user->name;
	}

	unsigned char smallId = m_isHost ? 0 : static_cast<unsigned char>(userIndex + 1);
	NetworkPlayerWindows* player = new NetworkPlayerWindows(userIndex, smallId, true, m_isHost, name);
	m_localPlayers[userIndex] = player;
	{
		std::lock_guard<std::mutex> lock(m_playersMutex);
		player->SetSessionIndex(static_cast<int>(m_players.size()));
		m_players.push_back(player);
	}
	m_gameNetworkManager->PlayerJoining(player);
	NotifyPlayerChanged(player, false);
	return player;
}

bool CPlatformNetworkManagerWindows::AddLocalPlayerByUserIndex(int userIndex) {
	return CreateLocalPlayer(userIndex) != nullptr;
}

bool CPlatformNetworkManagerWindows::RemoveLocalPlayerByUserIndex(int userIndex) {
	if (userIndex < 0 || userIndex >= XUSER_MAX_COUNT || m_localPlayers[userIndex] == nullptr) return false;
	return RemoveLocalPlayer(m_localPlayers[userIndex]);
}

INetworkPlayer* CPlatformNetworkManagerWindows::GetLocalPlayerByUserIndex(int userIndex) {
	if (userIndex < 0 || userIndex >= XUSER_MAX_COUNT) return nullptr;
	return m_localPlayers[userIndex];
}

INetworkPlayer* CPlatformNetworkManagerWindows::GetPlayerByIndex(int playerIndex) {
	if (playerIndex < 0) return nullptr;
	std::lock_guard<std::mutex> lock(m_playersMutex);
	int activeIndex = 0;
	for (std::vector<NetworkPlayerWindows*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if (!(*it)->IsConnected()) continue;
		if (activeIndex++ == playerIndex) return *it;
	}
	return nullptr;
}

INetworkPlayer* CPlatformNetworkManagerWindows::GetPlayerByXuid(PlayerUID xuid) {
	std::lock_guard<std::mutex> lock(m_playersMutex);
	for (std::vector<NetworkPlayerWindows*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->IsConnected() && (*it)->GetUID() == xuid) return *it;
	}
	return nullptr;
}

INetworkPlayer* CPlatformNetworkManagerWindows::GetPlayerBySmallId(unsigned char smallId) {
	std::lock_guard<std::mutex> lock(m_playersMutex);
	for (std::vector<NetworkPlayerWindows*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it)->IsConnected() && (*it)->GetSmallId() == smallId) return *it;
	}
	return nullptr;
}

bool CPlatformNetworkManagerWindows::ShouldMessageForFullSession() { return !SessionHasSpace(); }

INetworkPlayer* CPlatformNetworkManagerWindows::GetHostPlayer() {
	return m_hostPlayer != nullptr && m_hostPlayer->IsConnected() ? m_hostPlayer : nullptr;
}

bool CPlatformNetworkManagerWindows::IsHost() { return m_isHost && !m_isLeaving; }
bool CPlatformNetworkManagerWindows::JoinGameFromInviteInfo(int userIndex, int userMask, const INVITE_INFO* pInviteInfo) { return false; }

bool CPlatformNetworkManagerWindows::LeaveGame(bool bMigrateHost) {
	m_isLeaving = true;
	CloseListener();

	std::vector<NetworkPlayerWindows*> players;
	{
		std::lock_guard<std::mutex> lock(m_playersMutex);
		players = m_players;
	}
	for (std::vector<NetworkPlayerWindows*>::iterator it = players.begin(); it != players.end(); ++it) {
		NetworkPlayerWindows* player = *it;
		player->CloseTransport(m_isHost);
	}

	m_hostPlayer = nullptr;
	m_inSession = false;
	m_inGameplay = false;
	m_isHost = false;
	return true;
}

bool CPlatformNetworkManagerWindows::_LeaveGame(bool bMigrateHost, bool bLeaveRoom) {
	return LeaveGame(bMigrateHost);
}

bool CPlatformNetworkManagerWindows::IsInSession() { return m_inSession; }
bool CPlatformNetworkManagerWindows::IsInGameplay() { return m_inGameplay; }

bool CPlatformNetworkManagerWindows::IsReadyToPlayOrIdle() { return true; }
bool CPlatformNetworkManagerWindows::IsInStatsEnabledSession() { return false; }

bool CPlatformNetworkManagerWindows::SessionHasSpace(unsigned int spaceRequired) {
	return GetPlayerCount() + spaceRequired <= MINECRAFT_NET_MAX_PLAYERS;
}

void CPlatformNetworkManagerWindows::SendInviteGUI(int quadrant) {}

bool CPlatformNetworkManagerWindows::IsAddingPlayer() { return false; }

void CPlatformNetworkManagerWindows::HostGame(int localUsersMask, bool bOnlineGame, bool bIsPrivate, unsigned char publicSlots, unsigned char privateSlots) {
	SetLocalGame(true);
	SetPrivateGame(bIsPrivate);

	m_isLeaving = false;
	m_isHost = true;
	m_nextSmallId = 1;
	if (!m_winsockStarted) {
		m_isHost = false;
		m_inSession = false;
		return;
	}
	if (!StartListener()) {
		m_isHost = false;
		m_inSession = false;
		app.DebugPrintf("Could not open the LAN game ports (TCP 25565 and UDP 25566)");
		return;
	}

	m_inSession = true;
	if (m_localPlayers[0] != nullptr) {
		m_hostPlayer = CreateLocalPlayer(0);
	}
	_HostGame(localUsersMask, publicSlots, privateSlots);
}

void CPlatformNetworkManagerWindows::_HostGame(int usersMask, unsigned char publicSlots, unsigned char privateSlots) {}

bool CPlatformNetworkManagerWindows::_StartGame() {
	m_inGameplay = true;
	return m_isHost ? m_listenSocket != INVALID_SOCKET : m_inSession;
}

int CPlatformNetworkManagerWindows::JoinGame(FriendSessionInfo* searchResult, int localUsersMask, int primaryUserIndex) {
	if (searchResult == nullptr || primaryUserIndex < 0 || primaryUserIndex >= XUSER_MAX_COUNT) return CGameNetworkManager::JOINGAME_FAIL_GENERAL;

	sockaddr_in address;
	if (searchResult->sessionId == LOCAL_SESSION_ID) {
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(WINDOWS_NETWORK_PORT);
	} else if (!GetLanAddress(searchResult->sessionId, &address)) {
		return CGameNetworkManager::JOINGAME_FAIL_GENERAL;
	}

	SetLocalGame(true);
	if (m_gameNetworkManager == nullptr || !m_winsockStarted || m_inSession) {
		return CGameNetworkManager::JOINGAME_FAIL_GENERAL;
	}

	SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client == INVALID_SOCKET) {
		return CGameNetworkManager::JOINGAME_FAIL_GENERAL;
	}

	if (!ConnectWithTimeout(client, address, TCP_JOIN_TIMEOUT_MS)) {
		int error = WSAGetLastError();
		app.DebugPrintf("Connecting to Windows game at %s:%u failed: %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port), error);
		closesocket(client);
		return CGameNetworkManager::JOINGAME_FAIL_GENERAL;
	}
	const char request = TCP_REQUEST_JOIN_GAME;
	if (!SendAllWithTimeout(client, &request, sizeof(request), TCP_REQUEST_TIMEOUT_MS)) {
		closesocket(client);
		return CGameNetworkManager::JOINGAME_FAIL_GENERAL;
	}
	BOOL noDelay = TRUE;
	setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

	m_gameNetworkManager->StateChange_AnyToJoining();
	NetworkPlayerWindows* localPlayer = CreateLocalPlayer(primaryUserIndex);
	if (localPlayer == nullptr) {
		closesocket(client);
		m_gameNetworkManager->StateChange_JoiningToIdle(JOIN_FAILED_NONSPECIFIC);
		return CGameNetworkManager::JOINGAME_FAIL_GENERAL;
	}
	localPlayer->SetHost(false);
	localPlayer->SetTransportSocket(client, address.sin_addr.s_addr);
	localPlayer->SetSocket(new Socket(localPlayer, false, false));

	std::wstring hostName = searchResult->displayLabel != nullptr ? searchResult->displayLabel : L"LAN Game";
	NetworkPlayerWindows* remoteHost = new NetworkPlayerWindows(-1, 0, false, true, hostName);
	remoteHost->SetSessionIndex(0);
	{
		std::lock_guard<std::mutex> lock(m_playersMutex);
		m_players.insert(m_players.begin(), remoteHost);
		for (unsigned int index = 0; index < m_players.size(); ++index) {
			m_players[index]->SetSessionIndex(static_cast<int>(index));
		}
	}

	m_hostPlayer = remoteHost;
	m_isHost = false;
	m_isLeaving = false;
	m_inSession = true;
	m_inGameplay = false;
	m_gameNetworkManager->PlayerJoining(remoteHost);
	NotifyPlayerChanged(remoteHost, false);
	m_gameNetworkManager->StateChange_AnyToStarting();
	return CGameNetworkManager::JOINGAME_SUCCESS;
}

bool CPlatformNetworkManagerWindows::SetLocalGame(bool isLocal) {
	m_isLocalGame = isLocal;
	return true;
}

bool CPlatformNetworkManagerWindows::IsLocalGame() { return m_isLocalGame; }
void CPlatformNetworkManagerWindows::SetPrivateGame(bool isPrivate) { m_isPrivateGame = isPrivate; }
bool CPlatformNetworkManagerWindows::IsPrivateGame() { return m_isPrivateGame; }

bool CPlatformNetworkManagerWindows::IsLeavingGame() { return m_isLeaving; }
void CPlatformNetworkManagerWindows::ResetLeavingGame() { m_isLeaving = false; }

void CPlatformNetworkManagerWindows::RegisterPlayerChangedCallback(int iPad, void (*callback)(void* callbackParam, INetworkPlayer* pPlayer, bool leaving), void* callbackParam) {
	if (iPad < 0 || iPad >= XUSER_MAX_COUNT) return;
	m_playerChangedCallback[iPad] = callback;
	m_playerChangedCallbackParam[iPad] = callbackParam;
}

void CPlatformNetworkManagerWindows::UnRegisterPlayerChangedCallback(int iPad, void (*callback)(void* callbackParam, INetworkPlayer* pPlayer, bool leaving), void* callbackParam) {
	if (iPad < 0 || iPad >= XUSER_MAX_COUNT) return;
	if (m_playerChangedCallback[iPad] == callback && m_playerChangedCallbackParam[iPad] == callbackParam) {
		m_playerChangedCallback[iPad] = nullptr;
		m_playerChangedCallbackParam[iPad] = nullptr;
	}
}

void CPlatformNetworkManagerWindows::NotifyPlayerChanged(NetworkPlayerWindows* player, bool leaving) {
	for (int index = 0; index < XUSER_MAX_COUNT; ++index) {
		if (m_playerChangedCallback[index] != nullptr) {
			m_playerChangedCallback[index](m_playerChangedCallbackParam[index], player, leaving);
		}
	}
}

void CPlatformNetworkManagerWindows::HandleSignInChange() {}

bool CPlatformNetworkManagerWindows::_RunNetworkGame() {
	if (!m_isHost || m_listenSocket == INVALID_SOCKET) return false;
	m_inGameplay = true;

	std::vector<NetworkPlayerWindows*> players;
	{
		std::lock_guard<std::mutex> lock(m_playersMutex);
		players = m_players;
	}

	for (std::vector<NetworkPlayerWindows*>::iterator it = players.begin(); it != players.end(); ++it) {
		Socket* socket = (*it)->GetSocket();
		if (!(*it)->IsLocal() && (*it)->IsConnected() && socket != nullptr) {
			Socket::addIncomingSocket(socket);
		}
	}
	return true;
}

void CPlatformNetworkManagerWindows::UpdateAndSetGameSessionData(INetworkPlayer* pNetworkPlayerLeaving) {
	m_sessionData.netVersion = MINECRAFT_NET_VERSION;
	m_sessionData.m_uiGameHostSettings = app.GetGameHostOption(eGameHostOption_All);
}

void CPlatformNetworkManagerWindows::SystemFlagSet(INetworkPlayer* pNetworkPlayer, int index) {
	NetworkPlayerWindows* player = dynamic_cast<NetworkPlayerWindows*>(pNetworkPlayer);
	if (player == nullptr || index < 0 || index >= m_flagIndexSize) return;

	size_t required = static_cast<size_t>(index / 8 + 1);
	if (player->m_systemFlags.size() < required) player->m_systemFlags.resize(required, 0);
	player->m_systemFlags[index / 8] |= static_cast<unsigned char>(128 >> (index % 8));
}

bool CPlatformNetworkManagerWindows::SystemFlagGet(INetworkPlayer* pNetworkPlayer, int index)
{
	NetworkPlayerWindows* player = dynamic_cast<NetworkPlayerWindows*>(pNetworkPlayer);
	return player != nullptr && index >= 0 && index < m_flagIndexSize && static_cast<size_t>(index / 8) < player->m_systemFlags.size() && (player->m_systemFlags[index / 8] & static_cast<unsigned char>(128 >> (index % 8))) != 0;
}

std::wstring CPlatformNetworkManagerWindows::GatherStats() { return L"Winsock TCP players: " + _toString(GetPlayerCount()); }

std::wstring CPlatformNetworkManagerWindows::GatherRTTStats() { return L"TCP RTT: unavailable"; }

void CPlatformNetworkManagerWindows::SetSessionLabel(FriendSessionInfo* sessionInfo, const wchar_t* label, SessionID sessionId) {
	if (sessionInfo == nullptr) return;
	if (label == nullptr) label = L"LAN Game";
	size_t labelLength = wcslen(label);
	delete[] sessionInfo->displayLabel;
	sessionInfo->displayLabel = new wchar_t[labelLength + 1];
	wcscpy_s(sessionInfo->displayLabel, labelLength + 1, label);
	sessionInfo->displayLabelLength = static_cast<unsigned char>(labelLength);
	sessionInfo->displayLabelViewableStartIndex = 0;
	sessionInfo->sessionId = sessionId;
	sessionInfo->data.netVersion = MINECRAFT_NET_VERSION;
	sessionInfo->data.isReadyToJoin = true;
}

std::vector<FriendSessionInfo*>* CPlatformNetworkManagerWindows::GetSessionList(int iPad, int localPlayers, bool partyOnly) {
	std::vector<FriendSessionInfo*>* sessions = new std::vector<FriendSessionInfo*>();
	std::vector<ExternalServerList::Entry> savedServers;
	if (!ExternalServerList::Read(savedServers)) {
		app.DebugPrintf("Failed to read external servers from servers.db.");
	} else {
		for (size_t index = 0; index < savedServers.size(); ++index) {
			uint64_t sessionId = 0;
			if (!ExternalServerList::MakeSessionId(savedServers[index].address, savedServers[index].port, sessionId)) {
				app.DebugPrintf("Ignoring invalid address in servers.db: %s", savedServers[index].address.c_str());
				continue;
			}

			int nameLength = MultiByteToWideChar(CP_ACP, 0, savedServers[index].name.c_str(), -1, nullptr, 0);
			if (nameLength <= 0) {
				app.DebugPrintf("Ignoring server with an invalid name in servers.db.");
				continue;
			}
			std::vector<wchar_t> wideName(nameLength);
			if (MultiByteToWideChar(CP_ACP, 0, savedServers[index].name.c_str(), -1, &wideName[0], nameLength) == 0) {
				app.DebugPrintf("Ignoring server with an invalid name in servers.db.");
				continue;
			}

			FriendSessionInfo* sessionInfo = new FriendSessionInfo();
			SetSessionLabel(sessionInfo, wideName.data(), sessionId);
			sessions->push_back(sessionInfo);
		}
	}

	if (!m_winsockStarted) return sessions;

	SOCKET querySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (querySocket == INVALID_SOCKET) {
		app.DebugPrintf("Creating a Windows LAN discovery query socket failed: %d", WSAGetLastError());
		return sessions;
	}

	BOOL allowBroadcast = TRUE;
	if (setsockopt(querySocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&allowBroadcast), sizeof(allowBroadcast)) == SOCKET_ERROR) {
		app.DebugPrintf("Enabling LAN discovery broadcasts failed: %d", WSAGetLastError());
		closesocket(querySocket);
		return sessions;
	}

	sockaddr_in localAddress;
	memset(&localAddress, 0, sizeof(localAddress));
	localAddress.sin_family = AF_INET;
	localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddress.sin_port = 0;
	if (::bind(querySocket, reinterpret_cast<sockaddr*>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR || !SetNonBlocking(querySocket)) {
		app.DebugPrintf("Preparing a Windows LAN discovery query socket failed: %d", WSAGetLastError());
		closesocket(querySocket);
		return sessions;
	}

	LanDiscoveryPacket query = {};
	query.magic = LAN_DISCOVERY_MAGIC;
	query.type = LAN_DISCOVERY_QUERY;

	sockaddr_in broadcastAddress;
	memset(&broadcastAddress, 0, sizeof(broadcastAddress));
	broadcastAddress.sin_family = AF_INET;
	broadcastAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	broadcastAddress.sin_port = htons(WINDOWS_DISCOVERY_PORT);
	bool sentQuery = false;
	if (sendto(querySocket, reinterpret_cast<const char*>(&query), sizeof(query), 0, reinterpret_cast<sockaddr*>(&broadcastAddress), sizeof(broadcastAddress)) != SOCKET_ERROR) {
		sentQuery = true;
	} else {
		app.DebugPrintf("Sending a Windows LAN discovery broadcast failed: %d", WSAGetLastError());
	}

	INTERFACE_INFO interfaces[32] = {};
	DWORD interfaceBytes = 0;
	if (WSAIoctl(querySocket, SIO_GET_INTERFACE_LIST, nullptr, 0, interfaces, sizeof(interfaces), &interfaceBytes, nullptr, nullptr) == SOCKET_ERROR) {
		app.DebugPrintf("Enumerating Windows LAN interfaces for discovery failed: %d", WSAGetLastError());
	} else {
		size_t interfaceCount = interfaceBytes / sizeof(INTERFACE_INFO);
		for (size_t index = 0; index < interfaceCount; ++index) {
			if ((interfaces[index].iiFlags & (IFF_UP | IFF_BROADCAST)) != (IFF_UP | IFF_BROADCAST)) continue;

			sockaddr_in interfaceBroadcast = *reinterpret_cast<const sockaddr_in*>(&interfaces[index].iiBroadcastAddress);
			if (interfaceBroadcast.sin_family != AF_INET || interfaceBroadcast.sin_addr.s_addr == htonl(INADDR_ANY) || interfaceBroadcast.sin_addr.s_addr == htonl(INADDR_BROADCAST)) continue;

			interfaceBroadcast.sin_port = htons(WINDOWS_DISCOVERY_PORT);
			if (sendto(querySocket, reinterpret_cast<const char*>(&query), sizeof(query), 0, reinterpret_cast<sockaddr*>(&interfaceBroadcast), sizeof(interfaceBroadcast)) != SOCKET_ERROR) {
				sentQuery = true;
			} else {
				app.DebugPrintf("Sending a directed Windows LAN discovery broadcast failed: %d", WSAGetLastError());
			}
		}
	}

	if (!sentQuery) {
		closesocket(querySocket);
		return sessions;
	}

	fd_set readableSockets;
	FD_ZERO(&readableSockets);
	FD_SET(querySocket, &readableSockets);
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;
	int ready = select(0, &readableSockets, nullptr, nullptr, &timeout);
	if (ready == SOCKET_ERROR) {
		app.DebugPrintf("Waiting for Windows LAN discovery responses failed: %d", WSAGetLastError());
		closesocket(querySocket);
		return sessions;
	}
	if (ready == 0) {
		sockaddr_in loopbackAddress;
		memset(&loopbackAddress, 0, sizeof(loopbackAddress));
		loopbackAddress.sin_family = AF_INET;
		loopbackAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		loopbackAddress.sin_port = htons(WINDOWS_DISCOVERY_PORT);
		if (sendto(querySocket, reinterpret_cast<const char*>(&query), sizeof(query), 0, reinterpret_cast<sockaddr*>(&loopbackAddress), sizeof(loopbackAddress)) != SOCKET_ERROR) {
			FD_ZERO(&readableSockets);
			FD_SET(querySocket, &readableSockets);
			timeout.tv_sec = 0;
			timeout.tv_usec = 200000;
			ready = select(0, &readableSockets, nullptr, nullptr, &timeout);
		}
	}

	if (ready > 0) {
		while (true) {
			LanDiscoveryPacket response = {};
			sockaddr_in remoteAddress;
			int addressLength = sizeof(remoteAddress);
			int received = recvfrom(querySocket, reinterpret_cast<char*>(&response), sizeof(response), 0, reinterpret_cast<sockaddr*>(&remoteAddress), &addressLength);
			if (received == SOCKET_ERROR) {
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK && error != WSAEINTR) {
					app.DebugPrintf("Receiving a Windows LAN discovery response failed: %d", error);
				}
				break;
			}

			if (received != sizeof(response) || response.magic != LAN_DISCOVERY_MAGIC || response.type != LAN_DISCOVERY_RESPONSE) continue;

			sockaddr_in serverAddress = remoteAddress;
			serverAddress.sin_port = htons(WINDOWS_NETWORK_PORT);
			ServerDataPacket serverData = {};
			if (!QueryServerData(serverAddress, serverData) ||
				serverData.netVersion != MINECRAFT_NET_VERSION ||
				serverData.ready == 0 || serverData.gamePort == 0 ||
				(serverData.maxPlayers != 0 && (serverData.playerCount + localPlayers) > serverData.maxPlayers)) {
				continue;
			}

			SessionID sessionId = MakeLanSessionId(remoteAddress.sin_addr.s_addr, serverData.gamePort);
			bool alreadyAdded = false;
			for (std::vector<FriendSessionInfo*>::const_iterator it = sessions->begin(); it != sessions->end(); ++it) {
				if ((*it)->sessionId == sessionId) {
					alreadyAdded = true;
					break;
				}
			}
			if (alreadyAdded) continue;

			const wchar_t* label = serverData.displayName[0] != L'\0' ? serverData.displayName : L"LAN Game";
			FriendSessionInfo* sessionInfo = new FriendSessionInfo();
			SetSessionLabel(sessionInfo, label, sessionId);
			ApplyServerData(sessionInfo, serverData);
			sessions->push_back(sessionInfo);
		}
	}

	closesocket(querySocket);
	app.DebugPrintf("Windows LAN discovery found %u session(s).", static_cast<unsigned int>(sessions->size()));
	return sessions;
}

bool CPlatformNetworkManagerWindows::GetGameSessionInfo(int iPad, SessionID sessionId, FriendSessionInfo* foundSession) {
	if (foundSession == nullptr) return false;
	if (sessionId == LOCAL_SESSION_ID) {
		SetSessionLabel(foundSession, L"Localhost", LOCAL_SESSION_ID);
		return true;
	}
	if (!IsLanSessionId(sessionId)) return false;

	foundSession->data.netVersion = MINECRAFT_NET_VERSION;
	foundSession->data.isReadyToJoin = true;
	return true;
}

void CPlatformNetworkManagerWindows::SetSessionsUpdatedCallback(void (*SessionsUpdatedCallback)(LPVOID pParam), LPVOID pSearchParam) {
	m_sessionsUpdatedCallback = SessionsUpdatedCallback;
	m_searchParam = pSearchParam;
}

void CPlatformNetworkManagerWindows::GetFullFriendSessionInfo(FriendSessionInfo* foundSession, void (*FriendSessionUpdatedFn)(bool success, void* pParam), void* pParam) {
	bool success = foundSession != nullptr && (foundSession->sessionId == LOCAL_SESSION_ID || IsLanSessionId(foundSession->sessionId));
	if (success && foundSession->sessionId != LOCAL_SESSION_ID) {
		sockaddr_in address;
		ServerDataPacket serverData = {};
		if (GetLanAddress(foundSession->sessionId, &address) && QueryServerData(address, serverData) &&
			serverData.netVersion == MINECRAFT_NET_VERSION && serverData.ready != 0 && serverData.gamePort != 0) {
			ApplyServerData(foundSession, serverData);
			if (serverData.displayName[0] != L'\0') {
				SetSessionLabel(foundSession, serverData.displayName, foundSession->sessionId);
			}
		}
	}
	if (FriendSessionUpdatedFn != nullptr) FriendSessionUpdatedFn(success, pParam);
}

void CPlatformNetworkManagerWindows::ForceFriendsSessionRefresh() {
	if (m_sessionsUpdatedCallback != nullptr) m_sessionsUpdatedCallback(m_searchParam);
}

void CPlatformNetworkManagerWindows::FakeLocalPlayerJoined() {
	NetworkPlayerWindows* localPlayer = CreateLocalPlayer(0);
	if (localPlayer != nullptr && m_isHost) {
		m_hostPlayer = localPlayer;
	}
}

NetworkPlayerWindows* CPlatformNetworkManagerWindows::AddRemotePlayer(SOCKET socket, unsigned long remoteAddress, bool isHost) {
	std::wstring name = isHost ? L"Local Host" : L"LAN Player";
	NetworkPlayerWindows* player = new NetworkPlayerWindows(-1, isHost ? 0 : m_nextSmallId++, false, isHost, name);
	player->SetTransportSocket(socket, remoteAddress);
	{
		std::lock_guard<std::mutex> lock(m_playersMutex);
		player->SetSessionIndex(static_cast<int>(m_players.size()));
		m_players.push_back(player);
	}

	if (!isHost) {
		m_gameNetworkManager->PlayerJoining(player);
		m_gameNetworkManager->CreateSocket(player, false);
	} else {
		m_hostPlayer = player;
		m_gameNetworkManager->PlayerJoining(player);
	}
	NotifyPlayerChanged(player, false);
	return player;
}

void CPlatformNetworkManagerWindows::RemoveRemotePlayer(NetworkPlayerWindows* player) {
	if (player == nullptr || !player->IsConnected()) return;

	if (m_isHost) {
		m_gameNetworkManager->CloseConnection(player);
	}
	player->CloseTransport(m_isHost);
	m_gameNetworkManager->PlayerLeaving(player);
	NotifyPlayerChanged(player, true);

	if (!m_isHost) {
		if (m_hostPlayer != nullptr && m_hostPlayer != player) {
			m_hostPlayer->CloseTransport(false);
		}
		m_hostPlayer = nullptr;
		m_inSession = false;
		m_inGameplay = false;
		m_isLeaving = true;
	}
}

bool CPlatformNetworkManagerWindows::RemoveLocalPlayer(INetworkPlayer* pNetworkPlayer) {
	NetworkPlayerWindows* player = dynamic_cast<NetworkPlayerWindows*>(pNetworkPlayer);
	if (player == nullptr || !player->IsLocal()) return false;

	int userIndex = player->GetUserIndex();
	if (userIndex < 0 || userIndex >= XUSER_MAX_COUNT) return false;

	player->CloseTransport(m_isHost);
	m_gameNetworkManager->PlayerLeaving(player);
	NotifyPlayerChanged(player, true);
	m_localPlayers[userIndex] = nullptr;
	if (player == m_hostPlayer) m_hostPlayer = nullptr;
	return true;
}

void CPlatformNetworkManagerWindows::SetSessionTexturePackParentId(int id) {
	m_sessionData.texturePackParentId = id;
}

void CPlatformNetworkManagerWindows::SetSessionSubTexturePackId(int id) {
	m_sessionData.subTexturePackId = static_cast<unsigned char>(id);
}

void CPlatformNetworkManagerWindows::Notify(int ID, ULONG_PTR Param) {}
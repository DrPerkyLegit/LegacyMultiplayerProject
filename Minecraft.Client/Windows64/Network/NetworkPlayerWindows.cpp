#include "stdafx.h"
#include "NetworkPlayerWindows.h"
#include "..\..\..\Minecraft.World\Socket.h"

NetworkPlayerWindows::NetworkPlayerWindows(int userIndex, unsigned char smallId, bool isLocal, bool isHost, const std::wstring& name) {
	m_name = name;
	m_userIndex = userIndex;
	m_smallId = smallId;
	m_uid = ((static_cast<PlayerUID>(userIndex + 1) << 8) | smallId);
	
	m_isLocal = isLocal;
	m_isHost = isHost;

	m_sessionIndex = -1;
	m_remoteAddress = 0;
	m_sendQueueOffset = 0;
	m_sendQueueBytes = 0;
	m_gameSocket = nullptr;
	m_connected = true;
	m_transportSocket = INVALID_SOCKET;
}

NetworkPlayerWindows::~NetworkPlayerWindows() {
	CloseTransport(m_isHost);
}

unsigned char NetworkPlayerWindows::GetSmallId() { return m_smallId; }

NetworkPlayerWindows* NetworkPlayerWindows::GetTransportPlayer(INetworkPlayer* player) {
	if (m_isHost) {
		return dynamic_cast<NetworkPlayerWindows*>(player);
	}
	return this;
}

void NetworkPlayerWindows::SendData(INetworkPlayer* player, const void* pvData, int dataSize, bool lowPriority) {
	NetworkPlayerWindows* transportPlayer = GetTransportPlayer(player);
	if (transportPlayer == nullptr || pvData == nullptr || dataSize <= 0) return;

	if (transportPlayer->m_transportSocket.load() == INVALID_SOCKET || !transportPlayer->m_connected.load()) {
		app.DebugPrintf("Windows network send skipped: player has no active TCP connection.");
		return;
	}

	std::vector<char> message(dataSize);
	memcpy(message.data(), pvData, dataSize);

	std::lock_guard<std::mutex> lock(transportPlayer->m_sendQueueMutex);
	transportPlayer->m_sendQueueBytes += message.size();
	transportPlayer->m_sendQueue.push_back(std::move(message));
}

bool NetworkPlayerWindows::IsSameSystem(INetworkPlayer* player) {
	NetworkPlayerWindows* other = dynamic_cast<NetworkPlayerWindows*>(player);
	return other != nullptr && (this == other || m_remoteAddress == other->m_remoteAddress);
}

int NetworkPlayerWindows::GetQueueSizeBytes() {
	std::lock_guard<std::mutex> lock(m_sendQueueMutex);
	return static_cast<int>(std::min<size_t>(m_sendQueueBytes, INT_MAX));
}

int NetworkPlayerWindows::GetQueueSizeMessages() {
	std::lock_guard<std::mutex> lock(m_sendQueueMutex);
	return static_cast<int>(std::min<size_t>(m_sendQueue.size(), INT_MAX));
}

int NetworkPlayerWindows::GetSendQueueSizeBytes(INetworkPlayer* player, bool lowPriority) {
	NetworkPlayerWindows* transportPlayer = GetTransportPlayer(player);
	if (transportPlayer == nullptr) return 0;

	return transportPlayer->GetQueueSizeBytes();
}

int NetworkPlayerWindows::GetSendQueueSizeMessages(INetworkPlayer* player, bool lowPriority) {
	NetworkPlayerWindows* transportPlayer = GetTransportPlayer(player);
	if (transportPlayer == nullptr) return 0;

	return transportPlayer->GetQueueSizeMessages();
}

int NetworkPlayerWindows::GetCurrentRtt() { return 0; }

bool NetworkPlayerWindows::IsHost() { return m_isHost; }
bool NetworkPlayerWindows::IsGuest() { return false; }
bool NetworkPlayerWindows::IsLocal() { return m_isLocal; }
int NetworkPlayerWindows::GetSessionIndex() { return m_sessionIndex; }

bool NetworkPlayerWindows::IsTalking() { return false; }
bool NetworkPlayerWindows::IsMutedByLocalUser(int userIndex) { return false; }
bool NetworkPlayerWindows::HasVoice() { return false; }
bool NetworkPlayerWindows::HasCamera() { return false; }

int NetworkPlayerWindows::GetUserIndex() { return m_userIndex; }

void NetworkPlayerWindows::SetSocket(Socket* pSocket) { m_gameSocket = pSocket; }
Socket* NetworkPlayerWindows::GetSocket() { return m_gameSocket; }

const wchar_t* NetworkPlayerWindows::GetOnlineName() { return m_name.c_str(); }
std::wstring NetworkPlayerWindows::GetDisplayName() { return m_name; }

PlayerUID NetworkPlayerWindows::GetUID() { return m_uid; }

void NetworkPlayerWindows::SetHost(bool isHost) { m_isHost = isHost; }
void NetworkPlayerWindows::SetSmallId(unsigned char smallId) {
	m_smallId = smallId;
	m_uid = ((static_cast<PlayerUID>(m_userIndex + 1) << 8) | smallId);
}
void NetworkPlayerWindows::SetDisplayName(const std::wstring& name) { m_name = name; }

void NetworkPlayerWindows::SetSessionIndex(int index) { m_sessionIndex = index; }

void NetworkPlayerWindows::ResetForSession(bool isHost, unsigned char smallId) {
	m_isHost = isHost;
	SetSmallId(smallId);
	m_gameSocket = nullptr;
	m_remoteAddress = 0;
	m_connected.store(true);
	std::lock_guard<std::mutex> lock(m_sendQueueMutex);
	m_sendQueue.clear();
	m_sendQueueOffset = 0;
	m_sendQueueBytes = 0;
}

void NetworkPlayerWindows::SetTransportSocket(SOCKET socket, unsigned long remoteAddress) {
	std::lock_guard<std::mutex> lock(m_transportMutex);
	m_transportSocket.store(socket);
	m_remoteAddress = remoteAddress;
	m_connected.store(socket != INVALID_SOCKET);
}

SOCKET NetworkPlayerWindows::GetTransportSocket() const { return m_transportSocket.load();  }

bool NetworkPlayerWindows::IsConnected() const { return m_connected.load(); }

bool NetworkPlayerWindows::ReceivePendingData(bool hostIsServer) {
	char buffer[8192];
	while (true) {
		int bytesReceived;
		{
			std::lock_guard<std::mutex> lock(m_transportMutex);
			SOCKET socket = m_transportSocket.load();
			if (socket == INVALID_SOCKET || !m_connected.load()) {
				return false;
			}
			bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
		}

		if (bytesReceived > 0) {
			if (m_gameSocket == nullptr) {
				app.DebugPrintf("Windows network data received before the game socket was created.");
				return false;
			}
			m_gameSocket->pushDataToQueue(reinterpret_cast<const BYTE*>(buffer), bytesReceived, !hostIsServer);
			continue;
		}

		if (bytesReceived == 0) return false;

		int error = WSAGetLastError();
		if (error == WSAEWOULDBLOCK) return true;
		if (error == WSAEINTR) continue;

		return false;
	}
}

bool NetworkPlayerWindows::FlushSendQueue()
{
	std::lock_guard<std::mutex> transportLock(m_transportMutex);
	SOCKET socket = m_transportSocket.load();
	if (socket == INVALID_SOCKET || !m_connected.load()) return false;

	std::lock_guard<std::mutex> queueLock(m_sendQueueMutex);
	while (!m_sendQueue.empty()) {
		std::vector<char>& message = m_sendQueue.front();
		int remaining = static_cast<int>(message.size() - m_sendQueueOffset);
		int bytesSent = send(socket, message.data() + m_sendQueueOffset, remaining, 0);
		if (bytesSent > 0) {
			m_sendQueueOffset += bytesSent;
			m_sendQueueBytes -= bytesSent;
			if (m_sendQueueOffset == message.size()) {
				m_sendQueue.pop_front();
				m_sendQueueOffset = 0;
			}
			continue;
		}

		if (bytesSent == 0) return false;

		int error = WSAGetLastError();
		if (error == WSAEWOULDBLOCK) return true;
		if (error == WSAEINTR) continue;

		return false;
	}
	return true;
}

void NetworkPlayerWindows::CloseTransport(bool isServerConnection) {
	Socket* gameSocket = nullptr;
	{
		std::lock_guard<std::mutex> lock(m_transportMutex);
		SOCKET socket = m_transportSocket.exchange(INVALID_SOCKET);
		bool wasConnected = m_connected.exchange(false);
		if (socket != INVALID_SOCKET) {
			shutdown(socket, SD_BOTH);
			closesocket(socket);
		}

		if (wasConnected) {
			gameSocket = m_gameSocket;
			m_gameSocket = nullptr;
		}
	}

	if (gameSocket != nullptr) {
		InputStream* input = gameSocket->getInputStream(isServerConnection);
		Socket::SocketOutputStream* output = gameSocket->getOutputStream(isServerConnection);
		if (input != nullptr) input->close();
		if (output != nullptr) output->close();
		gameSocket->close(isServerConnection);
	}
}
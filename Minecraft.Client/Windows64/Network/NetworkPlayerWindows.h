#pragma once

#include <winsock2.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "..\..\Common\Network\NetworkPlayerInterface.h"

class Socket;

class NetworkPlayerWindows : public INetworkPlayer {
public:
	NetworkPlayerWindows(int userIndex, unsigned char smallId, bool isLocal, bool isHost, const std::wstring& name);
	virtual ~NetworkPlayerWindows();

	virtual unsigned char GetSmallId();
	virtual void SendData(INetworkPlayer* player, const void* pvData, int dataSize, bool lowPriority);
	virtual bool IsSameSystem(INetworkPlayer* player);
	virtual int GetSendQueueSizeBytes(INetworkPlayer* player, bool lowPriority);
	virtual int GetSendQueueSizeMessages(INetworkPlayer* player, bool lowPriority);
	virtual int GetCurrentRtt();
	virtual bool IsHost();
	virtual bool IsGuest();
	virtual bool IsLocal();
	virtual int GetSessionIndex();
	virtual bool IsTalking();
	virtual bool IsMutedByLocalUser(int userIndex);
	virtual bool HasVoice();
	virtual bool HasCamera();
	virtual int GetUserIndex();
	virtual void SetSocket(Socket* pSocket);
	virtual Socket* GetSocket();
	virtual const wchar_t* GetOnlineName();
	virtual std::wstring GetDisplayName();
	virtual PlayerUID GetUID();

	void SetHost(bool isHost);
	void SetSmallId(unsigned char smallId);
	void SetDisplayName(const std::wstring& name);
	void SetSessionIndex(int index);
	void ResetForSession(bool isHost, unsigned char smallId);
	void SetTransportSocket(SOCKET socket, unsigned long remoteAddress);
	SOCKET GetTransportSocket() const;
	bool IsConnected() const;
	bool ReceivePendingData(bool hostIsServer);
	bool FlushSendQueue();
	void CloseTransport(bool isServerConnection);

	std::vector<unsigned char> m_systemFlags;

private:
	NetworkPlayerWindows* GetTransportPlayer(INetworkPlayer* player);
	int GetQueueSizeBytes();
	int GetQueueSizeMessages();

	int m_userIndex;
	unsigned char m_smallId;
	bool m_isLocal;
	bool m_isHost;
	int m_sessionIndex;
	std::wstring m_name;
	PlayerUID m_uid;
	Socket* m_gameSocket;
	std::atomic<SOCKET> m_transportSocket;
	std::atomic<bool> m_connected;
	unsigned long m_remoteAddress;
	mutable std::mutex m_transportMutex;
	mutable std::mutex m_sendQueueMutex;
	std::deque<std::vector<char>> m_sendQueue;
	size_t m_sendQueueOffset;
	size_t m_sendQueueBytes;
};
#include "stdafx.h"
#include "NetworkPlayerStub.h"

NetworkPlayerStub::NetworkPlayerStub(IQNetPlayer * qnetPlayer)
{
	m_qnetPlayer = qnetPlayer;
	m_pSocket = NULL;
}

unsigned char NetworkPlayerStub::GetSmallId()
{
	return m_qnetPlayer->GetSmallId();
}

void NetworkPlayerStub::SendData(INetworkPlayer* player, const void* pvData, int dataSize, bool lowPriority)
{
	DWORD flags;
	flags = QNET_SENDDATA_RELIABLE | QNET_SENDDATA_SEQUENTIAL;
	if (lowPriority) flags |= QNET_SENDDATA_LOW_PRIORITY | QNET_SENDDATA_SECONDARY;
	m_qnetPlayer->SendData(((NetworkPlayerStub*)player)->m_qnetPlayer, pvData, dataSize, flags);
}

bool NetworkPlayerStub::IsSameSystem(INetworkPlayer* player)
{
	return (m_qnetPlayer->IsSameSystem(((NetworkPlayerStub*)player)->m_qnetPlayer) == TRUE);
}

int NetworkPlayerStub::GetSendQueueSizeBytes(INetworkPlayer* player, bool lowPriority)
{
	DWORD flags = QNET_GETSENDQUEUESIZE_BYTES;
	if (lowPriority) flags |= QNET_GETSENDQUEUESIZE_SECONDARY_TYPE;
	return m_qnetPlayer->GetSendQueueSize(player ? ((NetworkPlayerStub*)player)->m_qnetPlayer : NULL, flags);
}

int NetworkPlayerStub::GetSendQueueSizeMessages(INetworkPlayer* player, bool lowPriority)
{
	DWORD flags = QNET_GETSENDQUEUESIZE_MESSAGES;
	if (lowPriority) flags |= QNET_GETSENDQUEUESIZE_SECONDARY_TYPE;
	return m_qnetPlayer->GetSendQueueSize(player ? ((NetworkPlayerStub*)player)->m_qnetPlayer : NULL, flags);
}

int NetworkPlayerStub::GetCurrentRtt()
{
	return m_qnetPlayer->GetCurrentRtt();
}

bool NetworkPlayerStub::IsHost()
{
	return (m_qnetPlayer->IsHost() == TRUE);
}

bool NetworkPlayerStub::IsGuest()
{
	return (m_qnetPlayer->IsGuest() == TRUE);
}

bool NetworkPlayerStub::IsLocal()
{
	return (m_qnetPlayer->IsLocal() == TRUE);
}

int NetworkPlayerStub::GetSessionIndex()
{
	return m_qnetPlayer->GetSessionIndex();
}

bool NetworkPlayerStub::IsTalking()
{
	return (m_qnetPlayer->IsTalking() == TRUE);
}

bool NetworkPlayerStub::IsMutedByLocalUser(int userIndex)
{
	return (m_qnetPlayer->IsMutedByLocalUser(userIndex) == TRUE);
}

bool NetworkPlayerStub::HasVoice()
{
	return (m_qnetPlayer->HasVoice() == TRUE);
}

bool NetworkPlayerStub::HasCamera()
{
	return (m_qnetPlayer->HasCamera() == TRUE);
}

int NetworkPlayerStub::GetUserIndex()
{
	return m_qnetPlayer->GetUserIndex();
}

void NetworkPlayerStub::SetSocket(Socket* pSocket)
{
	m_pSocket = pSocket;
}

Socket* NetworkPlayerStub::GetSocket()
{
	return m_pSocket;
}

PlayerUID NetworkPlayerStub::GetUID()
{
	return m_qnetPlayer->GetXuid();
}

const wchar_t* NetworkPlayerStub::GetOnlineName()
{
	return m_qnetPlayer->GetGamertag();
}

std::wstring NetworkPlayerStub::GetDisplayName()
{
	return m_qnetPlayer->GetGamertag();
}

IQNetPlayer* NetworkPlayerStub::GetQNetPlayer()
{
	return m_qnetPlayer;
}

#include "stdafx.h"
#include "UI.h"
#include "UIScene_AddExternalServerMenu.h"
#include "..\Network\ExternalServerList.h"

UIScene_AddExternalServerMenu::UIScene_AddExternalServerMenu(int iPad, void *initData, UILayer *parentLayer) : UIScene(iPad, parentLayer)
{
	m_serverName = L"";
	m_serverIP = L"192.168.0.1";
	m_serverPort = L"25565";
	m_bIgnoreInput = false;
	m_keyboardTarget = eKeyboardTarget_ServerName;

	initialiseMovie();

	m_labelServerName.init(L"Server Name");
	m_editServerName.init(m_serverName, eControl_ServerName);
	m_editServerName.SetCharLimit(32);

	m_labelAddressName.init(L"Server Address");
	m_editServerAddress.init(m_serverIP, eControl_ServerAddress);
	m_editServerAddress.SetCharLimit(64);

	m_labelPortName.init(L"Server Port");
	m_editServerPort.init(m_serverPort, eControl_ServerPort);
	m_editServerPort.SetCharLimit(5);

	m_buttonJoinEntry.init(L"Join", eControl_JoinEntry);
	m_buttonSaveAndJoinEntry.init(L"Save And Join", eControl_SaveAndJoinEntry);
}

UIScene_AddExternalServerMenu::~UIScene_AddExternalServerMenu() {}

wstring UIScene_AddExternalServerMenu::getMoviePath()
{
	return L"AddExternalServerMenu";
}

UIControl *UIScene_AddExternalServerMenu::GetMainPanel()
{
	return &m_controlMainPanel;
}

void UIScene_AddExternalServerMenu::updateTooltips()
{
	ui.SetTooltips(m_iPad, IDS_TOOLTIPS_SELECT, IDS_TOOLTIPS_BACK);
}

void UIScene_AddExternalServerMenu::updateComponents()
{
	m_parentLayer->showComponent(m_iPad, eUIComponent_Panorama, true);
	m_parentLayer->showComponent(m_iPad, eUIComponent_Logo, false);
}

void UIScene_AddExternalServerMenu::handleGainFocus(bool navBack)
{
	UIScene::handleGainFocus(navBack);
	if (navBack)
		m_bIgnoreInput = false;
	else
		SetFocusToElement(eControl_ServerName);
}

void UIScene_AddExternalServerMenu::handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled)
{
	if (m_bIgnoreInput) return;

	ui.AnimateKeyPress(m_iPad, key, repeat, pressed, released);
	switch (key)
	{
	case ACTION_MENU_CANCEL:
		if (pressed)
		{
			navigateBack();
			handled = true;
		}
		break;
	case ACTION_MENU_OK:
	case ACTION_MENU_UP:
	case ACTION_MENU_DOWN:
	case ACTION_MENU_LEFT:
	case ACTION_MENU_RIGHT:
	case ACTION_MENU_OTHER_STICK_UP:
	case ACTION_MENU_OTHER_STICK_DOWN:
		sendInputToMovie(key, repeat, pressed, released);
		handled = true;
		break;
	}
}

void UIScene_AddExternalServerMenu::handlePress(F64 controlId, F64 childId)
{
	ui.PlayUISFX(eSFX_Press);

	switch (static_cast<int>(controlId))
	{
	case eControl_ServerName:
		m_bIgnoreInput = true;
		m_keyboardTarget = eKeyboardTarget_ServerName;
		RequestUIKeyboard(L"Server Name", m_editServerName.getLabel(), static_cast<DWORD>(m_iPad), 32,
			&UIScene_AddExternalServerMenu::KeyboardCompleteServerNameCallback, this, C_4JInput::EKeyboardMode_Default, &m_editServerName);
		break;
	case eControl_ServerAddress:
		m_bIgnoreInput = true;
		m_keyboardTarget = eKeyboardTarget_ServerAddress;
		RequestUIKeyboard(L"Server Address", m_editServerAddress.getLabel(), static_cast<DWORD>(m_iPad), 64,
			&UIScene_AddExternalServerMenu::KeyboardCompleteServerNameCallback, this, C_4JInput::EKeyboardMode_IP_Address, &m_editServerAddress);
		break;
	case eControl_ServerPort:
		m_bIgnoreInput = true;
		m_keyboardTarget = eKeyboardTarget_ServerPort;
		RequestUIKeyboard(L"Server Port", m_editServerPort.getLabel(), static_cast<DWORD>(m_iPad), 5,
			&UIScene_AddExternalServerMenu::KeyboardCompleteServerNameCallback, this, C_4JInput::EKeyboardMode_Numeric, &m_editServerPort);
		break;
	case eControl_JoinEntry:
		JoinServer(false);
		break;
	case eControl_SaveAndJoinEntry:
		JoinServer(true);
		break;
	}
}

void UIScene_AddExternalServerMenu::JoinServer(bool save) {
	if (m_serverName.empty() || m_serverIP.empty() || m_serverPort.empty()) {
		UINT buttons[1] = { IDS_CONFIRM_OK };
		ui.RequestMessageBox(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, buttons, 1, m_iPad, NULL, NULL, app.GetStringTable());
		return;
	}

	unsigned int port = 0;
	for (size_t index = 0; index < m_serverPort.size(); ++index) {
		if (m_serverPort[index] < L'0' || m_serverPort[index] > L'9') {
			UINT buttons[1] = { IDS_CONFIRM_OK };
			ui.RequestMessageBox(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, buttons, 1, m_iPad, NULL, NULL, app.GetStringTable());
			return;
		}
		port = port * 10 + static_cast<unsigned int>(m_serverPort[index] - L'0');
		if (port > 65535) {
			UINT buttons[1] = { IDS_CONFIRM_OK };
			ui.RequestMessageBox(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, buttons, 1, m_iPad, NULL, NULL, app.GetStringTable());
			return;
		}
	}

	if (port == 0) {
		UINT buttons[1] = { IDS_CONFIRM_OK };
		ui.RequestMessageBox(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, buttons, 1, m_iPad, NULL, NULL, app.GetStringTable());
		return;
	}

	char address[257] = {};
	char name[257] = {};
	if (WideCharToMultiByte(CP_ACP, 0, m_serverIP.c_str(), -1, address, sizeof(address), NULL, NULL) == 0 || WideCharToMultiByte(CP_ACP, 0, m_serverName.c_str(), -1, name, sizeof(name), NULL, NULL) == 0) {
		UINT buttons[1] = { IDS_CONFIRM_OK };
		ui.RequestMessageBox(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, buttons, 1, m_iPad, NULL, NULL, app.GetStringTable());
		return;
	}

	uint64_t sessionId = 0;
	if (!ExternalServerList::MakeSessionId(address, static_cast<uint16_t>(port), sessionId)) {
		UINT buttons[1] = { IDS_CONFIRM_OK };
		ui.RequestMessageBox(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, buttons, 1, m_iPad, NULL, NULL, app.GetStringTable());
		return;
	}

	if (save) {
		ExternalServerList::Entry entry = { address, static_cast<uint16_t>(port), name };
		if (!ExternalServerList::Append(entry)) {
			app.DebugPrintf("Failed to save external server to servers.db");
			UINT buttons[1] = { IDS_CONFIRM_OK };
			ui.RequestMessageBox(IDS_ERROR_NETWORK_TITLE, IDS_ERROR_NETWORK, buttons, 1, m_iPad, NULL, NULL, app.GetStringTable());
			return;
		}
		g_NetworkManager.ForceFriendsSessionRefresh();
	}

	delete[] m_serverSession.displayLabel;
	m_serverSession.displayLabel = new wchar_t[m_serverName.size() + 1];
	wcscpy_s(m_serverSession.displayLabel, m_serverName.size() + 1, m_serverName.c_str());
	m_serverSession.displayLabelLength = static_cast<unsigned char>(m_serverName.size());
	m_serverSession.displayLabelViewableStartIndex = 0;
	m_serverSession.sessionId = sessionId;
	m_serverSession.data.netVersion = MINECRAFT_NET_VERSION;
	m_serverSession.data.isReadyToJoin = true;
	m_joinMenuInitData.selectedSession = &m_serverSession;
	m_joinMenuInitData.iPad = m_iPad;

	m_bIgnoreInput = true;
	ui.NavigateToScene(m_iPad, eUIScene_JoinMenu, &m_joinMenuInitData);
}

int UIScene_AddExternalServerMenu::KeyboardCompleteServerNameCallback(LPVOID lpParam, bool bRes)
{
	UIScene_AddExternalServerMenu* scene = static_cast<UIScene_AddExternalServerMenu*>(lpParam);
	scene->m_bIgnoreInput = false;
	if (bRes) {
		uint16_t text[128];
		ZeroMemory(text, sizeof(text));
		InputManager.GetText(text);
		if (text[0] != 0) {
			wstring value = reinterpret_cast<const wchar_t *>(text);
			switch (scene->m_keyboardTarget)
			{
			case eKeyboardTarget_ServerName:
				if (value.size() > 32) value.resize(32);
				scene->m_serverName = value;
				scene->m_editServerName.setLabel(value);
				break;
			case eKeyboardTarget_ServerAddress:
				if (value.size() > 64) value.resize(64);
				scene->m_serverIP = value;
				scene->m_editServerAddress.setLabel(value);
				break;
			case eKeyboardTarget_ServerPort:
				if (value.size() > 5) value.resize(5);
				scene->m_serverPort = value;
				scene->m_editServerPort.setLabel(value);
				break;
			}
		}
	}
	return 0;
}

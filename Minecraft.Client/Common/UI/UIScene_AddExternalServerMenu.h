#pragma once

#include "UIScene.h"

class UIScene_AddExternalServerMenu : public UIScene
{
private:
	enum EControls
	{
		eControl_ServerName,
		eControl_ServerAddress,
		eControl_ServerPort,
		eControl_JoinEntry,
		eControl_SaveAndJoinEntry
	};

	enum EKeyboardTarget
	{
		eKeyboardTarget_ServerName,
		eKeyboardTarget_ServerAddress,
		eKeyboardTarget_ServerPort
	};

	wstring m_serverName;
	bool m_bIgnoreInput;
	EKeyboardTarget m_keyboardTarget;

	UIControl m_controlMainPanel;
	UIControl_Label m_labelServerName, m_labelAddressName, m_labelPortName;
	UIControl_TextInput m_editServerName, m_editServerAddress, m_editServerPort;
	UIControl_Button m_buttonJoinEntry, m_buttonSaveAndJoinEntry;

	UI_BEGIN_MAP_ELEMENTS_AND_NAMES(UIScene)
		UI_MAP_ELEMENT(m_controlMainPanel, "MainPanel")
		UI_BEGIN_MAP_CHILD_ELEMENTS(m_controlMainPanel)
			UI_MAP_ELEMENT(m_labelServerName, "ServerName")
			UI_MAP_ELEMENT(m_editServerName, "EditServerName")

			UI_MAP_ELEMENT(m_labelAddressName, "AddressName")
			UI_MAP_ELEMENT(m_editServerAddress, "EditServerAddress")

			UI_MAP_ELEMENT(m_labelPortName, "PortName")
			UI_MAP_ELEMENT(m_editServerPort, "EditServerPort")

			UI_MAP_ELEMENT(m_buttonJoinEntry, "JoinServer")
			UI_MAP_ELEMENT(m_buttonSaveAndJoinEntry, "SaveAndJoinServer")
		UI_END_MAP_CHILD_ELEMENTS()
	UI_END_MAP_ELEMENTS_AND_NAMES()

public:
	UIScene_AddExternalServerMenu(int iPad, void *initData, UILayer *parentLayer);
	virtual ~UIScene_AddExternalServerMenu();

	virtual EUIScene getSceneType() { return eUIScene_AddExternalServerMenu; }
	virtual void updateTooltips();
	virtual void updateComponents();
	virtual void handleGainFocus(bool navBack);
	virtual UIControl *GetMainPanel();
	virtual void handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled);

protected:
	virtual wstring getMoviePath();
	virtual void handlePress(F64 controlId, F64 childId);

private:
	static int KeyboardCompleteServerNameCallback(LPVOID lpParam, bool bRes);
};

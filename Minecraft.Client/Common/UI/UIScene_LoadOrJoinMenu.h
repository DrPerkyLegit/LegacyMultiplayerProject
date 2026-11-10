#pragma once

#include "UIScene.h"

class LevelGenerationOptions;




class UIScene_LoadOrJoinMenu : public UIScene
{
private:
	enum EControls
	{
		eControl_SavesList,
		eControl_GamesList,
	};

	enum EState
	{
		e_SavesIdle,
		e_SavesRepopulate,
		e_SavesRepopulateAfterMashupHide,
		e_SavesRepopulateAfterDelete,
		e_SavesRepopulateAfterTransferDownload,
	};

	enum eActions
	{
		eAction_None=0,
		eAction_ViewInvites,
		eAction_JoinGame,
	};
	eActions m_eAction;

	static const int JOIN_LOAD_CREATE_BUTTON_INDEX = 0;

	SaveListDetails *m_saveDetails;
	int m_iSaveDetailsCount;

protected:
	UIControl_SaveList m_buttonListSaves;
	UIControl_SaveList m_buttonListGames;
	UIControl_Label m_labelSavesListTitle, m_labelJoinListTitle, m_labelNoGames;
	UIControl m_controlSavesTimer, m_controlJoinTimer;

private:
	UI_BEGIN_MAP_ELEMENTS_AND_NAMES(UIScene)		
		UI_MAP_ELEMENT( m_buttonListSaves, "SavesList")
		UI_MAP_ELEMENT( m_buttonListGames, "JoinList")

		UI_MAP_ELEMENT( m_labelSavesListTitle, "SavesListTitle")
		UI_MAP_ELEMENT( m_labelJoinListTitle, "JoinListTitle")
		UI_MAP_ELEMENT( m_labelNoGames, "NoGames")

		UI_MAP_ELEMENT( m_controlSavesTimer, "SavesTimer")
		UI_MAP_ELEMENT( m_controlJoinTimer, "JoinTimer")

	UI_END_MAP_ELEMENTS_AND_NAMES()

	int m_iDefaultButtonsC;
	int m_iMashUpButtonsC;
	int m_iState;

	vector<FriendSessionInfo *> *m_currentSessions;
	vector<LevelGenerationOptions *> m_generators;
	vector<File *> *m_saves;
	
	bool m_bIgnoreInput;
	bool m_bAllLoaded;
	bool m_bRetrievingSaveThumbnails;
	bool m_bSaveThumbnailReady;
	bool m_bShowingPartyGamesOnly;
	bool m_bInParty;
	JoinMenuInitData *m_initData;
	bool m_bMultiplayerAllowed;
	int m_iTexturePacksNotInstalled;
	int m_iRequestingThumbnailId;
	SAVE_DETAILS *m_pSaveDetails;
	bool m_bSavesDisplayed;
	bool m_bExitScene;
	bool m_bCopying;
	bool m_bCopyingCancelled;
	int m_iSaveInfoC;
	int m_iSaveListIndex;
	int m_iGameListIndex;
	//int *m_iConfigA; // track the texture packs that we don't have installed
	bool m_bSaveTransferInProgress;
	bool m_bSaveTransferCancelled;
	bool m_bUpdateSaveSize;

public:
	UIScene_LoadOrJoinMenu(int iPad, void *initData, UILayer *parentLayer);
	virtual ~UIScene_LoadOrJoinMenu();
	
	virtual void updateTooltips();
	virtual void updateComponents();

	virtual void handleDestroy();
	virtual void handleLoseFocus();
	virtual void handleGainFocus(bool navBack);
	virtual void handleTimerComplete(int id);
	// INPUT
	virtual void handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled);
	virtual void handleFocusChange(F64 controlId, F64 childId);
	virtual void handleInitFocus(F64 controlId, F64 childId);

	virtual EUIScene getSceneType() { return eUIScene_LoadOrJoinMenu;}

	static void UpdateGamesListCallback(LPVOID pParam);
	virtual void tick();

private:
	void Initialise();
	void GetSaveInfo();
	void UpdateGamesList();
	void AddDefaultButtons();
	bool DoesSavesListHaveFocus();
	bool DoesMashUpWorldHaveFocus();
	bool DoesGamesListHaveFocus();

protected:
	// TODO: This should be pure virtual in this class
	virtual wstring getMoviePath();

public:

	static int LoadSaveDataThumbnailReturned(LPVOID lpParam,PBYTE pbThumbnail,DWORD dwThumbnailBytes);
	static int LoadSaveCallback(LPVOID lpParam,bool bRes);
	static int DeleteSaveDialogReturned(void *pParam,int iPad,C4JStorage::EMessageResult result);
	static int SaveOptionsDialogReturned(void *pParam,int iPad,C4JStorage::EMessageResult result);
	static int TexturePackDialogReturned(void *pParam,int iPad,C4JStorage::EMessageResult result);	
	static int DeleteSaveDataReturned(LPVOID lpParam,bool bRes);
	static int RenameSaveDataReturned(LPVOID lpParam,bool bRes);
	static int KeyboardCompleteWorldNameCallback(LPVOID lpParam,bool bRes);
protected:
	void handlePress(F64 controlId, F64 childId);
	void LoadLevelGen(LevelGenerationOptions *levelGen);
	void LoadSaveFromDisk(File *saveFile, ESavePlatform savePlatform = SAVE_FILE_PLATFORM_LOCAL);
public:
	virtual void HandleDLCMountingComplete();


private:
	void CheckAndJoinGame(int gameIndex);
 





};

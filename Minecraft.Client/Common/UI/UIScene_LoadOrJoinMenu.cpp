#include "stdafx.h"
#include "UI.h"
#include "UIScene_LoadOrJoinMenu.h"

#include "..\..\..\Minecraft.World\StringHelpers.h"
#include "..\..\..\Minecraft.World\net.minecraft.world.item.h"
#include "..\..\..\Minecraft.World\net.minecraft.world.level.h"
#include "..\..\..\Minecraft.World\net.minecraft.world.level.chunk.storage.h"
#include "..\..\..\Minecraft.World\ConsoleSaveFile.h"
#include "..\..\..\Minecraft.World\ConsoleSaveFileOriginal.h"
#include "..\..\..\Minecraft.World\ConsoleSaveFileSplit.h"
#include "..\..\ProgressRenderer.h"
#include "..\..\MinecraftServer.h"
#include "..\..\TexturePackRepository.h"
#include "..\..\TexturePack.h"
#include "..\Network\SessionInfo.h"




#define JOIN_LOAD_ONLINE_TIMER_ID 0
#define JOIN_LOAD_ONLINE_TIMER_TIME 100



int UIScene_LoadOrJoinMenu::LoadSaveDataThumbnailReturned(LPVOID lpParam,PBYTE pbThumbnail,DWORD dwThumbnailBytes)
{
    UIScene_LoadOrJoinMenu *pClass= (UIScene_LoadOrJoinMenu *)lpParam;

    app.DebugPrintf("Received data for save thumbnail\n");

    if(pbThumbnail && dwThumbnailBytes)
    {
        pClass->m_saveDetails[pClass->m_iRequestingThumbnailId].pbThumbnailData = new BYTE[dwThumbnailBytes];
        memcpy(pClass->m_saveDetails[pClass->m_iRequestingThumbnailId].pbThumbnailData, pbThumbnail, dwThumbnailBytes);
        pClass->m_saveDetails[pClass->m_iRequestingThumbnailId].dwThumbnailSize = dwThumbnailBytes;
    }
    else
    {
        pClass->m_saveDetails[pClass->m_iRequestingThumbnailId].pbThumbnailData = NULL;
        pClass->m_saveDetails[pClass->m_iRequestingThumbnailId].dwThumbnailSize = 0;
        app.DebugPrintf("Save thumbnail data is NULL, or has size 0\n");
    }
    pClass->m_bSaveThumbnailReady = true;

    return 0;
}

int UIScene_LoadOrJoinMenu::LoadSaveCallback(LPVOID lpParam,bool bRes)
{
    //UIScene_LoadOrJoinMenu *pClass= (UIScene_LoadOrJoinMenu *)lpParam;
    // Get the save data now
    if(bRes)
    {
        app.DebugPrintf("Loaded save OK\n");
    }
    return 0;
}

UIScene_LoadOrJoinMenu::UIScene_LoadOrJoinMenu(int iPad, void *initData, UILayer *parentLayer) : UIScene(iPad, parentLayer)
{
    // Setup all the Iggy references we need for this scene
    initialiseMovie();
    app.SetLiveLinkRequired( true );

    m_iRequestingThumbnailId = 0;
    m_iSaveInfoC=0;
    m_bIgnoreInput = false;
    m_bShowingPartyGamesOnly = false;
    m_bInParty = false;
    m_currentSessions = NULL;
    m_iState=e_SavesIdle;
    //m_bRetrievingSaveInfo=false;

    m_buttonListSaves.init(eControl_SavesList);
    m_buttonListGames.init(eControl_GamesList);

    m_labelSavesListTitle.init( app.GetString(IDS_START_GAME) );
    m_labelJoinListTitle.init( app.GetString(IDS_JOIN_GAME) );
    m_labelNoGames.init( app.GetString(IDS_NO_GAMES_FOUND) );
    m_labelNoGames.setVisible( false );
    m_controlSavesTimer.setVisible( true );
    m_controlJoinTimer.setVisible( true );


    m_bUpdateSaveSize = false;

    m_bAllLoaded = false;
    m_bRetrievingSaveThumbnails = false;
    m_bSaveThumbnailReady = false;
    m_bExitScene=false;
    m_pSaveDetails=NULL;
    m_bSavesDisplayed=false;
    m_saveDetails = NULL;
    m_iSaveDetailsCount = 0;
    m_iTexturePacksNotInstalled = 0;
	m_bCopying = false;
	m_bCopyingCancelled = false;

    m_bSaveTransferCancelled=false;
    m_bSaveTransferInProgress=false;
	m_eAction = eAction_None;

    m_bMultiplayerAllowed = ProfileManager.IsSignedInLive( m_iPad ) && ProfileManager.AllowedToPlayMultiplayer(m_iPad);



    int iLB = -1;



    // block input if we're waiting for DLC to install, and wipe the saves list. The end of dlc mounting custom message will fill the list again
    if(app.StartInstallDLCProcess(m_iPad)==true || app.DLCInstallPending())
    {
        // if we're waiting for DLC to mount, don't fill the save list. The custom message on end of dlc mounting will do that
        m_bIgnoreInput = true;
    }
    else
    {
        Initialise();
    }


    UpdateGamesList();

    g_NetworkManager.SetSessionsUpdatedCallback( &UpdateGamesListCallback, this );

    m_initData= new JoinMenuInitData();

    // 4J Stu - Fix for #12530 -TCR 001 BAS Game Stability: Title will crash if the player disconnects while starting a new world and then opts to play the tutorial once they have been returned to the Main Menu.
    MinecraftServer::resetFlags();

    // If we're not ignoring input, then we aren't still waiting for the DLC to mount, and can now check for corrupt dlc. Otherwise this will happen when the dlc has finished mounting.
    if( !m_bIgnoreInput)
    {
        app.m_dlcManager.checkForCorruptDLCAndAlert();
    }

    // 4J-PB - Only Xbox will not have trial DLC patched into the game

}


UIScene_LoadOrJoinMenu::~UIScene_LoadOrJoinMenu()
{
    g_NetworkManager.SetSessionsUpdatedCallback( NULL, NULL );
    app.SetLiveLinkRequired( false );

    if(m_currentSessions)
    {
        for(AUTO_VAR(it, m_currentSessions->begin()); it < m_currentSessions->end(); ++it)
        {
            delete (*it);
        }
    }

#if TO_BE_IMPLEMENTED
    // Reset the background downloading, in case we changed it by attempting to download a texture pack
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
#endif

    if(m_saveDetails)
    {
        for(int i = 0; i < m_iSaveDetailsCount; ++i)
        {
            delete m_saveDetails[i].pbThumbnailData;
        }
        delete [] m_saveDetails;
    }
}

void UIScene_LoadOrJoinMenu::updateTooltips()
{
    // update the tooltips
    // if the saves list has focus, then we should show the Delete Save tooltip
    // if the games list has focus, then we should the the View Gamercard tooltip
    int iRB=-1;	
    int iY = -1;
    int iLB = -1;
    int iX=-1;
    if (DoesGamesListHaveFocus() && m_buttonListGames.getItemCount() > 0)
    {
        if (m_currentSessions == NULL || m_iGameListIndex < static_cast<int>(m_currentSessions->size()))
        {
            iY = IDS_TOOLTIPS_VIEW_GAMERCARD;
        }
    }
    else if (DoesSavesListHaveFocus())
    {
        if((m_iDefaultButtonsC > 0) && (m_iSaveListIndex >= m_iDefaultButtonsC))
        {		
            if(StorageManager.GetSaveDisabled())
            {
                iRB=IDS_TOOLTIPS_DELETESAVE;
            }
            else
            {
                if(StorageManager.EnoughSpaceForAMinSaveGame())
                {
                    iRB=IDS_TOOLTIPS_SAVEOPTIONS;
                }
                else
                {
                    iRB=IDS_TOOLTIPS_DELETESAVE;
                }
            }
        }
    }
	else if(DoesMashUpWorldHaveFocus())
	{
		// If it's a mash-up pack world, give the Hide option
		iRB=IDS_TOOLTIPS_HIDE;
	}

    if(m_bInParty)
    {
        if( m_bShowingPartyGamesOnly ) iLB = IDS_TOOLTIPS_ALL_GAMES;
        else iLB = IDS_TOOLTIPS_PARTY_GAMES;
    }


    if(ProfileManager.IsFullVersion()==false )
    {
        iRB = -1;
    }
    else if(StorageManager.GetSaveDisabled())
    {
    }
    else
    {
        iX = IDS_TOOLTIPS_CHANGEDEVICE;
    }

    ui.SetTooltips( DEFAULT_XUI_MENU_USER, IDS_TOOLTIPS_SELECT, IDS_TOOLTIPS_BACK, iX, iY,-1,-1,iLB,iRB);
}

// 
void UIScene_LoadOrJoinMenu::Initialise()
{
    m_iSaveListIndex = 0;
	m_iGameListIndex = 0;

    m_iDefaultButtonsC = 0;
	m_iMashUpButtonsC=0;

    // Check if we're in the trial version
    if(ProfileManager.IsFullVersion()==false)
    {


        AddDefaultButtons();

#if TO_BE_IMPLEMENTED
        m_pSavesList->SetCurSelVisible(0);
#endif
    }
    else if(StorageManager.GetSaveDisabled())
    {

#if TO_BE_IMPLEMENTED
        if(StorageManager.GetSaveDeviceSelected(m_iPad))
#endif
        {
            // saving is disabled, but we should still be able to load from a selected save device



            GetSaveInfo();
        }
#if TO_BE_IMPLEMENTED
        else
        {
            AddDefaultButtons();
            m_controlSavesTimer.setVisible( false );
        }
#endif
    }
    else
    {
        // 4J-PB - we need to check that there is enough space left to create a copy of the save (for a rename)
        bool bCanRename = StorageManager.EnoughSpaceForAMinSaveGame();

        GetSaveInfo();
    }

    m_bIgnoreInput=false;
    app.m_dlcManager.checkForCorruptDLCAndAlert();
}

void UIScene_LoadOrJoinMenu::updateComponents()
{
    m_parentLayer->showComponent(m_iPad,eUIComponent_Panorama,true);
    m_parentLayer->showComponent(m_iPad,eUIComponent_Logo,true);
}

void UIScene_LoadOrJoinMenu::handleDestroy()
{

	// shut down the keyboard if it is displayed
}

void UIScene_LoadOrJoinMenu::handleGainFocus(bool navBack)
{
    UIScene::handleGainFocus(navBack);

    updateTooltips();

    // Add load online timer
    addTimer(JOIN_LOAD_ONLINE_TIMER_ID,JOIN_LOAD_ONLINE_TIMER_TIME);

    if(navBack)
    {
        app.SetLiveLinkRequired( true );

        m_bMultiplayerAllowed = ProfileManager.IsSignedInLive( m_iPad ) && ProfileManager.AllowedToPlayMultiplayer(m_iPad); 

        // re-enable button presses
        m_bIgnoreInput=false;

        // block input if we're waiting for DLC to install, and wipe the saves list. The end of dlc mounting custom message will fill the list again
        if(app.StartInstallDLCProcess(m_iPad)==false)
        {
            // not doing a mount, so re-enable input
            m_bIgnoreInput=false;
        }
        else
        {
            m_bIgnoreInput=true;
            m_buttonListSaves.clearList();
            m_controlSavesTimer.setVisible(true);
        }

        if( m_bMultiplayerAllowed )
        {
#if TO_BE_IMPLEMENTED
            HXUICLASS hClassFullscreenProgress = XuiFindClass( L"CScene_FullscreenProgress" );
            HXUICLASS hClassConnectingProgress = XuiFindClass( L"CScene_ConnectingProgress" );

            // If we are navigating back from a full screen progress scene, then that means a connection attempt failed
            if( XuiIsInstanceOf( hSceneFrom, hClassFullscreenProgress ) || XuiIsInstanceOf( hSceneFrom, hClassConnectingProgress ) )
            {
                UpdateGamesList();
            }
#endif
        }
        else
        {
            m_buttonListGames.clearList();
            m_controlJoinTimer.setVisible(true);
            m_labelNoGames.setVisible(false);
#if TO_BE_IMPLEMENTED
            m_SavesList.InitFocus(m_iPad);
#endif
        }

        // are we back here because of a delete of a corrupt save?

        if(app.GetCorruptSaveDeleted())
        {
            // wipe the list and repopulate it
            m_iState=e_SavesRepopulateAfterDelete;
            app.SetCorruptSaveDeleted(false);
        }
    }
}

void UIScene_LoadOrJoinMenu::handleLoseFocus()
{
    // Kill load online timer
    killTimer(JOIN_LOAD_ONLINE_TIMER_ID);
}

wstring UIScene_LoadOrJoinMenu::getMoviePath()
{
    return L"LoadOrJoinMenu";
}

void UIScene_LoadOrJoinMenu::tick()
{
    UIScene::tick();

    if(m_bExitScene) // navigate forward or back
    {
        if(!m_bRetrievingSaveThumbnails)
        {
            // need to wait for any callback retrieving thumbnail to complete
            navigateBack();
        }
    }
    // Stop loading thumbnails if we navigate forwards
    if(hasFocus(m_iPad))
    {
        // Display the saves if we have them
        if(!m_bSavesDisplayed)
        {
            m_pSaveDetails=StorageManager.ReturnSavesInfo();
            if(m_pSaveDetails!=NULL)
            {
                //CD - Fix - Adding define for ORBIS/XBOXONE

                AddDefaultButtons();
                m_bSavesDisplayed=true;
                UpdateGamesList();

                if(m_saveDetails!=NULL)
                {
                    for(unsigned int i = 0; i < m_iSaveDetailsCount; ++i)
                    {
                        if(m_saveDetails[i].pbThumbnailData!=NULL)
                        {
                            delete m_saveDetails[i].pbThumbnailData;
                        }
                    }
                    delete m_saveDetails;
                }
                m_saveDetails = new SaveListDetails[m_pSaveDetails->iSaveC];

                m_iSaveDetailsCount = m_pSaveDetails->iSaveC;
                for(unsigned int i = 0; i < m_pSaveDetails->iSaveC; ++i)
                {
                    m_buttonListSaves.addItem(m_pSaveDetails->SaveInfoA[i].UTF8SaveTitle, L"");

                    m_saveDetails[i].saveId = i;
                    memcpy(m_saveDetails[i].UTF8SaveName, m_pSaveDetails->SaveInfoA[i].UTF8SaveTitle, 128);
                    memcpy(m_saveDetails[i].UTF8SaveFilename, m_pSaveDetails->SaveInfoA[i].UTF8SaveFilename, MAX_SAVEFILENAME_LENGTH);
                }
                m_controlSavesTimer.setVisible( false );

                // set focus on the first button

			}
        }

        if(!m_bExitScene && m_bSavesDisplayed && !m_bRetrievingSaveThumbnails && !m_bAllLoaded)
        {
            if( m_iRequestingThumbnailId < (m_buttonListSaves.getItemCount() - m_iDefaultButtonsC ))
            {
                m_bRetrievingSaveThumbnails = true;
                app.DebugPrintf("Requesting the first thumbnail\n");
                // set the save to load
                PSAVE_DETAILS pSaveDetails=StorageManager.ReturnSavesInfo();
                C4JStorage::ESaveGameState eLoadStatus=StorageManager.LoadSaveDataThumbnail(&pSaveDetails->SaveInfoA[(int)m_iRequestingThumbnailId],&LoadSaveDataThumbnailReturned,this);

                if(eLoadStatus!=C4JStorage::ESaveGame_GetSaveThumbnail)
                {
                    // something went wrong
                    m_bRetrievingSaveThumbnails=false;
                    m_bAllLoaded = true;
                }
            }
        }
        else if (m_bSavesDisplayed && m_bSaveThumbnailReady)
        {
            m_bSaveThumbnailReady = false;

            // check we're not waiting to exit the scene
            if(!m_bExitScene)
            {
                // convert to utf16
                uint16_t u16Message[MAX_SAVEFILENAME_LENGTH];
                int result = ::MultiByteToWideChar(
                    CP_UTF8,                // convert from UTF-8
                    MB_ERR_INVALID_CHARS,   // error on invalid chars
                    m_saveDetails[m_iRequestingThumbnailId].UTF8SaveFilename,            // source UTF-8 string
                    MAX_SAVEFILENAME_LENGTH,                 // total length of source UTF-8 string,
                    // in CHAR's (= bytes), including end-of-string \0
                    (wchar_t *)u16Message,               // destination buffer
                    MAX_SAVEFILENAME_LENGTH                // size of destination buffer, in WCHAR's
                    );
                if( m_saveDetails[m_iRequestingThumbnailId].pbThumbnailData )
                {
                    registerSubstitutionTexture((wchar_t *)u16Message,m_saveDetails[m_iRequestingThumbnailId].pbThumbnailData,m_saveDetails[m_iRequestingThumbnailId].dwThumbnailSize);
                }
                m_buttonListSaves.setTextureName(m_iRequestingThumbnailId + m_iDefaultButtonsC, (wchar_t *)u16Message);

                ++m_iRequestingThumbnailId;
                if( m_iRequestingThumbnailId < (m_buttonListSaves.getItemCount() - m_iDefaultButtonsC ))
                {
                    app.DebugPrintf("Requesting another thumbnail\n");
                    // set the save to load
                    PSAVE_DETAILS pSaveDetails=StorageManager.ReturnSavesInfo();
                    C4JStorage::ESaveGameState eLoadStatus=StorageManager.LoadSaveDataThumbnail(&pSaveDetails->SaveInfoA[(int)m_iRequestingThumbnailId],&LoadSaveDataThumbnailReturned,this);
                    if(eLoadStatus!=C4JStorage::ESaveGame_GetSaveThumbnail)
                    {
                        // something went wrong
                        m_bRetrievingSaveThumbnails=false;
                        m_bAllLoaded = true;
                    }
                }
                else
                {
                    m_bRetrievingSaveThumbnails = false;
                    m_bAllLoaded = true;
                }
            }
            else
            {
                // stop retrieving thumbnails, and exit
                m_bRetrievingSaveThumbnails = false;
            }
        }
    }

    switch(m_iState)
    {
    case e_SavesIdle:
        break;
    case e_SavesRepopulate:
        m_bIgnoreInput = false;
        m_iState=e_SavesIdle;
        m_bAllLoaded=false;
        m_bRetrievingSaveThumbnails=false;
        m_iRequestingThumbnailId = 0;
        GetSaveInfo();
        break;
	case e_SavesRepopulateAfterMashupHide:
        m_bIgnoreInput = false;
        m_iRequestingThumbnailId = 0;
        m_bAllLoaded=false;
        m_bRetrievingSaveThumbnails=false;
        m_bSavesDisplayed=false;
        m_iSaveInfoC=0;
        m_buttonListSaves.clearList();
        GetSaveInfo();
        m_iState=e_SavesIdle;
		break;
    case e_SavesRepopulateAfterDelete:
	case e_SavesRepopulateAfterTransferDownload:
        m_bIgnoreInput = false;
        m_iRequestingThumbnailId = 0;
        m_bAllLoaded=false;
        m_bRetrievingSaveThumbnails=false;
        m_bSavesDisplayed=false;
        m_iSaveInfoC=0;
        m_buttonListSaves.clearList();
        StorageManager.ClearSavesInfo();
        GetSaveInfo();
        m_iState=e_SavesIdle;
        break;
    }


    // SAVE TRANSFERS

}

void UIScene_LoadOrJoinMenu::GetSaveInfo()
{
    unsigned int uiSaveC=0;

    // This will return with the number retrieved in uiSaveC

    if(app.DebugSettingsOn() && app.GetLoadSavesFromFolderEnabled())
    {

        uiSaveC = 0;
        File savesDir(L"Saves");
        if( savesDir.exists() )
        {
            m_saves = savesDir.listFiles();
            uiSaveC = (unsigned int)m_saves->size();
        }
        // add the New Game and Tutorial after the saves list is retrieved, if there are any saves

        // Add two for New Game and Tutorial
        unsigned int listItems = uiSaveC;

        AddDefaultButtons();

        for(unsigned int i=0;i<listItems;i++)
        {

            wstring wName = m_saves->at(i)->getName();
            wchar_t *name = new wchar_t[wName.size()+1];
            for(unsigned int j = 0; j < wName.size(); ++j)
            {
                name[j] = wName[j];
            }
            name[wName.size()] = 0;
            m_buttonListSaves.addItem(name,L"");
        }
        m_bSavesDisplayed = true;
        m_bAllLoaded = true;
        m_bIgnoreInput = false;
    }
    else
    {
        // clear the saves list
        m_bSavesDisplayed = false; // we're blocking the exit from this scene until complete
        m_buttonListSaves.clearList();
        m_iSaveInfoC=0;
        m_controlSavesTimer.setVisible(true);

        m_pSaveDetails=StorageManager.ReturnSavesInfo();
        if(m_pSaveDetails==NULL)
        {
            C4JStorage::ESaveGameState eSGIStatus= StorageManager.GetSavesInfo(m_iPad,NULL,this,"save"); 
        }

#if TO_BE_IMPLEMENTED
        if(eSGIStatus==C4JStorage::ESGIStatus_NoSaves)
        {
            uiSaveC=0;
            m_controlSavesTimer.setVisible( false );
            m_SavesList.SetEnable(TRUE);
        }
#endif
    }

    return;
}

void UIScene_LoadOrJoinMenu::AddDefaultButtons()
{
    m_iDefaultButtonsC = 0;
	m_iMashUpButtonsC=0;
	m_generators.clear();

    m_buttonListSaves.addItem(app.GetString(IDS_CREATE_NEW_WORLD));
    m_iDefaultButtonsC++;

    int i = 0;

    for(AUTO_VAR(it, app.getLevelGenerators()->begin()); it != app.getLevelGenerators()->end(); ++it)
    {
        LevelGenerationOptions *levelGen = *it;

        // retrieve the save icon from the texture pack, if there is one
        unsigned int uiTexturePackID=levelGen->getRequiredTexturePackId();

		if(uiTexturePackID!=0)
		{
			unsigned int uiMashUpWorldsBitmask=app.GetMashupPackWorlds(m_iPad);

			if((uiMashUpWorldsBitmask & (1<<(uiTexturePackID-1024)))==0)
			{
				// this world is hidden, so skip
				continue;
			}
		}
		
		m_generators.push_back(levelGen);
		m_buttonListSaves.addItem(levelGen->getWorldName());

        if(uiTexturePackID!=0)
        {
			// increment the count of the mash-up pack worlds in the save list
			m_iMashUpButtonsC++;
            TexturePack *tp = Minecraft::GetInstance()->skins->getTexturePackById(levelGen->getRequiredTexturePackId());
            DWORD dwImageBytes;
            PBYTE pbImageData = tp->getPackIcon(dwImageBytes);

            if(dwImageBytes > 0 && pbImageData)
            {
                wchar_t imageName[64];
                swprintf(imageName,64,L"tpack%08x",tp->getId());
                registerSubstitutionTexture(imageName, pbImageData, dwImageBytes);
                m_buttonListSaves.setTextureName( m_buttonListSaves.getItemCount() - 1, imageName );
            }
        }

        ++i;
    }
    m_iDefaultButtonsC += i;
}

void UIScene_LoadOrJoinMenu::handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled)
{
    if(m_bIgnoreInput) return;

    // if we're retrieving save info, ignore key presses
    if(!m_bSavesDisplayed) return;

    ui.AnimateKeyPress(m_iPad, key, repeat, pressed, released);

    switch(key)
    {
    case ACTION_MENU_CANCEL:
        if(pressed)
        {
            navigateBack();
            handled = true;
        }
        break;
    case ACTION_MENU_X:
#if TO_BE_IMPLEMENTED
        // Change device
        // Fix for #12531 - TCR 001: BAS Game Stability: When a player selects to change a storage 
        // device, and repeatedly backs out of the SD screen, disconnects from LIVE, and then selects a SD, the title crashes.
        m_bIgnoreInput=true;
        StorageManager.SetSaveDevice(&CScene_MultiGameJoinLoad::DeviceSelectReturned,this,true);
        ui.PlayUISFX(eSFX_Press);
#endif
        // Save Transfer
        break;
    case ACTION_MENU_Y:
        break;

    case ACTION_MENU_RIGHT_SCROLL:
        if(DoesSavesListHaveFocus())
        {
            // 4J-PB - check we are on a valid save
            if((m_iDefaultButtonsC != 0) && (m_iSaveListIndex >= m_iDefaultButtonsC))
            {
                m_bIgnoreInput = true;

                // Could be delete save or Save Options
                if(StorageManager.GetSaveDisabled())
                {
                    // delete the save game
                    // Have to ask the player if they are sure they want to delete this game
                    UINT uiIDA[2];
                    uiIDA[0]=IDS_CONFIRM_CANCEL;
                    uiIDA[1]=IDS_CONFIRM_OK;
                    ui.RequestMessageBox(IDS_TOOLTIPS_DELETESAVE, IDS_TEXT_DELETE_SAVE, uiIDA, 2, iPad,&UIScene_LoadOrJoinMenu::DeleteSaveDialogReturned,this, app.GetStringTable(),NULL,0,false);
                }
                else
                {
                    if(StorageManager.EnoughSpaceForAMinSaveGame())
                    {
                        UINT uiIDA[4];
                        uiIDA[0]=IDS_CONFIRM_CANCEL;
                        uiIDA[1]=IDS_TITLE_RENAMESAVE;
                        uiIDA[2]=IDS_TOOLTIPS_DELETESAVE;
                        int numOptions = 3;
                        ui.RequestMessageBox(IDS_TOOLTIPS_SAVEOPTIONS, IDS_TEXT_SAVEOPTIONS, uiIDA, numOptions, iPad,&UIScene_LoadOrJoinMenu::SaveOptionsDialogReturned,this, app.GetStringTable(),NULL,0,false);
                    }
                    else
                    {
                        // delete the save game
                        // Have to ask the player if they are sure they want to delete this game
                        UINT uiIDA[2];
                        uiIDA[0]=IDS_CONFIRM_CANCEL;
                        uiIDA[1]=IDS_CONFIRM_OK;
                        ui.RequestMessageBox(IDS_TOOLTIPS_DELETESAVE, IDS_TEXT_DELETE_SAVE, uiIDA, 2,iPad,&UIScene_LoadOrJoinMenu::DeleteSaveDialogReturned,this, app.GetStringTable(),NULL,0,false);
                    }
                }
                ui.PlayUISFX(eSFX_Press);
            }
        }
		else if(DoesMashUpWorldHaveFocus())
		{
			// hiding a mash-up world
			if((m_iSaveListIndex != JOIN_LOAD_CREATE_BUTTON_INDEX))
			{
				LevelGenerationOptions *levelGen = m_generators.at(m_iSaveListIndex - 1);

				if(!levelGen->isTutorial())
				{
					if(levelGen->requiresTexturePack())
					{
						unsigned int uiPackID=levelGen->getRequiredTexturePackId();

						m_bIgnoreInput = true;
						app.HideMashupPackWorld(m_iPad,uiPackID);

						// update the saves list
						m_iState = e_SavesRepopulateAfterMashupHide;
					}
				}
			}
			ui.PlayUISFX(eSFX_Press);

		}
        break;
    case ACTION_MENU_LEFT_SCROLL:
        break;
    case ACTION_MENU_LEFT:
    case ACTION_MENU_RIGHT:
        {
            // if we are on the saves menu, check there are games in the games list to move to
            if(DoesSavesListHaveFocus())
            {
                if( m_buttonListGames.getItemCount() > 0)
                {
                    sendInputToMovie(key, repeat, pressed, released);
                }
            }
            else
            {
                sendInputToMovie(key, repeat, pressed, released);
            }
        }
        break;

    case ACTION_MENU_OK:
    case ACTION_MENU_UP:
    case ACTION_MENU_DOWN:
    case ACTION_MENU_PAGEUP:
    case ACTION_MENU_PAGEDOWN:
        sendInputToMovie(key, repeat, pressed, released);
        handled = true;
        break;
    }
}

int UIScene_LoadOrJoinMenu::KeyboardCompleteWorldNameCallback(LPVOID lpParam,bool bRes)
{
    // 4J HEG - No reason to set value if keyboard was cancelled
    UIScene_LoadOrJoinMenu *pClass=(UIScene_LoadOrJoinMenu *)lpParam;
	pClass->m_bIgnoreInput=false;
    if (bRes)
    {	
        uint16_t ui16Text[128];
        ZeroMemory(ui16Text, 128 * sizeof(uint16_t) );
        InputManager.GetText(ui16Text);

        // check the name is valid
        if(ui16Text[0]!=0)
        {
        }
        else 
        {
            pClass->m_bIgnoreInput=false;
            pClass->updateTooltips();
        }
    } 
    else 
    {
        pClass->m_bIgnoreInput=false;
        pClass->updateTooltips();
    }


    return 0;
}
void UIScene_LoadOrJoinMenu::handleInitFocus(F64 controlId, F64 childId)
{
    app.DebugPrintf(app.USER_SR, "UIScene_LoadOrJoinMenu::handleInitFocus - %d , %d\n", (int)controlId, (int)childId);
}

void UIScene_LoadOrJoinMenu::handleFocusChange(F64 controlId, F64 childId) 
{
    app.DebugPrintf(app.USER_SR, "UIScene_LoadOrJoinMenu::handleFocusChange - %d , %d\n", (int)controlId, (int)childId);
    
	switch((int)controlId)
	{
	case eControl_GamesList:	
		m_iGameListIndex = childId;
		m_buttonListGames.updateChildFocus( (int) childId );
		break;
	case eControl_SavesList:
		m_iSaveListIndex = childId;
        m_bUpdateSaveSize = true;
		break;
	};
    updateTooltips();
}



void UIScene_LoadOrJoinMenu::handlePress(F64 controlId, F64 childId)
{
    switch((int)controlId)
    {
    case eControl_SavesList:
        {
            m_bIgnoreInput=true;

            int lGenID = (int)childId - 1;

            //CD - Added for audio
            ui.PlayUISFX(eSFX_Press);

            if((int)childId == JOIN_LOAD_CREATE_BUTTON_INDEX)
            {		
                app.SetTutorialMode( false );

                m_controlJoinTimer.setVisible( false );

                app.SetCorruptSaveDeleted(false);

                CreateWorldMenuInitData *params = new CreateWorldMenuInitData();
                params->iPad = m_iPad;
                ui.NavigateToScene(m_iPad,eUIScene_CreateWorldMenu,(void *)params);
            }
            else if (lGenID < m_generators.size())
            {
                LevelGenerationOptions *levelGen = m_generators.at(lGenID);
                app.SetTutorialMode( levelGen->isTutorial() );
                // Reset the autosave time
                app.SetAutosaveTimerTime();

                if(levelGen->isTutorial())
                {
                    LoadLevelGen(levelGen);
                }
                else
                {
                    LoadMenuInitData *params = new LoadMenuInitData();
                    params->iPad = m_iPad;
                    // need to get the iIndex from the list item, since the position in the list doesn't correspond to the GetSaveGameInfo list because of sorting
                    params->iSaveGameInfoIndex=-1;
                    //params->pbSaveRenamed=&m_bSaveRenamed;
                    params->levelGen = levelGen;
                    params->saveDetails = NULL;

                    // navigate to the settings scene
                    ui.NavigateToScene(ProfileManager.GetPrimaryPad(),eUIScene_LoadMenu, params);
                }
            }
            else
            {
                {		
                    app.SetTutorialMode( false );

                    if(app.DebugSettingsOn() && app.GetLoadSavesFromFolderEnabled())
                    {
                        LoadSaveFromDisk(m_saves->at((int)childId-m_iDefaultButtonsC));
                    }
                    else
                    {
                        LoadMenuInitData *params = new LoadMenuInitData();
                        params->iPad = m_iPad;
                        // need to get the iIndex from the list item, since the position in the list doesn't correspond to the GetSaveGameInfo list because of sorting
                        params->iSaveGameInfoIndex=((int)childId)-m_iDefaultButtonsC;
                        //params->pbSaveRenamed=&m_bSaveRenamed;
                        params->levelGen = NULL;
                        params->saveDetails = &m_saveDetails[ ((int)childId)-m_iDefaultButtonsC ];

                        {
                            // navigate to the settings scene
                            ui.NavigateToScene(ProfileManager.GetPrimaryPad(),eUIScene_LoadMenu, params);
                        }
                    }
                }
            }
        }
        break;
    case eControl_GamesList:
        {
            m_bIgnoreInput=true;
			
			m_eAction = eAction_JoinGame;

            //CD - Added for audio
            ui.PlayUISFX(eSFX_Press);

			{
				int nIndex = (int)childId;
				m_iGameListIndex = nIndex;
				if (m_currentSessions != NULL && nIndex == static_cast<int>(m_currentSessions->size()))
				{
					m_bIgnoreInput = false;
					ui.NavigateToScene(m_iPad, eUIScene_AddExternalServerMenu, NULL);
					break;
				}
				CheckAndJoinGame(nIndex);
			}

            break;
        }
    }
}

void UIScene_LoadOrJoinMenu::CheckAndJoinGame(int gameIndex)
{
	if( m_buttonListGames.getItemCount() > 0 && gameIndex < m_currentSessions->size() )
	{

		//CScene_MultiGameInfo::JoinMenuInitData *initData = new CScene_MultiGameInfo::JoinMenuInitData();
		m_initData->iPad = 0;;
		m_initData->selectedSession = m_currentSessions->at( gameIndex );

		// check that we have the texture pack available
		// If it's not the default texture pack
		if(m_initData->selectedSession->data.texturePackParentId!=0)
		{
			int texturePacksCount = Minecraft::GetInstance()->skins->getTexturePackCount();
			bool bHasTexturePackInstalled=false;

			for(int i=0;i<texturePacksCount;i++)
			{
				TexturePack *tp = Minecraft::GetInstance()->skins->getTexturePackByIndex(i);
				if(tp->getDLCParentPackId()==m_initData->selectedSession->data.texturePackParentId)
				{
					bHasTexturePackInstalled=true;
					break;
				}
			}

			if(bHasTexturePackInstalled==false)
			{
				// upsell the texture pack
				// tell sentient about the upsell of the full version of the skin pack
				UINT uiIDA[2];

				uiIDA[0]=IDS_TEXTUREPACK_FULLVERSION;
				//uiIDA[1]=IDS_TEXTURE_PACK_TRIALVERSION;
				uiIDA[1]=IDS_CONFIRM_CANCEL;


				// Give the player a warning about the texture pack missing
				ui.RequestMessageBox(IDS_DLC_TEXTUREPACK_NOT_PRESENT_TITLE, IDS_DLC_TEXTUREPACK_NOT_PRESENT, uiIDA, 2, m_iPad,&UIScene_LoadOrJoinMenu::TexturePackDialogReturned,this,app.GetStringTable(),NULL,0,false);

				return;
			}
		}
		m_controlJoinTimer.setVisible( false );


		m_bIgnoreInput=true;
		ui.NavigateToScene(ProfileManager.GetPrimaryPad(),eUIScene_JoinMenu,m_initData);
	}
}

void UIScene_LoadOrJoinMenu::LoadLevelGen(LevelGenerationOptions *levelGen)
{	
    // Load data from disc
    //File saveFile( L"Tutorial\\Tutorial" );
    //LoadSaveFromDisk(&saveFile);

    // clear out the app's terrain features list
    app.ClearTerrainFeaturePosition();

    StorageManager.ResetSaveData();
    // Make our next save default to the name of the level
    StorageManager.SetSaveTitle(levelGen->getDefaultSaveName().c_str());

    bool isClientSide = false;
    bool isPrivate = false;
    // TODO int maxPlayers = MINECRAFT_NET_MAX_PLAYERS;
    int maxPlayers = 8;

    if( app.GetTutorialMode() )
    {
        isClientSide = false;
        maxPlayers = 4;
    }

    g_NetworkManager.HostGame(0,isClientSide,isPrivate,maxPlayers,0);

    NetworkGameInitData *param = new NetworkGameInitData();
    param->seed = 0;
    param->saveData = NULL;
    param->settings = app.GetGameHostOption( eGameHostOption_Tutorial );
    param->levelGen = levelGen;

    if(levelGen->requiresTexturePack())
    {
        param->texturePackId = levelGen->getRequiredTexturePackId();

        Minecraft *pMinecraft = Minecraft::GetInstance();
        pMinecraft->skins->selectTexturePackById(param->texturePackId);
        //pMinecraft->skins->updateUI();
    }

    g_NetworkManager.FakeLocalPlayerJoined();

    LoadingInputParams *loadingParams = new LoadingInputParams();
    loadingParams->func = &CGameNetworkManager::RunNetworkGameThreadProc;
    loadingParams->lpParam = (LPVOID)param;

    UIFullscreenProgressCompletionData *completionData = new UIFullscreenProgressCompletionData();
    completionData->bShowBackground=TRUE;
    completionData->bShowLogo=TRUE;
    completionData->type = e_ProgressCompletion_CloseAllPlayersUIScenes;
    completionData->iPad = DEFAULT_XUI_MENU_USER;
    loadingParams->completionData = completionData;

    ui.NavigateToScene(ProfileManager.GetPrimaryPad(),eUIScene_FullscreenProgress, loadingParams);
}

void UIScene_LoadOrJoinMenu::UpdateGamesListCallback(LPVOID pParam)
{
    if(pParam != NULL)
    {
        UIScene_LoadOrJoinMenu *pScene = (UIScene_LoadOrJoinMenu *)pParam;
        pScene->UpdateGamesList();
    }
}

void UIScene_LoadOrJoinMenu::UpdateGamesList()
{
    // If we're ignoring input scene isn't active so do nothing
    if (m_bIgnoreInput) return;

    // If a texture pack is loading, or will be loading, then ignore this ( we are going to be destroyed anyway)
    if( Minecraft::GetInstance()->skins->getSelected()->isLoadingData() || (Minecraft::GetInstance()->skins->needsUIUpdate() || ui.IsReloadingSkin()) ) return;

    // if we're retrieving save info, don't show the list yet as we will be ignoring press events
    if(!m_bSavesDisplayed)
    {
        return;
    }


    FriendSessionInfo *pSelectedSession = NULL;
    bool addServerWasSelected = false;
    if(DoesGamesListHaveFocus() && m_buttonListGames.getItemCount() > 0)
    {
        unsigned int nIndex = m_buttonListGames.getCurrentSelection();
        if (m_currentSessions != NULL)
        {
            addServerWasSelected = nIndex == m_currentSessions->size();
            if (nIndex < m_currentSessions->size())
            {
                pSelectedSession = m_currentSessions->at(nIndex);
            }
        }
    }

    SessionID selectedSessionId;
	ZeroMemory(&selectedSessionId,sizeof(SessionID));
    if( pSelectedSession != NULL )selectedSessionId = pSelectedSession->sessionId;
    pSelectedSession = NULL;

    m_controlJoinTimer.setVisible( false );

    // if the saves list has focus, then we should show the Delete Save tooltip
    // if the games list has focus, then we should show the View Gamercard tooltip
    int iRB=-1;	
    int iY = -1;
    int iX=-1;

    delete m_currentSessions;
    m_currentSessions = g_NetworkManager.GetSessionList( m_iPad, 1, m_bShowingPartyGamesOnly );

    // Update the xui list displayed
    unsigned int xuiListSize = m_buttonListGames.getItemCount();
    unsigned int filteredListSize = (unsigned int)m_currentSessions->size();

    BOOL gamesListHasFocus = DoesGamesListHaveFocus();

    if(filteredListSize > 0)
    {
#if TO_BE_IMPLEMENTED
        if( !m_pGamesList->IsEnabled() )
        {
            m_pGamesList->SetEnable(TRUE);
            m_pGamesList->SetCurSel( 0 );
        }
#endif
        m_labelNoGames.setVisible( false );
        m_controlJoinTimer.setVisible( false );
    }
    else
    {
#if TO_BE_IMPLEMENTED
        m_pGamesList->SetEnable(FALSE);
#endif
        m_controlJoinTimer.setVisible( false );
        m_labelNoGames.setVisible( true );

#if TO_BE_IMPLEMENTED
        if( gamesListHasFocus ) m_pGamesList->InitFocus(m_iPad);
#endif
    }

    // clear out the games list and re-fill
    m_buttonListGames.clearList();

    if( filteredListSize > 0 )
    {
        // Reset the focus to the selected session if it still exists
        unsigned int sessionIndex = 0;
        m_buttonListGames.setCurrentSelection(0);

        for( AUTO_VAR(it, m_currentSessions->begin()); it < m_currentSessions->end(); ++it)
        {
            FriendSessionInfo *sessionInfo = *it;

            wchar_t textureName[64] = L"\0";

            // Is this a default game or a texture pack game?
            if(sessionInfo->data.texturePackParentId!=0)
            {
                // Do we have the texture pack
                Minecraft *pMinecraft = Minecraft::GetInstance();
                TexturePack *tp = pMinecraft->skins->getTexturePackById(sessionInfo->data.texturePackParentId);
                HRESULT hr;

                DWORD dwImageBytes=0;
                PBYTE pbImageData=NULL;

                if(tp==NULL)
                {
                    DWORD dwBytes=0;
                    PBYTE pbData=NULL;
                    app.GetTPD(sessionInfo->data.texturePackParentId,&pbData,&dwBytes);

                    // is it in the tpd data ?
                    app.GetFileFromTPD(eTPDFileType_Icon,pbData,dwBytes,&pbImageData,&dwImageBytes );
                    if(dwImageBytes > 0 && pbImageData)
                    {
                        swprintf(textureName,64,L"%ls",sessionInfo->displayLabel);
                        registerSubstitutionTexture(textureName,pbImageData,dwImageBytes);
                    }
                }
                else
                {
                    pbImageData = tp->getPackIcon(dwImageBytes);
                    if(dwImageBytes > 0 && pbImageData)
                    {
                        swprintf(textureName,64,L"%ls",sessionInfo->displayLabel);
                        registerSubstitutionTexture(textureName,pbImageData,dwImageBytes);
                    }
                }
            }
            else
            {
                // default texture pack
                Minecraft *pMinecraft = Minecraft::GetInstance();
                TexturePack *tp = pMinecraft->skins->getTexturePackByIndex(0);

                DWORD dwImageBytes;
                PBYTE pbImageData = tp->getPackIcon(dwImageBytes);

                if(dwImageBytes > 0 && pbImageData)
                {
                    swprintf(textureName,64,L"%ls",sessionInfo->displayLabel);
                    registerSubstitutionTexture(textureName,pbImageData,dwImageBytes);
                }
            }

            m_buttonListGames.addItem( sessionInfo->displayLabel, textureName );

            if(memcmp( &selectedSessionId, &sessionInfo->sessionId, sizeof(SessionID) ) == 0)
            {
                m_buttonListGames.setCurrentSelection(sessionIndex);
                break;
            }
            ++sessionIndex;
        }
    }

    m_labelNoGames.setVisible(false);
    m_buttonListGames.addItem(L"Add External Server");
    if (filteredListSize == 0 || addServerWasSelected)
    {
        int selectedIndex = addServerWasSelected ? static_cast<int>(filteredListSize) : 0;
        m_buttonListGames.setCurrentSelection(selectedIndex);
        m_iGameListIndex = selectedIndex;
    }

	updateTooltips();
}

void UIScene_LoadOrJoinMenu::HandleDLCMountingComplete()
{
    Initialise();
}

bool UIScene_LoadOrJoinMenu::DoesSavesListHaveFocus()
{
	if( m_buttonListSaves.hasFocus() )
	{
		// check it's not the first or second element (new world or tutorial)
		if(m_iSaveListIndex > (m_iDefaultButtonsC-1))
		{
			return true;
		}
	}
	return false;
}

bool UIScene_LoadOrJoinMenu::DoesMashUpWorldHaveFocus()
{
	if(m_buttonListSaves.hasFocus())
	{
		// check it's not the first or second element (new world or tutorial)
		if(m_iSaveListIndex > (m_iDefaultButtonsC - 1))
		{
			return false;
		}

		if(m_iSaveListIndex > (m_iDefaultButtonsC - 1 - m_iMashUpButtonsC))
		{
			return true;
		}
		else return false;
	}
	else return false;
}

bool UIScene_LoadOrJoinMenu::DoesGamesListHaveFocus()
{
    return m_buttonListGames.hasFocus();
}

void UIScene_LoadOrJoinMenu::handleTimerComplete(int id)
{
    switch(id)
    {
    case JOIN_LOAD_ONLINE_TIMER_ID:
        {

            bool bMultiplayerAllowed = ProfileManager.IsSignedInLive( m_iPad ) && ProfileManager.AllowedToPlayMultiplayer(m_iPad);
            if(bMultiplayerAllowed != m_bMultiplayerAllowed)
            {
                if( bMultiplayerAllowed )
                {
                    // 					m_CheckboxOnline.SetEnable(TRUE);
                    // 					m_CheckboxPrivate.SetEnable(TRUE);
                }
                else
                {
                    m_bInParty = false;
                    m_buttonListGames.clearList();
                    m_controlJoinTimer.setVisible( true );
                    m_labelNoGames.setVisible( false );
                }

                m_bMultiplayerAllowed = bMultiplayerAllowed;
            }
        }
        break;
        // 4J-PB - Only Xbox will not have trial DLC patched into the game
    }

}

void UIScene_LoadOrJoinMenu::LoadSaveFromDisk(File *saveFile, ESavePlatform savePlatform /*= SAVE_FILE_PLATFORM_LOCAL*/)
{	
    // we'll only be coming in here when the tutorial is loaded now

    StorageManager.ResetSaveData();

    // Make our next save default to the name of the level
    StorageManager.SetSaveTitle(saveFile->getName().c_str());

    __int64 fileSize = saveFile->length();
    FileInputStream fis(*saveFile);
    byteArray ba(fileSize);
    fis.read(ba);
    fis.close();



    bool isClientSide = false;
    bool isPrivate = false;
    int maxPlayers = MINECRAFT_NET_MAX_PLAYERS;

    if( app.GetTutorialMode() )
    {
        isClientSide = false;
        maxPlayers = 4;
    }

    app.SetGameHostOption(eGameHostOption_GameType,GameType::CREATIVE->getId() );

    g_NetworkManager.HostGame(0,isClientSide,isPrivate,maxPlayers,0);

    LoadSaveDataThreadParam *saveData = new LoadSaveDataThreadParam(ba.data, ba.length, saveFile->getName());

    NetworkGameInitData *param = new NetworkGameInitData();
    param->seed = 0;
    param->saveData = saveData;
    param->settings = app.GetGameHostOption( eGameHostOption_All );
    param->savePlatform = savePlatform;

    g_NetworkManager.FakeLocalPlayerJoined();

    LoadingInputParams *loadingParams = new LoadingInputParams();
    loadingParams->func = &CGameNetworkManager::RunNetworkGameThreadProc;
    loadingParams->lpParam = (LPVOID)param;

    UIFullscreenProgressCompletionData *completionData = new UIFullscreenProgressCompletionData();
    completionData->bShowBackground=TRUE;
    completionData->bShowLogo=TRUE;
    completionData->type = e_ProgressCompletion_CloseAllPlayersUIScenes;
    completionData->iPad = DEFAULT_XUI_MENU_USER;
    loadingParams->completionData = completionData;

    ui.NavigateToScene(ProfileManager.GetPrimaryPad(),eUIScene_FullscreenProgress, loadingParams);
}


int UIScene_LoadOrJoinMenu::DeleteSaveDialogReturned(void *pParam,int iPad,C4JStorage::EMessageResult result)
{
    UIScene_LoadOrJoinMenu* pClass = (UIScene_LoadOrJoinMenu*)pParam;
    // results switched for this dialog

	// Check that we have a valid save selected (can get a bad index if the save list has been refreshed)
	bool validSelection= pClass->m_iDefaultButtonsC != 0 && pClass->m_iSaveListIndex >= pClass->m_iDefaultButtonsC;

    if(result==C4JStorage::EMessage_ResultDecline && validSelection) 
    {
        if(app.DebugSettingsOn() && app.GetLoadSavesFromFolderEnabled())
        {
            pClass->m_bIgnoreInput=false;
        }
        else
        {
			StorageManager.DeleteSaveData(&pClass->m_pSaveDetails->SaveInfoA[pClass->m_iSaveListIndex - pClass->m_iDefaultButtonsC], UIScene_LoadOrJoinMenu::DeleteSaveDataReturned, (LPVOID)pClass->GetCallbackUniqueId());
            pClass->m_controlSavesTimer.setVisible( true );
        }
    }
    else
    {
        pClass->m_bIgnoreInput=false;
    }

    return 0;
}

int UIScene_LoadOrJoinMenu::DeleteSaveDataReturned(LPVOID lpParam,bool bRes)
{
	ui.EnterCallbackIdCriticalSection();
    UIScene_LoadOrJoinMenu* pClass = (UIScene_LoadOrJoinMenu*)ui.GetSceneFromCallbackId((size_t)lpParam);

	if(pClass)
	{
		if(bRes)
		{
			// wipe the list and repopulate it
			pClass->m_iState=e_SavesRepopulateAfterDelete;		
		}
		else pClass->m_bIgnoreInput=false;

		pClass->updateTooltips();
	}
	ui.LeaveCallbackIdCriticalSection();
    return 0;
}


int UIScene_LoadOrJoinMenu::RenameSaveDataReturned(LPVOID lpParam,bool bRes)
{
    UIScene_LoadOrJoinMenu* pClass = (UIScene_LoadOrJoinMenu*)lpParam;

    if(bRes)
    {
        pClass->m_iState=e_SavesRepopulate;
    }
    else pClass->m_bIgnoreInput=false;

    pClass->updateTooltips();

    return 0;
}



int UIScene_LoadOrJoinMenu::SaveOptionsDialogReturned(void *pParam,int iPad,C4JStorage::EMessageResult result)
{
    UIScene_LoadOrJoinMenu* pClass = (UIScene_LoadOrJoinMenu*)pParam;

    // results switched for this dialog
    // EMessage_ResultAccept means cancel
    switch(result)
    {
    case C4JStorage::EMessage_ResultDecline:  // rename
        {
			pClass->m_bIgnoreInput=true;
            // bring up a keyboard
            wchar_t wSaveName[128];
            //CD - Fix - We must memset the SaveName
            ZeroMemory(wSaveName, 128 * sizeof(wchar_t) );
            mbstowcs(wSaveName, pClass->m_saveDetails[pClass->m_iSaveListIndex - pClass->m_iDefaultButtonsC].UTF8SaveName, strlen(pClass->m_saveDetails->UTF8SaveName)+1); // plus null
            LPWSTR ptr = wSaveName;
            RequestUIKeyboard(app.GetString(IDS_RENAME_WORLD_TITLE),wSaveName,(DWORD)0,25,&UIScene_LoadOrJoinMenu::KeyboardCompleteWorldNameCallback,pClass,C_4JInput::EKeyboardMode_Default);
        }
        break;

    case C4JStorage::EMessage_ResultThirdOption:  // delete -
        {
            // delete the save game
            // Have to ask the player if they are sure they want to delete this game
            UINT uiIDA[2];
            uiIDA[0]=IDS_CONFIRM_CANCEL;
            uiIDA[1]=IDS_CONFIRM_OK;
            ui.RequestMessageBox(IDS_TOOLTIPS_DELETESAVE, IDS_TEXT_DELETE_SAVE, uiIDA, 2, iPad,&UIScene_LoadOrJoinMenu::DeleteSaveDialogReturned,pClass, app.GetStringTable(),NULL,0,false);
        }
        break;


    case C4JStorage::EMessage_Cancelled:
    default:
        {
            // reset the tooltips
            pClass->updateTooltips();
            pClass->m_bIgnoreInput=false;
        }
        break;
    }
    return 0;
}

int UIScene_LoadOrJoinMenu::TexturePackDialogReturned(void *pParam,int iPad,C4JStorage::EMessageResult result)
{
    UIScene_LoadOrJoinMenu *pClass = (UIScene_LoadOrJoinMenu *)pParam;

    // Exit with or without saving
    if(result==C4JStorage::EMessage_ResultAccept) 
    {
        // we need to enable background downloading for the DLC
        XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
#if TO_BE_IMPLEMENTED
        ULONGLONG ullOfferID_Full;
        ULONGLONG ullIndexA[1];
        app.GetDLCFullOfferIDForPackID(pClass->m_initData->selectedSession->data.texturePackParentId,&ullOfferID_Full);


        if( result==C4JStorage::EMessage_ResultAccept ) // Full version
        {
            ullIndexA[0]=ullOfferID_Full;
            StorageManager.InstallOffer(1,ullIndexA,NULL,NULL);

        }
        else // trial version
        {
            DLC_INFO *pDLCInfo=app.GetDLCInfoForFullOfferID(ullOfferID_Full);
            ullIndexA[0]=pDLCInfo->ullOfferID_Trial;
            StorageManager.InstallOffer(1,ullIndexA,NULL,NULL);
        }
#endif



    }
    pClass->m_bIgnoreInput=false;
    return 0;
}


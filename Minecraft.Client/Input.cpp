#include "stdafx.h"
#include "Minecraft.h"
#include "GameMode.h"
#include "..\Minecraft.World\net.minecraft.world.entity.player.h"
#include "..\Minecraft.World\net.minecraft.world.level.h"
#include "..\Minecraft.World\net.minecraft.world.level.storage.h"
#include "Input.h"
#include "..\Minecraft.Client\LocalPlayer.h"
#include "Options.h"

Input::Input()
{
	xa = 0;
	ya = 0;
	wasJumping = false;
	jumping = false;
	sneaking = false;
	m_gamepadSneaking = false;

	lReset = false;
    rReset = false;
}

void Input::tick(LocalPlayer *player)
{
	// 4J Stu -  Assume that we only need one input class, even though the java has subclasses for keyboard/controller
	// This function is based on the ControllerInput class in the Java, and will probably need changed
	//OutputDebugString("INPUT: Beginning input tick\n");

	Minecraft *pMinecraft=Minecraft::GetInstance();
	int iPad=player->GetXboxPad();

	float controllerXA = 0.0f;
	float controllerYA = 0.0f;

	// 4J-PB minecraft movement seems to be the wrong way round, so invert x!
	if( pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LEFT) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_RIGHT) )
		controllerXA = -InputManager.GetJoypadStick_LX(iPad);
	
	if( pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_FORWARD) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_BACKWARD) )
		controllerYA = InputManager.GetJoypadStick_LY(iPad);

	float keyboardXA = 0.0f;
	float keyboardYA = 0.0f;
#ifdef _WINDOWS64
	if (iPad == 0 && Windows64KeyboardInput::IsCaptured() && Windows64KeyboardInput::IsKBMActive())
	{
		if (pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LEFT) ||
			pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_RIGHT))
			keyboardXA = Windows64KeyboardInput::GetMoveX();
		if (pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_FORWARD) ||
			pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_BACKWARD))
			keyboardYA = Windows64KeyboardInput::GetMoveY();
	}
#endif

	if (keyboardXA != 0.0f || keyboardYA != 0.0f)
	{
		xa = keyboardXA;
		ya = keyboardYA;
	}
	else
	{
		xa = controllerXA;
		ya = controllerYA;
	}

#ifndef _CONTENT_PACKAGE
	if (app.GetFreezePlayers())
	{
		xa = ya = 0.0f;
		player->abilities.flying = true;
	}
#endif
	
    if (!lReset)
    {
        if (xa*xa+ya*ya==0.0f)
        {
            lReset = true;
        }
        xa = ya = 0.0f;
    }

	// 4J - in flying mode, don't actually toggle sneaking
	if(!player->abilities.flying)
	{
		if((player->ullButtonsPressed&(1LL<<MINECRAFT_ACTION_SNEAK_TOGGLE)) && pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_SNEAK_TOGGLE))
		{
			m_gamepadSneaking=!m_gamepadSneaking;
		}
	}
	sneaking = m_gamepadSneaking;

	if (iPad == 0 && Windows64KeyboardInput::IsCaptured() && Windows64KeyboardInput::IsKBMActive() &&
		Windows64KeyboardInput::IsKeyDown(VK_SHIFT) && !player->abilities.flying) {
		sneaking = true;
	}

	if(sneaking)
	{
		xa*=0.3f;
		ya*=0.3f;
	}

    float turnSpeed = 50.0f;

	float tx = 0.0f;
	float ty = 0.0f;
	if( pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LOOK_LEFT) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LOOK_RIGHT) )
		tx = InputManager.GetJoypadStick_RX(iPad)*(((float)app.GetGameSettings(iPad,eGameSetting_Sensitivity_InGame))/100.0f); // apply sensitivity to look
	if( pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LOOK_UP) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LOOK_DOWN) )
		ty = InputManager.GetJoypadStick_RY(iPad)*(((float)app.GetGameSettings(iPad,eGameSetting_Sensitivity_InGame))/100.0f); // apply sensitivity to look
	
#ifndef _CONTENT_PACKAGE
	if (app.GetFreezePlayers())	tx = ty = 0.0f;
#endif

	// 4J: WESTY : Invert look Y if required.
	if ( app.GetGameSettings(iPad,eGameSetting_ControlInvertLook) )
	{
		ty = -ty;
	}

    if (!rReset)
    {
        if (tx*tx+ty*ty==0.0f)
        {
            rReset = true;
        }
        tx = ty = 0.0f;
    }
	player->interpolateTurn(tx * abs(tx) * turnSpeed, ty * abs(ty) * turnSpeed);
        
    //jumping = controller.isButtonPressed(0);

	// Mouse look (added after stick-based turn)
	if (iPad == 0 && Windows64KeyboardInput::IsCaptured() && Windows64KeyboardInput::IsKBMActive()) {
		float mouseSensitivity = static_cast<float>(app.GetGameSettings(iPad, eGameSetting_Sensitivity_InGame)) / 100.0f;
		float mouseLookScale = 1.0f;
		float mdx = Windows64KeyboardInput::GetMouseDeltaX() * mouseSensitivity * mouseLookScale;
		float mdy = -Windows64KeyboardInput::GetMouseDeltaY() * mouseSensitivity * mouseLookScale;
		if (app.GetGameSettings(iPad, eGameSetting_ControlInvertLook)) mdy = -mdy;
		player->interpolateTurn(mdx, mdy);
	}
	
	unsigned int jump = InputManager.GetValue(iPad, MINECRAFT_ACTION_JUMP);
	if( jump > 0 && pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_JUMP) )
		jumping = true;
	else
 		jumping = false;

	if (iPad == 0 && Windows64KeyboardInput::IsCaptured() && Windows64KeyboardInput::IsKBMActive() &&
		Windows64KeyboardInput::IsKeyDown(VK_SPACE) && pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_JUMP)) jumping = true;

#ifndef _CONTENT_PACKAGE
	if (app.GetFreezePlayers())	jumping = false;
#endif

#ifdef _WINDOWS64
	if (iPad == 0 && Windows64KeyboardInput::IsKeyPressed(VK_ESCAPE) &&
		Windows64KeyboardInput::IsCaptured())
	{
		Windows64KeyboardInput::SetCapture(false);
	}
#endif

	//OutputDebugString("INPUT: End input tick\n");
}
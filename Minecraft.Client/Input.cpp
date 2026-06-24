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

	// 4J-PB minecraft movement seems to be the wrong way round, so invert x!
	if( pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LEFT) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_RIGHT) )
		xa = -InputManager.GetJoypadStick_LX(iPad);
	else
		xa = 0.0f;
	
	if( pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_FORWARD) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_BACKWARD) )
		ya = InputManager.GetJoypadStick_LY(iPad);
	else
		ya = 0.0f;

	if (iPad == 0) {
		float movementDeltaX = 0.0f;
		float movementDeltaY = 0.0f;

		if (Windows64KeyboardInput::IsKeyDown('W')) movementDeltaY += 1.0f;
		if (Windows64KeyboardInput::IsKeyDown('S')) movementDeltaY -= 1.0f;
		if (Windows64KeyboardInput::IsKeyDown('A')) movementDeltaX += 1.0f;
		if (Windows64KeyboardInput::IsKeyDown('D')) movementDeltaX -= 1.0f;

		//normalize diagonal movement
		if (movementDeltaX != 0.0f && movementDeltaY != 0) {
			movementDeltaX *= 0.707f;
			movementDeltaY *= 0.707f;
		}

		if (pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_LEFT) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_RIGHT)) {
			xa = max(min(xa + movementDeltaX, 1.0f), -1.0f);
		}
		if (pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_FORWARD) || pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_BACKWARD)) {
			ya = max(min(ya + movementDeltaY, 1.0f), -1.0f);
		}
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

	if (iPad == 0 && Windows64KeyboardInput::IsKeyDown(VK_SHIFT) && !player->abilities.flying) {
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
	if (iPad == 0 && Windows64KeyboardInput::IsCaptured()) {
		float mouseSensitivity = 1.25f;
		float mdx = Windows64KeyboardInput::GetMouseDeltaX() * mouseSensitivity;
		float mdy = -Windows64KeyboardInput::GetMouseDeltaY() * mouseSensitivity;
		if (app.GetGameSettings(iPad, eGameSetting_ControlInvertLook)) mdy = -mdy;
		player->interpolateTurn(mdx, mdy);
	}
	
	unsigned int jump = InputManager.GetValue(iPad, MINECRAFT_ACTION_JUMP);
	if( jump > 0 && pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_JUMP) )
		jumping = true;
	else
 		jumping = false;

	if (iPad == 0 && Windows64KeyboardInput::IsKeyDown(VK_SPACE) && pMinecraft->localgameModes[iPad]->isInputAllowed(MINECRAFT_ACTION_JUMP)) jumping = true;

#ifndef _CONTENT_PACKAGE
	if (app.GetFreezePlayers())	jumping = false;
#endif

	//OutputDebugString("INPUT: End input tick\n");
}
#pragma once

//DrPerky - KeyboardAndMouse controls are taken from smartcmd, not a 1:1 paste but a remake using that as the original

#include <windows.h>

// HID usage page and usage for raw input registration
#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((USHORT)0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif

class Windows64KeyboardInput {
public:
	static void Init(HWND hWnd);
	static void ResetStates();
	static void MoveMouseToCenter();

	static void SetCapture(bool value);
	static bool IsCaptured();

	static void PreTick();
	static void PostTick();

	static void OnKeyEvent(WPARAM vk, bool isDown);
	static void OnMouseEvent(int button, bool isDown);

	static void OnRawMouseInput(LPARAM lParam);
	static void OnRawMouseWheel(int delta);

	static bool IsKeyDown(int vk);
	static bool IsKeyPressed(int vk);
	static bool IsKeyReleased(int vk);

	static bool IsMouseDown(int button);
	static bool IsMousePressed(int button);
	static bool IsMouseReleased(int button);

	static float GetMouseDeltaX();
	static float GetMouseDeltaY();
	static int GetScrollDelta();


private:

	static uint8_t m_keyStates[32];    //256 bits
	static uint8_t m_keyStatesOld[32]; //256 bits

	static uint8_t m_mouseStates[4];    // 32 bits
	static uint8_t m_mouseStatesOld[4]; // 32 bits

	static float m_mouseDeltaX;
	static float m_mouseDeltaY;

	static float m_mouseDeltaXAccum;
	static float m_mouseDeltaYAccum;

	static int m_scrollDelta;
	static int m_scrollDeltaAccum;

	static bool m_isCaptured;
	static HWND m_hWnd;
	static bool m_initialized;
};
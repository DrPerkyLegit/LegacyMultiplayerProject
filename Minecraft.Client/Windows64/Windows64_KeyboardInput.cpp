#include "stdafx.h"

#include "Windows64_KeyboardInput.h"

uint8_t Windows64KeyboardInput::m_keyStates[32];    //256 bits
uint8_t Windows64KeyboardInput::m_keyStatesOld[32]; //256 bits

uint8_t Windows64KeyboardInput::m_mouseStates[4];    // 32 bits
uint8_t Windows64KeyboardInput::m_mouseStatesOld[4]; // 32 bits

float Windows64KeyboardInput::m_mouseDeltaX = 0;
float Windows64KeyboardInput::m_mouseDeltaY = 0;

float Windows64KeyboardInput::m_mouseDeltaXAccum = 0;
float Windows64KeyboardInput::m_mouseDeltaYAccum = 0;

int Windows64KeyboardInput::m_scrollDelta = 0;
int Windows64KeyboardInput::m_scrollDeltaAccum = 0;

bool Windows64KeyboardInput::m_isCaptured = false;
HWND Windows64KeyboardInput::m_hWnd = nullptr;
bool Windows64KeyboardInput::m_initialized = false;

template <typename T>
void SwapHelper(T& destination, T& original, T newValue) {
	destination = original;
	original = newValue;
}

void Windows64KeyboardInput::Init(HWND hWnd) {
	memset(m_keyStates, 0, sizeof(m_keyStates));
	memset(m_keyStatesOld, 0, sizeof(m_keyStatesOld));
	memset(m_mouseStates, 0, sizeof(m_mouseStates));
	memset(m_mouseStatesOld, 0, sizeof(m_mouseStatesOld));

	Windows64KeyboardInput::m_hWnd = hWnd;
	Windows64KeyboardInput::m_initialized = true;

	// Register for raw mouse input
	RAWINPUTDEVICE rid;
	rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid.usUsage = HID_USAGE_GENERIC_MOUSE;
	rid.dwFlags = 0;
	rid.hwndTarget = hWnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

void Windows64KeyboardInput::ResetStates() {
	memset(m_keyStates, 0, sizeof(m_keyStates));
	memset(m_mouseStates, 0, sizeof(m_mouseStates));

	Windows64KeyboardInput::m_mouseDeltaXAccum = 0.0f;
	Windows64KeyboardInput::m_mouseDeltaYAccum = 0.0f;
	Windows64KeyboardInput::m_scrollDeltaAccum = 0;
}

void Windows64KeyboardInput::MoveMouseToCenter() {
	RECT rect;
	GetClientRect(m_hWnd, &rect);
	POINT center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
	ClientToScreen(m_hWnd, &center);
	SetCursorPos(center.x, center.y);
}

void Windows64KeyboardInput::SetCapture(bool value) {
	if (value == Windows64KeyboardInput::m_isCaptured) return;
	Windows64KeyboardInput::m_isCaptured = value;

	ShowCursor(!m_isCaptured);

	if (m_isCaptured) {
		RECT rect;
		GetClientRect(m_hWnd, &rect);
		POINT topLeft = { rect.left, rect.top };
		POINT bottomRight = { rect.right, rect.bottom };
		ClientToScreen(m_hWnd, &topLeft);
		ClientToScreen(m_hWnd, &bottomRight);
		RECT screenRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
		ClipCursor(&screenRect);
		MoveMouseToCenter();

		// Flush accumulated deltas so the snap-to-center doesn't cause a jump
		m_mouseDeltaXAccum = 0.0f;
		m_mouseDeltaYAccum = 0.0f;
	} else {
		ClipCursor(NULL);
	}
}

bool Windows64KeyboardInput::IsCaptured() {
	return Windows64KeyboardInput::m_isCaptured;
}

void Windows64KeyboardInput::PreTick() {
	if (!Windows64KeyboardInput::m_initialized) return;

	SwapHelper<float>(m_mouseDeltaX, m_mouseDeltaXAccum, 0.0f);
	SwapHelper<float>(m_mouseDeltaY, m_mouseDeltaYAccum, 0.0f);

	SwapHelper<int>(m_scrollDelta, m_scrollDeltaAccum, 0);

	if (m_isCaptured) {
		//push curcor to the center
	}
}

void Windows64KeyboardInput::PostTick() {
	if (!Windows64KeyboardInput::m_initialized) return;

	memcpy(m_keyStatesOld, m_keyStates, sizeof(m_keyStates));
	memcpy(m_mouseStatesOld, m_mouseStates, sizeof(m_mouseStates));
}

void Windows64KeyboardInput::OnKeyEvent(WPARAM vk, bool isDown) {
	if (!Windows64KeyboardInput::m_initialized) return;
	if (vk < 0 || vk > 256) return;

	if (isDown) {
		Windows64KeyboardInput::m_keyStates[(vk >> 3)] |= (1 << (vk & 7));
	} else {
		Windows64KeyboardInput::m_keyStates[(vk >> 3)] &= ~(1 << (vk & 7));
	}
}


void Windows64KeyboardInput::OnMouseEvent(int button, bool isDown) {
	if (!Windows64KeyboardInput::m_initialized) return;
	if (button < 0 || button > 32) return;

	if (isDown) {
		Windows64KeyboardInput::m_mouseStates[(button >> 3)] |= (1 << (button & 7));
	} else {
		Windows64KeyboardInput::m_mouseStates[(button >> 3)] &= ~(1 << (button & 7));
	}
}

void Windows64KeyboardInput::OnRawMouseInput(LPARAM lParam) {
	if (!Windows64KeyboardInput::m_initialized) return;
	if (!Windows64KeyboardInput::m_isCaptured) return;

	UINT dwSize = 0;
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

	BYTE* lpb = (BYTE*)_malloca(dwSize);
	if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) return;

	RAWINPUT* raw = (RAWINPUT*)lpb;
	if (raw->header.dwType == RIM_TYPEMOUSE) {
		if (raw->data.mouse.usFlags == MOUSE_MOVE_RELATIVE) {
			Windows64KeyboardInput::m_mouseDeltaXAccum += (float)raw->data.mouse.lLastX;
			Windows64KeyboardInput::m_mouseDeltaYAccum += (float)raw->data.mouse.lLastY;
		}
	}

	_freea(lpb);
}

void Windows64KeyboardInput::OnRawMouseWheel(int delta) {
	if (!Windows64KeyboardInput::m_initialized) return;
	Windows64KeyboardInput::m_scrollDeltaAccum += delta;
}

bool Windows64KeyboardInput::IsKeyDown(int vk) {
	if (vk < 0 || vk > 256) return false;

	return (Windows64KeyboardInput::m_keyStates[(vk >> 3)] & (1 << (vk & 7)));
}

bool Windows64KeyboardInput::IsKeyPressed(int vk) {
	if (vk < 0 || vk > 256) return false;

	int key = (vk >> 3);
	int index = (vk & 7);

	bool currentlyClicked = Windows64KeyboardInput::m_keyStates[key] & (1 << index);
	bool wasClicked = Windows64KeyboardInput::m_keyStatesOld[key] & (1 << index);

	return currentlyClicked && !wasClicked;
}

bool Windows64KeyboardInput::IsKeyReleased(int vk) {
	if (vk < 0 || vk > 256) return false;

	int key = (vk >> 3);
	int index = (vk & 7);

	bool currentlyClicked = Windows64KeyboardInput::m_keyStates[key] & (1 << index);
	bool wasClicked = Windows64KeyboardInput::m_keyStatesOld[key] & (1 << index);

	return !currentlyClicked && wasClicked;
}

bool Windows64KeyboardInput::IsMouseDown(int button) {
	if (button < 0 || button > 32) return false;

	return Windows64KeyboardInput::m_mouseStates[(button >> 3)] & (1 << (button & 7));
}

bool Windows64KeyboardInput::IsMousePressed(int button) {
	if (button < 0 || button > 32) return false;

	int key = (button >> 3);
	int index = (button & 7);

	bool currentlyClicked = Windows64KeyboardInput::m_mouseStates[key] & (1 << index);
	bool wasClicked = Windows64KeyboardInput::m_mouseStatesOld[key] & (1 << index);

	return currentlyClicked && !wasClicked;
}

bool Windows64KeyboardInput::IsMouseReleased(int button) {
	if (button < 0 || button > 32) return false;

	int key = (button >> 3);
	int index = (button & 7);

	bool currentlyClicked = Windows64KeyboardInput::m_mouseStates[key] & (1 << index);
	bool wasClicked = Windows64KeyboardInput::m_mouseStatesOld[key] & (1 << index);

	return !currentlyClicked && wasClicked;
}

float Windows64KeyboardInput::GetMouseDeltaX() { return m_mouseDeltaX; }
float Windows64KeyboardInput::GetMouseDeltaY() { return m_mouseDeltaY; }
int Windows64KeyboardInput::GetScrollDelta() { return m_scrollDelta; }


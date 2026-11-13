#include "stdafx.h"

#include "Windows64_KeyboardInput.h"

#include <algorithm>
#include <string>

namespace
{
	struct TextInputState
	{
		bool active = false;
		bool selectAll = false;
		std::wstring text;
		std::wstring originalText;
		size_t cursor = 0;
		UINT maxChars = 0;
		int (*completeCallback)(LPVOID, const bool) = nullptr;
		LPVOID callbackParam = nullptr;
		void (*previewCallback)(LPVOID, LPCWSTR) = nullptr;
		LPVOID previewParam = nullptr;
	};

	TextInputState g_textInput;
	bool g_consumeReturnCharacter = false;

	void UpdateTextInputPreview()
	{
		SetKeyboardTextResult(g_textInput.text.c_str());
		if (g_textInput.previewCallback != nullptr)
		{
			g_textInput.previewCallback(g_textInput.previewParam, g_textInput.text.c_str());
		}
	}

	void FinishTextInput(bool accepted)
	{
		if (!g_textInput.active) return;

		std::wstring result = accepted ? g_textInput.text : L"";
		std::wstring preview = accepted ? g_textInput.text : g_textInput.originalText;
		int (*completeCallback)(LPVOID, const bool) = g_textInput.completeCallback;
		LPVOID callbackParam = g_textInput.callbackParam;
		void (*previewCallback)(LPVOID, LPCWSTR) = g_textInput.previewCallback;
		LPVOID previewParam = g_textInput.previewParam;

		g_textInput.active = false;
		g_textInput.selectAll = false;
		g_textInput.text.clear();
		g_textInput.originalText.clear();
		g_textInput.cursor = 0;
		g_textInput.maxChars = 0;
		g_textInput.completeCallback = nullptr;
		g_textInput.callbackParam = nullptr;
		g_textInput.previewCallback = nullptr;
		g_textInput.previewParam = nullptr;

		SetKeyboardTextResult(result.c_str());
		if (previewCallback != nullptr)
		{
			previewCallback(previewParam, preview.c_str());
		}
		if (completeCallback != nullptr)
		{
			completeCallback(callbackParam, accepted);
		}
	}

	void DeleteSelectedText()
	{
		g_textInput.text.clear();
		g_textInput.cursor = 0;
		g_textInput.selectAll = false;
	}

	void DeleteCharacterBeforeCursor()
	{
		if (g_textInput.selectAll)
		{
			DeleteSelectedText();
			return;
		}
		if (g_textInput.cursor == 0) return;

		size_t start = g_textInput.cursor - 1;
		if (g_textInput.text[start] >= 0xDC00 && g_textInput.text[start] <= 0xDFFF &&
			start > 0 && g_textInput.text[start - 1] >= 0xD800 && g_textInput.text[start - 1] <= 0xDBFF)
		{
			--start;
		}
		g_textInput.text.erase(start, g_textInput.cursor - start);
		g_textInput.cursor = start;
		g_textInput.selectAll = false;
	}

	void DeleteCharacterAtCursor()
	{
		if (g_textInput.selectAll)
		{
			DeleteSelectedText();
			return;
		}
		if (g_textInput.cursor >= g_textInput.text.size()) return;

		size_t count = 1;
		if (g_textInput.text[g_textInput.cursor] >= 0xD800 && g_textInput.text[g_textInput.cursor] <= 0xDBFF &&
			g_textInput.cursor + 1 < g_textInput.text.size() &&
			g_textInput.text[g_textInput.cursor + 1] >= 0xDC00 && g_textInput.text[g_textInput.cursor + 1] <= 0xDFFF)
		{
			count = 2;
		}
		g_textInput.text.erase(g_textInput.cursor, count);
		g_textInput.selectAll = false;
	}
}

uint8_t Windows64KeyboardInput::m_keyStates[32];    //256 bits
uint8_t Windows64KeyboardInput::m_keyStatesOld[32]; //256 bits
uint8_t Windows64KeyboardInput::m_keyPressedStates[32];
uint8_t Windows64KeyboardInput::m_keyReleasedStates[32];
uint8_t Windows64KeyboardInput::m_keyPressedAccum[32];
uint8_t Windows64KeyboardInput::m_keyReleasedAccum[32];

uint8_t Windows64KeyboardInput::m_mouseStates[4];    // 32 bits
uint8_t Windows64KeyboardInput::m_mouseStatesOld[4]; // 32 bits
uint8_t Windows64KeyboardInput::m_mousePressedStates[4];
uint8_t Windows64KeyboardInput::m_mouseReleasedStates[4];
uint8_t Windows64KeyboardInput::m_mousePressedAccum[4];
uint8_t Windows64KeyboardInput::m_mouseReleasedAccum[4];

float Windows64KeyboardInput::m_mouseDeltaX = 0;
float Windows64KeyboardInput::m_mouseDeltaY = 0;

float Windows64KeyboardInput::m_mouseDeltaXAccum = 0;
float Windows64KeyboardInput::m_mouseDeltaYAccum = 0;

int Windows64KeyboardInput::m_scrollDelta = 0;
int Windows64KeyboardInput::m_scrollDeltaAccum = 0;

bool Windows64KeyboardInput::m_isCaptured = false;
bool Windows64KeyboardInput::m_isCursorLocked = false;
bool Windows64KeyboardInput::m_isCursorHidden = false;
bool Windows64KeyboardInput::m_cursorVisibilityHidden = false;
bool Windows64KeyboardInput::m_isKBMActive = true;
HWND Windows64KeyboardInput::m_hWnd = nullptr;
bool Windows64KeyboardInput::m_initialized = false;

void Windows64KeyboardInput::Init(HWND hWnd) {
	memset(m_keyStates, 0, sizeof(m_keyStates));
	memset(m_keyStatesOld, 0, sizeof(m_keyStatesOld));
	memset(m_keyPressedStates, 0, sizeof(m_keyPressedStates));
	memset(m_keyReleasedStates, 0, sizeof(m_keyReleasedStates));
	memset(m_keyPressedAccum, 0, sizeof(m_keyPressedAccum));
	memset(m_keyReleasedAccum, 0, sizeof(m_keyReleasedAccum));
	memset(m_mouseStates, 0, sizeof(m_mouseStates));
	memset(m_mouseStatesOld, 0, sizeof(m_mouseStatesOld));
	memset(m_mousePressedStates, 0, sizeof(m_mousePressedStates));
	memset(m_mouseReleasedStates, 0, sizeof(m_mouseReleasedStates));
	memset(m_mousePressedAccum, 0, sizeof(m_mousePressedAccum));
	memset(m_mouseReleasedAccum, 0, sizeof(m_mouseReleasedAccum));
	m_mouseDeltaX = 0.0f;
	m_mouseDeltaY = 0.0f;
	m_mouseDeltaXAccum = 0.0f;
	m_mouseDeltaYAccum = 0.0f;
	m_scrollDelta = 0;
	m_scrollDeltaAccum = 0;

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
	memset(m_keyStatesOld, 0, sizeof(m_keyStatesOld));
	memset(m_keyPressedStates, 0, sizeof(m_keyPressedStates));
	memset(m_keyReleasedStates, 0, sizeof(m_keyReleasedStates));
	memset(m_keyPressedAccum, 0, sizeof(m_keyPressedAccum));
	memset(m_keyReleasedAccum, 0, sizeof(m_keyReleasedAccum));
	memset(m_mouseStates, 0, sizeof(m_mouseStates));
	memset(m_mouseStatesOld, 0, sizeof(m_mouseStatesOld));
	memset(m_mousePressedStates, 0, sizeof(m_mousePressedStates));
	memset(m_mouseReleasedStates, 0, sizeof(m_mouseReleasedStates));
	memset(m_mousePressedAccum, 0, sizeof(m_mousePressedAccum));
	memset(m_mouseReleasedAccum, 0, sizeof(m_mouseReleasedAccum));
	m_mouseDeltaX = 0.0f;
	m_mouseDeltaY = 0.0f;
	Windows64KeyboardInput::m_mouseDeltaXAccum = 0.0f;
	Windows64KeyboardInput::m_mouseDeltaYAccum = 0.0f;
	Windows64KeyboardInput::m_scrollDelta = 0;
	Windows64KeyboardInput::m_scrollDeltaAccum = 0;
}

void Windows64KeyboardInput::MoveMouseToCenter() {
	if (!m_hWnd) return;
	RECT rect;
	GetClientRect(m_hWnd, &rect);
	POINT center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
	ClientToScreen(m_hWnd, &center);
	SetCursorPos(center.x, center.y);
}

void Windows64KeyboardInput::ClipCursorToWindow() {
	if (!m_hWnd) return;
	RECT rect;
	if (!GetClientRect(m_hWnd, &rect) || rect.right <= rect.left || rect.bottom <= rect.top) return;
	POINT topLeft = { rect.left, rect.top };
	POINT bottomRight = { rect.right, rect.bottom };
	ClientToScreen(m_hWnd, &topLeft);
	ClientToScreen(m_hWnd, &bottomRight);
	RECT screenRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
	ClipCursor(&screenRect);
}

void Windows64KeyboardInput::SetCapture(bool value) {
	if (value == Windows64KeyboardInput::m_isCaptured) return;
	Windows64KeyboardInput::m_isCaptured = value;

	if (m_isCaptured) {
		ClipCursorToWindow();
		MoveMouseToCenter();

		// Flush accumulated deltas so the snap-to-center doesn't cause a jump
		m_mouseDeltaXAccum = 0.0f;
		m_mouseDeltaYAccum = 0.0f;
		m_mouseDeltaX = 0.0f;
		m_mouseDeltaY = 0.0f;
	} else if (!m_isCursorLocked) {
		ClipCursor(nullptr);
	}
	UpdateCursorVisibility();
}

bool Windows64KeyboardInput::IsCaptured() {
	return Windows64KeyboardInput::m_isCaptured;
}

void Windows64KeyboardInput::SetCursorLock(bool value) {
	if (value == m_isCursorLocked) return;
	m_isCursorLocked = value;

	if (m_isCursorLocked) {
		ClipCursorToWindow();
		MoveMouseToCenter();
		m_mouseDeltaXAccum = 0.0f;
		m_mouseDeltaYAccum = 0.0f;
		m_mouseDeltaX = 0.0f;
		m_mouseDeltaY = 0.0f;
	} else if (!m_isCaptured) {
		ClipCursor(nullptr);
	}
	UpdateCursorVisibility();
}

bool Windows64KeyboardInput::IsCursorLocked() {
	return m_isCursorLocked;
}

void Windows64KeyboardInput::SetCursorHidden(bool value) {
	if (Windows64KeyboardInput::m_isCursorHidden == value) return;
	Windows64KeyboardInput::m_isCursorHidden = value;
	UpdateCursorVisibility();
}

void Windows64KeyboardInput::SetKBMActive(bool active) {
	m_isKBMActive = active;
}

bool Windows64KeyboardInput::IsKBMActive() {
	return m_isKBMActive;
}

void Windows64KeyboardInput::UpdateCursorVisibility() {
	const bool hideCursor = m_isCaptured || m_isCursorLocked || m_isCursorHidden;
	if (hideCursor != m_cursorVisibilityHidden) {
		if (hideCursor) {
			while (ShowCursor(FALSE) >= 0) {}
		} else {
			while (ShowCursor(TRUE) < 0) {}
		}
		m_cursorVisibilityHidden = hideCursor;
	}
}

void Windows64KeyboardInput::PreTick() {
	if (!Windows64KeyboardInput::m_initialized) return;

	m_mouseDeltaX += m_mouseDeltaXAccum;
	m_mouseDeltaY += m_mouseDeltaYAccum;
	m_mouseDeltaXAccum = 0.0f;
	m_mouseDeltaYAccum = 0.0f;

	m_scrollDelta += m_scrollDeltaAccum;
	m_scrollDeltaAccum = 0;

	if (m_isCaptured || m_isCursorLocked) {
		ClipCursorToWindow();
		MoveMouseToCenter();
	}

	memcpy(m_keyPressedStates, m_keyPressedAccum, sizeof(m_keyPressedStates));
	memset(m_keyPressedAccum, 0, sizeof(m_keyPressedAccum));
	memcpy(m_keyReleasedStates, m_keyReleasedAccum, sizeof(m_keyReleasedStates));
	memset(m_keyReleasedAccum, 0, sizeof(m_keyReleasedAccum));
	memcpy(m_mousePressedStates, m_mousePressedAccum, sizeof(m_mousePressedStates));
	memset(m_mousePressedAccum, 0, sizeof(m_mousePressedAccum));
	memcpy(m_mouseReleasedStates, m_mouseReleasedAccum, sizeof(m_mouseReleasedStates));
	memset(m_mouseReleasedAccum, 0, sizeof(m_mouseReleasedAccum));
}

void Windows64KeyboardInput::PostTick() {
	if (!Windows64KeyboardInput::m_initialized) return;

	memcpy(m_keyStatesOld, m_keyStates, sizeof(m_keyStates));
	memcpy(m_mouseStatesOld, m_mouseStates, sizeof(m_mouseStates));
}

void Windows64KeyboardInput::OnKeyEvent(WPARAM vk, bool isDown) {
	if (!Windows64KeyboardInput::m_initialized) return;
	if (vk >= 256) return;

	const int key = static_cast<int>(vk >> 3);
	const uint8_t mask = static_cast<uint8_t>(1 << (vk & 7));
	const bool wasDown = (m_keyStates[key] & mask) != 0;

	if (isDown) {
		if (!wasDown) m_keyPressedAccum[key] |= mask;
		m_keyStates[key] |= mask;
	} else {
		if (wasDown) m_keyReleasedAccum[key] |= mask;
		m_keyStates[key] &= static_cast<uint8_t>(~mask);
	}
}


void Windows64KeyboardInput::OnMouseEvent(int button, bool isDown) {
	if (g_textInput.active) return;
	if (!Windows64KeyboardInput::m_initialized) return;
	if (button < 0 || button >= 3) return;

	const int key = button >> 3;
	const uint8_t mask = static_cast<uint8_t>(1 << (button & 7));
	const bool wasDown = (m_mouseStates[key] & mask) != 0;

	if (isDown) {
		if (!wasDown) m_mousePressedAccum[key] |= mask;
		m_mouseStates[key] |= mask;
	} else {
		if (wasDown) m_mouseReleasedAccum[key] |= mask;
		m_mouseStates[key] &= static_cast<uint8_t>(~mask);
	}
}

void Windows64KeyboardInput::OnRawMouseInput(LPARAM lParam) {
	if (g_textInput.active) return;
	if (!Windows64KeyboardInput::m_initialized) return;
	if (!Windows64KeyboardInput::m_isCaptured && !Windows64KeyboardInput::m_isCursorLocked) return;

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
	if (g_textInput.active) return;
	if (!Windows64KeyboardInput::m_initialized) return;
	Windows64KeyboardInput::m_scrollDeltaAccum += delta / WHEEL_DELTA;
}

bool Windows64KeyboardInput::RequestTextInput(LPCWSTR text, UINT maxChars,
	int (*completeCallback)(LPVOID, const bool), LPVOID callbackParam,
	void (*previewCallback)(LPVOID, LPCWSTR), LPVOID previewParam)
{
	if (g_textInput.active)
	{
		app.DebugPrintf("Ignoring overlapping text input request.\n");
		SetKeyboardTextResult(L"");
		if (completeCallback != nullptr)
		{
			completeCallback(callbackParam, false);
		}
		return false;
	}

	g_consumeReturnCharacter = false;
	g_textInput.active = true;
	g_textInput.maxChars = std::min<UINT>(maxChars, 127);
	g_textInput.text = text != nullptr ? text : L"";
	if (g_textInput.text.size() > g_textInput.maxChars)
	{
		g_textInput.text.resize(g_textInput.maxChars);
	}
	g_textInput.originalText = g_textInput.text;
	g_textInput.selectAll = !g_textInput.text.empty();
	g_textInput.cursor = g_textInput.text.size();
	g_textInput.completeCallback = completeCallback;
	g_textInput.callbackParam = callbackParam;
	g_textInput.previewCallback = previewCallback;
	g_textInput.previewParam = previewParam;
	SetKBMActive(true);
	SetKeyboardTextResult(g_textInput.text.c_str());
	return true;
}

bool Windows64KeyboardInput::HandleTextInputKeyDown(WPARAM vk)
{
	if (!g_textInput.active) return false;

	if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && vk == 'A')
	{
		g_textInput.selectAll = true;
		return true;
	}

	switch (vk)
	{
	case VK_RETURN:
		FinishTextInput(true);
		g_consumeReturnCharacter = true;
		return true;
	case VK_ESCAPE:
		FinishTextInput(false);
		return true;
	case VK_LEFT:
		if (g_textInput.selectAll)
		{
			g_textInput.cursor = 0;
			g_textInput.selectAll = false;
		}
		else if (g_textInput.cursor > 0)
		{
			--g_textInput.cursor;
		}
		return true;
	case VK_RIGHT:
		if (g_textInput.selectAll)
		{
			g_textInput.cursor = g_textInput.text.size();
			g_textInput.selectAll = false;
		}
		else if (g_textInput.cursor < g_textInput.text.size())
		{
			++g_textInput.cursor;
		}
		return true;
	case VK_HOME:
		g_textInput.cursor = 0;
		g_textInput.selectAll = false;
		return true;
	case VK_END:
		g_textInput.cursor = g_textInput.text.size();
		g_textInput.selectAll = false;
		return true;
	case VK_DELETE:
		DeleteCharacterAtCursor();
		UpdateTextInputPreview();
		return true;
	default:
		return true;
	}
}

bool Windows64KeyboardInput::IsTextInputActive()
{
	return g_textInput.active;
}

bool Windows64KeyboardInput::HandleTextInputChar(wchar_t character)
{
	if (!g_textInput.active)
	{
		if (g_consumeReturnCharacter && character == L'\r')
		{
			g_consumeReturnCharacter = false;
			return true;
		}
		return false;
	}

	if (character == L'\r' || character == L'\n')
	{
		FinishTextInput(true);
		g_consumeReturnCharacter = character == L'\r';
		return true;
	}
	if (character == L'\b')
	{
		DeleteCharacterBeforeCursor();
		UpdateTextInputPreview();
		return true;
	}
	if (character < L' ')
	{
		return true;
	}

	if (g_textInput.selectAll)
	{
		DeleteSelectedText();
	}
	if (g_textInput.text.size() < g_textInput.maxChars)
	{
		g_textInput.text.insert(g_textInput.cursor, 1, character);
		++g_textInput.cursor;
		UpdateTextInputPreview();
	}
	return true;
}

void Windows64KeyboardInput::CancelTextInput()
{
	FinishTextInput(false);
}

bool Windows64KeyboardInput::IsKeyDown(int vk) {
	if (vk < 0 || vk >= 256) return false;

	return (Windows64KeyboardInput::m_keyStates[(vk >> 3)] & (1 << (vk & 7)));
}

bool Windows64KeyboardInput::IsKeyPressed(int vk) {
	if (vk < 0 || vk >= 256) return false;

	return (Windows64KeyboardInput::m_keyPressedStates[(vk >> 3)] & (1 << (vk & 7))) != 0;
}

bool Windows64KeyboardInput::IsKeyReleased(int vk) {
	if (vk < 0 || vk >= 256) return false;

	return (Windows64KeyboardInput::m_keyReleasedStates[(vk >> 3)] & (1 << (vk & 7))) != 0;
}

bool Windows64KeyboardInput::IsMouseDown(int button) {
	if (button < 0 || button >= 3) return false;

	return Windows64KeyboardInput::m_mouseStates[(button >> 3)] & (1 << (button & 7));
}

bool Windows64KeyboardInput::IsMousePressed(int button) {
	if (button < 0 || button >= 3) return false;

	return (Windows64KeyboardInput::m_mousePressedStates[(button >> 3)] & (1 << (button & 7))) != 0;
}

bool Windows64KeyboardInput::IsMouseReleased(int button) {
	if (button < 0 || button >= 3) return false;

	return (Windows64KeyboardInput::m_mouseReleasedStates[(button >> 3)] & (1 << (button & 7))) != 0;
}

float Windows64KeyboardInput::GetMouseDeltaX() {
	const float delta = m_mouseDeltaX;
	m_mouseDeltaX = 0.0f;
	return delta;
}

float Windows64KeyboardInput::GetMouseDeltaY() {
	const float delta = m_mouseDeltaY;
	m_mouseDeltaY = 0.0f;
	return delta;
}
int Windows64KeyboardInput::GetScrollDelta() {
	const int delta = m_scrollDelta;
	m_scrollDelta = 0;
	return delta;
}

float Windows64KeyboardInput::GetMoveX() {
	float movement = 0.0f;
	if (IsKeyDown('A')) movement += 1.0f;
	if (IsKeyDown('D')) movement -= 1.0f;
	return movement;
}

float Windows64KeyboardInput::GetMoveY() {
	float movement = 0.0f;
	if (IsKeyDown('W')) movement += 1.0f;
	if (IsKeyDown('S')) movement -= 1.0f;
	return movement;
}

bool Windows64KeyboardInput::HasAnyInput() {
	if (m_mouseDeltaX != 0.0f || m_mouseDeltaY != 0.0f || m_scrollDelta != 0)
		return true;

	for (int i = 0; i < sizeof(m_keyStates); ++i) {
		if (m_keyStates[i] || m_keyPressedStates[i] || m_keyReleasedStates[i])
			return true;
	}
	for (int i = 0; i < sizeof(m_mouseStates); ++i) {
		if (m_mouseStates[i] || m_mousePressedStates[i] || m_mouseReleasedStates[i])
			return true;
	}
	return false;
}


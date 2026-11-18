#include "stdafx.h"
#include "Screen.h"
#include "Button.h"
#include "GuiParticles.h"
#include "Tesselator.h"
#include "Textures.h"
#include "..\Minecraft.World\SoundTypes.h"
#include "..\Minecraft.World\SharedConstants.h"
#ifdef _WINDOWS64
#include "Windows64\Windows64_KeyboardInput.h"

namespace
{
    struct ChatTextInputRequest
    {
        Minecraft *minecraft;
        wstring appliedText;
    };

    ChatTextInputRequest *g_chatTextInputRequest = nullptr;

    bool IsRepeatableKey(int virtualKey)
    {
        return virtualKey == VK_BACK || virtualKey == VK_DELETE ||
            virtualKey == VK_LEFT || virtualKey == VK_RIGHT ||
            virtualKey == VK_UP || virtualKey == VK_DOWN;
    }
}
#endif
#include <ChatScreen.h>



Screen::Screen()	// 4J added
{
	minecraft = NULL;
	width = 0;
    height = 0;
	passEvents = false;
	font = NULL;
	particles = NULL;
	clickedButton = NULL;
}

void Screen::render(int xm, int ym, float a)
{
	AUTO_VAR(itEnd, buttons.end());
	for (AUTO_VAR(it, buttons.begin()); it != itEnd; it++)
	{
        Button *button = *it; //buttons[i];
        button->render(minecraft, xm, ym);
    }
}

void Screen::keyPressed(wchar_t eventCharacter, int eventKey)
{
	if (eventKey == Keyboard::KEY_ESCAPE)
	{
		minecraft->setScreen(NULL);
//    minecraft->grabMouse();	// 4J - removed
	}
}

wstring Screen::getClipboard()
{
	// 4J - removed
	return NULL;
}

void Screen::setClipboard(const wstring& str)
{
	// 4J - removed
}

void Screen::mouseClicked(int x, int y, int buttonNum)
{
    if (buttonNum == 0)
	{
		AUTO_VAR(itEnd, buttons.end());
		for (AUTO_VAR(it, buttons.begin()); it != itEnd; it++)
		{
            Button *button = *it; //buttons[i];
            if (button->clicked(minecraft, x, y))
			{
                clickedButton = button;
                minecraft->soundEngine->playUI(eSoundType_RANDOM_CLICK, 1, 1);
                buttonClicked(button);
            }
        }
    }
}

void Screen::mouseReleased(int x, int y, int buttonNum)
{
    if (clickedButton!=NULL && buttonNum==0)
	{
        clickedButton->released(x, y);
        clickedButton = NULL;
    }
}

void Screen::buttonClicked(Button *button)
{
}

void Screen::init(Minecraft *minecraft, int width, int height)
{
    particles = new GuiParticles(minecraft);
    this->minecraft = minecraft;
    this->font = minecraft->font;
    this->width = width;
    this->height = height;
    buttons.clear();
    init();
}

void Screen::setSize(int width, int height)
{
    this->width = width;
    this->height = height;
}

void Screen::init()
{
}

void Screen::updateEvents()
{
    mouseEvent();
    keyboardEvent();
}

void Screen::mouseEvent()
{
#ifdef _WINDOWS64
    HWND window = GetActiveWindow();
    POINT cursor = {};
    RECT client = {};
    if (window == NULL || !GetCursorPos(&cursor) || !ScreenToClient(window, &cursor) || !GetClientRect(window, &client) || client.right <= client.left || client.bottom <= client.top) return;

    const int mouseX = cursor.x * width / (client.right - client.left);
    const int mouseY = height - cursor.y * height / (client.bottom - client.top) - 1;
    for (int button = 0; button < 3; button++) {
        if (Windows64KeyboardInput::IsMousePressed(button)) mouseClicked(mouseX, mouseY, button);
        if (Windows64KeyboardInput::IsMouseReleased(button)) mouseReleased(mouseX, mouseY, button);
    }
#endif
}

void Screen::keyboardEvent()
{
#ifdef _WINDOWS64
    ChatScreen *chatScreen = dynamic_cast<ChatScreen *>(this);
    if (g_chatTextInputRequest != nullptr && chatScreen == nullptr)
        Windows64KeyboardInput::CancelTextInput();

    if (chatScreen != nullptr)
    {
        if (g_chatTextInputRequest == nullptr && !Windows64KeyboardInput::IsTextInputActive())
        {
            ChatTextInputRequest *request = new ChatTextInputRequest{ minecraft, L"" };
            g_chatTextInputRequest = request;
            Windows64KeyboardInput::RequestTextInput(L"", SharedConstants::maxChatLength,
                [](LPVOID parameter, const bool accepted) -> int
                {
                    ChatTextInputRequest *request = static_cast<ChatTextInputRequest *>(parameter);
                    if (g_chatTextInputRequest == request)
                        g_chatTextInputRequest = nullptr;

                    Screen *currentScreen = request->minecraft != nullptr ? request->minecraft->screen : nullptr;
                    if (dynamic_cast<ChatScreen *>(currentScreen) != nullptr &&
                        (accepted || (GetKeyState(VK_ESCAPE) & 0x8000) != 0))
                    {
                        currentScreen->keyPressed(0, accepted ? Keyboard::KEY_RETURN : Keyboard::KEY_ESCAPE);
                    }

                    delete request;
                    return 0;
                },
                request,
                [](LPVOID parameter, LPCWSTR text)
                {
                    ChatTextInputRequest *request = static_cast<ChatTextInputRequest *>(parameter);
                    Screen *currentScreen = request->minecraft != nullptr ? request->minecraft->screen : nullptr;
                    if (dynamic_cast<ChatScreen *>(currentScreen) == nullptr)
                        return;

                    wstring updatedText;
                    if (text != nullptr)
                    {
                        wstring wrappedText(text);
                        for (const wchar_t character : wrappedText)
                        {
                            if (SharedConstants::acceptableLetters.find(character) != wstring::npos &&
                                updatedText.length() < SharedConstants::maxChatLength)
                            {
                                updatedText += character;
                            }
                        }
                    }
                    size_t commonLength = 0;
                    while (commonLength < request->appliedText.length() &&
                        commonLength < updatedText.length() &&
                        request->appliedText[commonLength] == updatedText[commonLength])
                    {
                        ++commonLength;
                    }

                    for (size_t i = commonLength; i < request->appliedText.length(); ++i)
                        currentScreen->keyPressed(0, Keyboard::KEY_BACK);
                    for (size_t i = commonLength; i < updatedText.length(); ++i)
                        currentScreen->keyPressed(updatedText[i], -1);

                    request->appliedText = updatedText;
                },
                request);
        }
        return;
    }

    if (Windows64KeyboardInput::IsTextInputActive())
        return;

    static ULONGLONG lastKeyTime[256] = {};
    static bool hasStartedRepeating[256] = {};
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG repeatDelay = 400;
    const ULONGLONG repeatInterval = 50;

    for (int virtualKey = 0; virtualKey < 256; virtualKey++) {
        const bool keyDown = Windows64KeyboardInput::IsKeyDown(virtualKey);
        bool deliver = Windows64KeyboardInput::IsKeyPressed(virtualKey);
        if (!keyDown) {
            lastKeyTime[virtualKey] = 0;
            hasStartedRepeating[virtualKey] = false;
        } else if (deliver) {
            lastKeyTime[virtualKey] = now;
            hasStartedRepeating[virtualKey] = false;
        } else if (IsRepeatableKey(virtualKey) && lastKeyTime[virtualKey] != 0) {
            const ULONGLONG interval = hasStartedRepeating[virtualKey] ? repeatInterval : repeatDelay;
            if (now - lastKeyTime[virtualKey] >= interval) {
                deliver = true;
                lastKeyTime[virtualKey] = now;
                hasStartedRepeating[virtualKey] = true;
            }
        }
        if (!deliver) continue;

        int eventKey = -1;
        switch (virtualKey) {
        case VK_ESCAPE: eventKey = Keyboard::KEY_ESCAPE; break;
        case VK_RETURN: eventKey = Keyboard::KEY_RETURN; break;
        case VK_BACK: eventKey = Keyboard::KEY_BACK; break;
        case VK_UP: eventKey = Keyboard::KEY_UP; break;
        case VK_DOWN: eventKey = Keyboard::KEY_DOWN; break;
        case VK_TAB: eventKey = Keyboard::KEY_TAB; break;
        case VK_SHIFT: eventKey = Keyboard::KEY_LSHIFT; break;

        case VK_LEFT:
        case VK_RIGHT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
            eventKey = virtualKey;
        default: break;
        }

        if (eventKey != -1) {
            keyPressed(0, eventKey);
        }
    }
#endif
}

void Screen::tick()
{
}

void Screen::removed()
{
}

void Screen::renderBackground()
{
	renderBackground(0);
}

void Screen::renderBackground(int vo)
{
	if (minecraft->level != NULL)
	{
		fillGradient(0, 0, width, height, 0xc0101010, 0xd0101010);
	}
	else
	{
		renderDirtBackground(vo);
	}
}

void Screen::renderDirtBackground(int vo)
{
	// 4J Unused
#if 0
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    Tesselator *t = Tesselator::getInstance();
    glBindTexture(GL_TEXTURE_2D, minecraft->textures->loadTexture(L"/gui/background.png"));
    glColor4f(1, 1, 1, 1);
    float s = 32;
    t->begin();
    t->color(0x404040);
    t->vertexUV((float)(0), (float)( height), (float)( 0), (float)( 0), (float)( height / s + vo));
    t->vertexUV((float)(width), (float)( height), (float)( 0), (float)( width / s), (float)( height / s + vo));
    t->vertexUV((float)(width), (float)( 0), (float)( 0), (float)( width / s), (float)( 0 + vo));
    t->vertexUV((float)(0), (float)( 0), (float)( 0), (float)( 0), (float)( 0 + vo));
    t->end();
#endif
}

bool Screen::isPauseScreen()
{
	return true;
}

void Screen::confirmResult(bool result, int id)
{
}

void Screen::tabPressed()
{
}

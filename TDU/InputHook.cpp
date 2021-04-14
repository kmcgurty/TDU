#include "Hooks.h"
#include "Logger.h"
#include "Globals.h"
#include "Cheats.h"
#include "Chat.h"
#include "Teardown.h"

#include <detours.h>
#include <imgui.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC	WndProc;

typedef bool(*tSetCursorPos)(int X, int Y);
tSetCursorPos	oSetCursorPos;

bool hSetCursorPos(int X, int Y)
{
	if (Cheats::Menu::Enabled || Chat::inputOpen) {
		return true;
	}

	return oSetCursorPos(X, Y);
	
}

LRESULT	APIENTRY hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	#define VK_T 84
	#define VK_ESC 27

	switch (uMsg)
	{
		case WM_KEYDOWN:
			if (wParam == VK_INSERT)
			{
				Cheats::Menu::Enabled = !Cheats::Menu::Enabled;
				return true;
			}

			if (!Chat::inputOpen && wParam == VK_T)
			{
				Chat::inputOpen = true;
				Chat::focusInput = true;
			}

			if (Chat::inputOpen && wParam == VK_ESC) {
				Chat::inputOpen = false;
				return true;
			}

			//if (wParam == 0x4E && !Cheats::Menu::Enabled)
			//{
			//	Cheats::Noclip::Toggle();
			//	return true;
			//}
			break;
		case WM_KEYUP:
			CallWindowProc(WndProc, hWnd, uMsg, wParam, lParam);
			break;
		case WM_LBUTTONDOWN: case WM_LBUTTONUP:
			if (Chat::IO->WantCaptureMouse) {
				if (Chat::IO->MouseDrawCursor || Teardown::pGame->State <= 3) {
					if(uMsg == WM_LBUTTONUP)
						Chat::focusInput = true;
					Chat::inputOpen = true;
				}

				Chat::IO->MouseDown[0] = false;
			}
			else {
				Chat::inputOpen = false;
				Chat::IO->MouseDown[0] = false;

				CallWindowProc(WndProc, hWnd, uMsg, wParam, lParam);
			}

			break;
	}

	if (Cheats::Menu::Enabled || Chat::inputOpen && GetForegroundWindow() == Globals::HWnd)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	return CallWindowProc(WndProc, hWnd, uMsg, wParam, lParam);
}

void Hooks::InputHooks::HookWndProc()
{
	WndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Globals::HWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hWndProc)));
}

void Hooks::InputHooks::HookCursorPos()
{
	HMODULE hUSER32 = GetModuleHandle("USER32.dll");
	oSetCursorPos = (tSetCursorPos)GetProcAddress(hUSER32, "SetCursorPos");

	WriteLog(LogType::Address, "SetCursorPos: 0x%p | hook: 0x%p", SetCursorPos, hSetCursorPos);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)oSetCursorPos, hSetCursorPos);
	DetourTransactionCommit();
}
#include "Hooks.h"
#include "Logger.h"
#include "Globals.h"

#include "Chat.h"
#include "Game.h"

#include <detours.h>
#include <imgui.h>

typedef bool(*tSetCursorPos)(int X, int Y);
tSetCursorPos	oSetCursorPos;

bool hSetCursorPos(int X, int Y)
{
	if (!g_FreeCursor)
		return oSetCursorPos(X, Y);
	return true;
}

void Hooks::InputHooks::HookCursor()
{
	HMODULE hUSER32 = GetModuleHandle("USER32.dll");
	oSetCursorPos = (tSetCursorPos)GetProcAddress(hUSER32, "SetCursorPos");

	WriteLog(ELogType::Address, "SetCursorPos: 0x%p", SetCursorPos);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)oSetCursorPos, hSetCursorPos);
	DetourTransactionCommit();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC	WndProc;

LRESULT	APIENTRY hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	#define VK_T 84
	#define VK_ESC 27

	switch (uMsg)
	{
	case WM_KEYDOWN:
		if (!Chat::inputOpen) {
			if (wParam == VK_RETURN && g_Game->m_State == eGameState::Editor) {
				CallWindowProc(WndProc, hWnd, uMsg, wParam, lParam);
				return true;
			}

			if (wParam == VK_T || wParam == VK_RETURN)
			{
				g_FreeCursor = true;
				Chat::p_open = true;
				Chat::inputOpen = true;
				Chat::focusInput = true;
			}
		}
		else if (!Chat::focusInput) {
			Chat::focusInput = true;
		}

		if (Chat::inputOpen && wParam == VK_ESC) {
			Chat::inputOpen = false;
			g_FreeCursor = false;
			return true;
		}

		break;
	case WM_KEYUP:
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		CallWindowProc(WndProc, hWnd, uMsg, wParam, lParam);
		break;

	case WM_LBUTTONDOWN: case WM_LBUTTONUP:
		if (Chat::IO->WantCaptureMouse) {
			if (Chat::IO->MouseDrawCursor || g_Game->m_State <= 3) {
				if (!Chat::inputOpen)
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

	if (Chat::inputOpen && GetForegroundWindow() == g_Wnd)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	return CallWindowProc(WndProc, hWnd, uMsg, wParam, lParam);
}

void Hooks::InputHooks::HookWndProc()
{
	WndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_Wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hWndProc)));
}
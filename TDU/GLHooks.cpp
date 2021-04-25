#include "Hooks.h"
#include "Logger.h"
#include "Globals.h"
#include "Memory.h"
#include "Signatures.h"
#include "Cheats.h"
#include "Chat.h"

#include <detours.h>

#define GLEW_STATIC
#include <GL/glew.h>
#include <gl/GL.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_opengl3.h>



/*
	SwapBuffers hook
	- Used for rendering ImGui / menus
*/

typedef BOOL	(*twglSwapBuffers)		(_In_ HDC hDc);
twglSwapBuffers owglSwapBuffers;

bool hwglSwapBuffers(_In_ HDC hDc)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//Cheats::Menu::Draw();
	Chat::Draw();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	return owglSwapBuffers(hDc);
}

void Hooks::GLHooks::HookSwapBuffers()
{
	HMODULE OpenGL = GetModuleHandle("C:\\Windows\\System32\\opengl32.dll");
	owglSwapBuffers = (twglSwapBuffers)GetProcAddress(OpenGL, "wglSwapBuffers");
	WriteLog(LogType::Address, "wglSwapBuffers: 0x%p | hook: 0x%p", owglSwapBuffers, hwglSwapBuffers);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)owglSwapBuffers, hwglSwapBuffers);
	DetourTransactionCommit();
}

/*
	GlewInit hook
	- Used to reset glew and imgui, so that they still work when the game creates everything opengl related again
*/

typedef void(*tglewInit)	();
tglewInit oglewInit;

void resetImgui()
{
	// shutdown imgui just in case
	ImGui_ImplWin32_Shutdown();
	ImGui_ImplOpenGL3_Shutdown();

	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("data\\ui\\font\\Ubuntu-Regular.ttf", 16);

	const char* glsl_version = "#version 130";
	ImGui_ImplWin32_Init(Globals::HWnd);
	ImGui_ImplOpenGL3_Init(glsl_version);
}

void hglewInit()
{
	oglewInit();
	glewInit();

	resetImgui();
}

void Hooks::GLHooks::HookGlewInit()
{

	oglewInit = (tglewInit)Memory::FindPattern(Signatures::glewInit.pattern, Signatures::glewInit.mask, Globals::HModule);
	WriteLog(LogType::Address, "glewInit: 0x%p | hook: 0x%p", oglewInit, hglewInit);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)oglewInit, hglewInit);
	DetourTransactionCommit();
}
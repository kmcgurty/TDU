#include "Hooks.h"
#include "Logger.h"
#include "Globals.h"
#include "Memory.h"
#include "Signatures.h"

#include "Chat.h"
#include "Menus.h"

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
typedef BOOL(*twglSwapBuffers)		(_In_ HDC hDc);
twglSwapBuffers owglSwapBuffers;

bool hwglSwapBuffers(_In_ HDC hDc)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	Chat::Draw();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	return owglSwapBuffers(hDc);
}

void Hooks::GLHooks::HookSB()
{
	HMODULE OpenGL = GetModuleHandle("C:\\Windows\\System32\\opengl32.dll");
	owglSwapBuffers = (twglSwapBuffers)GetProcAddress(OpenGL, "wglSwapBuffers");
	WriteLog(ELogType::Address, "wglSwapBuffers: 0x%p", owglSwapBuffers);

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

void ResetImgui()
{
	// shutdown imgui just in case
	ImGui_ImplWin32_Shutdown();
	ImGui_ImplOpenGL3_Shutdown();

	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("data\\ui\\font\\Ubuntu-Regular.ttf", 16);

	const char* glsl_version = "#version 130";
	ImGui_ImplWin32_Init(g_Wnd);
	ImGui_ImplOpenGL3_Init(glsl_version);
}

void hglewInit()
{
	oglewInit();
	glewInit();

	ResetImgui();
}

void Hooks::GLHooks::HookGlewInit()
{
	oglewInit = (tglewInit)Memory::dwFindPattern(Signatures::glewInit);
	WriteLog(ELogType::Address, "glewInit: 0x%p", oglewInit);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)oglewInit, hglewInit);
	DetourTransactionCommit();
}
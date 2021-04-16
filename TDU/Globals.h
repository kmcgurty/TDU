#pragma once

#include <string>
#include <Windows.h>

namespace Globals
{
	inline std::string version("1.0.0");
	inline std::string UpdateURL = "https://chat.kmcgurty.com/latest";

	#if defined(_DEBUG)
	inline std::string WSuri = "localhost:9999";
	#else
	inline std::string WSuri = "chat.kmcgurty.com:9999";
	#endif

	inline HWND HWnd;
	inline HMODULE HModule;
	inline float FPS;
}
#pragma once

#include <iostream>
#include <functional>
#include <imgui.h>
#include "helper.h"

typedef std::function<void(std::vector<std::string>)> cmdCallback;

namespace Chat
{
	class Command {
	public:
		const char* commandName;
		const char* helptext;
		int numArgs;
		cmdCallback callback;
	};

	inline char commandPrefix = '/';


	#if defined(_DEBUG)
		inline const char* configFile = "chat_config_dev.json";
	#else
		inline const char* configFile = "chat_config.json";
	#endif

	inline std::vector<Chat::Command> Commands;
	inline bool inputOpen = false;
	inline bool focusInput = false;
	inline std::string uuid = "";
	inline ImGuiIO* IO;
	void Draw();
	void Draw2();
	void SetupImGuiStyle();
	void SendLocalData(std::string data);
	void SendLocalMessageUnformatted(const char* username, std::string message);
	void RegisterCommand(const char* commandName, cmdCallback callback, int numArgs, const char* helptext);
	void RunCommand(std::string commandName);
}
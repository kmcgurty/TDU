#pragma once

#include <iostream>
#include <functional>
#include "json.hpp"

namespace Helper
{
	void TextWithColors(const char* fmt, ...);
	std::string GenerateLocalMessage(const char* username, const char* message, std::vector<int> color);
	std::string GenerateWSMessage(const char* uuid, const char* message);
	std::string GenerateWSCommand(const char* uuid, const char* command, const char* commandData = "");
	bool PullConfig();
	void UpdateConfig();
	void RegisterCommands();
}
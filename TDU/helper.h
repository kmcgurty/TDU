#pragma once

#include <iostream>
#include <functional>
#include "json.hpp"

#include "imgui_internal.h"

struct Segment
{
    Segment(const char* text, ImU32 col = 0, bool underline = false, const char* url = "")
        : textStart(text)
        , textEnd(text + strlen(text))
        , colour(col)
        , underline(underline)
        , url(url)
    {}

    const char* textStart;
    const char* textEnd;
    ImU32		colour;
    bool		underline;
    const char* url;
};

namespace Helper
{
    std::string GenerateLocalMessage(const char* username, const char* message, std::vector<int> color);
	std::string GenerateWSMessage(const char* uuid, const char* message);
	std::string GenerateWSCommand(const char* uuid, const char* command, const char* commandData = "");
    std::vector<Segment> TextToSegments(std::string text);
    void RenderMultiLineText(std::vector<Segment> segs);
	void UpdateConfig();
	void RegisterCommands();
    void OpenURL(const char* url);
    bool IsValidHex(std::string const& s);
	bool PullConfig();
	bool CheckForUpdate();

}
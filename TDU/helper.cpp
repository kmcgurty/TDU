#include "helper.h"
#include "Chat.h"
#include "Websocket.h"
#include "Globals.h"

#include <imgui.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <regex>

using json = nlohmann::json;

const char ColorMarkerStart = '{';
const char ColorMarkerEnd = '}';

bool ProcessInlineHexColor(const char* start, const char* end, ImVec4& color)
{
    const int hexCount = (int)(end - start);
    if (hexCount == 6 || hexCount == 8)
    {
        char hex[9];
        strncpy(hex, start, hexCount);
        hex[hexCount] = 0;

        unsigned int hexColor = 0;
        if (sscanf(hex, "%x", &hexColor) > 0)
        {
            color.x = static_cast<float>((hexColor & 0x00FF0000) >> 16) / 255.0f;
            color.y = static_cast<float>((hexColor & 0x0000FF00) >> 8) / 255.0f;
            color.z = static_cast<float>((hexColor & 0x000000FF)) / 255.0f;
            color.w = 1.0f;

            if (hexCount == 8)
            {
                color.w = static_cast<float>((hexColor & 0xFF000000) >> 24) / 255.0f;
            }

            return true;
        }
    }

    return false;
}

void Helper::TextWithColors(const char* fmt, ...)
{
    char tempStr[4096];

    va_list argPtr;
    va_start(argPtr, fmt);
    _vsnprintf(tempStr, sizeof(tempStr), fmt, argPtr);
    va_end(argPtr);
    tempStr[sizeof(tempStr) - 1] = '\0';

    bool pushedColorStyle = false;
    const char* textStart = tempStr;
    const char* textCur = tempStr;
    while (textCur < (tempStr + sizeof(tempStr)) && *textCur != '\0')
    {
        if (*textCur == ColorMarkerStart)
        {
            // Print accumulated text
            if (textCur != textStart)
            {
                ImGui::TextUnformatted(textStart, textCur);
                ImGui::SameLine(0.0f, 0.0f);
            }

            // Process color code
            const char* colorStart = textCur + 1;
            do
            {
                ++textCur;
            }             while (*textCur != '\0' && *textCur != ColorMarkerEnd);

            // Change color
            if (pushedColorStyle)
            {
                ImGui::PopStyleColor();
                pushedColorStyle = false;
            }

            ImVec4 textColor;
            if (ProcessInlineHexColor(colorStart, textCur, textColor))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                pushedColorStyle = true;
            }

            textStart = textCur + 1;
        }
        else if (*textCur == '\n')
        {
            // Print accumulated text an go to next line
            ImGui::TextUnformatted(textStart, textCur);
            textStart = textCur + 1;
        }

        ++textCur;
    }

    if (textCur != textStart)
    {
        ImGui::TextUnformatted(textStart, textCur);
    }
    else
    {
        ImGui::NewLine();
    }

    if (pushedColorStyle)
    {
        ImGui::PopStyleColor();
    }
}

std::string Helper::GenerateWSMessage(const char* uuid, const char* message)
{
    json j = {
        {"info", {
            {"type", "message"}
        }},
        {"message", message},
        {"sender", {
            {"uuid", uuid}
        }}
    };

    return j.dump();
}

std::string Helper::GenerateLocalMessage(const char* username, const char* message, std::vector<int> color)
{
    json j = {
        {"message", message},
        {"sender", {
            {"username", username},
            {"color", color}
        }}
    };

    return j.dump();
}

std::string Helper::GenerateWSCommand(const char* uuid, const char* command, const char* commandData)
{
    json j = {
        {"info", {
            {"type", "command"}
        }},
        {"command", command},
        {"commandData", commandData},
        {"sender", {
            {"uuid", uuid}
        }}
    };

    return j.dump();
}

bool validUUID(std::string uuid) {
    std::regex expr("^[0-9a-f]{8}-[0-9a-f]{4}-[0-5][0-9a-f]{3}-[089ab][0-9a-f]{3}-[0-9a-f]{12}$");

    return std::regex_match(uuid, expr);
}

std::string getFileContents(const char*) {
    std::ifstream fileR(Chat::configFile);
    std::stringstream contentStream;
    std::string line;

    while (std::getline(fileR, line)) {
        contentStream << line << '\n';
    }
    
    fileR.close();

    return contentStream.str();
}

bool Helper::PullConfig()
{
    std::ofstream fileW;
    std::string contents = getFileContents(Chat::configFile);


    if (contents == "") {
        std::string defaultconf = "{\"uuid\": \"\"}";

        fileW.open(Chat::configFile);
        fileW << defaultconf;
        fileW.close();

        contents = defaultconf;
    }

    try {
        json parsed = json::parse(contents);

        if (validUUID(parsed["uuid"]) || parsed["uuid"] == "") {
            Chat::uuid = parsed["uuid"];
        }
        else {
            std::stringstream message;
            message << "The supplied UUID inside " << Chat::configFile << " is not valid. You will need to either delete the file to generate another one, or try to figure out why it is not valid. Unable to continue.";

            Chat::SendLocalMessageUnformatted("FATAL ERROR", message.str());
            return false;
        }
    }
    catch (const std::exception& e) {
        std::stringstream message;
        message << Chat::configFile;
        message << " is malformed! Please make sure it is valid JSON. You can also delete the file to generate a new one (your username will reset when you do this). Unable to continue.";

        Chat::SendLocalMessageUnformatted("FATAL ERROR", message.str());
        std::cout << e.what() << std::endl;
        return false;
    }

    return true;
}

void Helper::UpdateConfig() {
    std::ofstream fileW;

    json newConfig = {
        {"uuid", Chat::uuid}
    };

    fileW.open(Chat::configFile);
    fileW << newConfig.dump();
    fileW.close();
}

void Helper::RegisterCommands() {
    Chat::RegisterCommand("help", [](std::vector<std::string> args) {
        if (args.size() == 0) {
            std::stringstream message;

            message << "List of available commands: \n\n";

            for (int i = 0; i < Chat::Commands.size(); i++) {
                auto command = Chat::Commands[i];

                message << Chat::commandPrefix << command.commandName << " - " << command.helptext << "\n";
            }

            Chat::SendLocalMessageUnformatted("System", message.str());
        }
        else {
            for (int i = 0; i < Chat::Commands.size(); i++) {
                auto command = Chat::Commands[i];

                if (command.commandName == args[0]) {
                    std::stringstream message;
                    message << "Help for /\"" << Chat::Commands[i].commandName << "\": \n\n" << Chat::Commands[i].helptext;
                    Chat::SendLocalMessageUnformatted("System", message.str());
                    
                    return;
                }
            }

            std::stringstream message;
            message << "Help text for command \"" << args[0] << "\" was not found. Type \"" << Chat::commandPrefix << "help\" to view a list of available commands.";
            Chat::SendLocalMessageUnformatted("System", message.str());
        }
    }, -1, "Use with /help <command> to get help with a specific command. Also shows all of the available commands.");

    Chat::RegisterCommand("setuser", [](std::vector<std::string> args) {
        std::string newUsername = args[0];
        int maxLen = 11;

        if (newUsername.size() > maxLen || newUsername.size() < 0) {
            std::stringstream message;
            message << "Your username needs to be less than " << maxLen << " characters! \"" << newUsername << "\" is " << newUsername.size() << ".";
            Chat::SendLocalMessageUnformatted("System", message.str());

            return;
        }

        Websocket::Send(Helper::GenerateWSCommand(Chat::uuid.c_str(), "setuser", args[0].c_str()));
    }, 1, "Changes your username. /setuser <username>");

    Chat::RegisterCommand("reconnect", [](std::vector<std::string> args) {
        std::stringstream message;
        message << "Trying to reconnect to the server...";
        Chat::SendLocalMessageUnformatted("System", message.str());
        
        Websocket::Open(Globals::WSuri);
    }, -1, "Reconnects your client to the server.");
    //Chat::RegisterCommand("reconnect", "Reconnects to the chat server. Helpful if you've lose connection for some reason.");
}
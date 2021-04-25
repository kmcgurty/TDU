#include "Chat.h"
#include "helper.h"
#include "Websocket.h"
#include "Globals.h"

#include <imgui.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <regex>

using json = nlohmann::json;

bool Helper::IsValidHex(std::string const& s) {
    return s.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
}

//most of this function is from https://github.com/ocornut/imgui/issues/2313
//it saved me a ton of time. thank you!
void Helper::RenderMultiLineText(std::vector<Segment> segs) {
    const float wrapWidth = ImGui::GetWindowContentRegionWidth();
    for (int i = 0; i < segs.size(); ++i)
    {
        const char* textStart = segs[i].textStart;
        const char* textEnd = segs[i].textEnd ? segs[i].textEnd : textStart + strlen(textStart);

        ImFont* Font = ImGui::GetFont();

        do
        {
            float widthRemaining = ImGui::CalcWrapWidthForPos(ImGui::GetCursorScreenPos(), 0.0f);
            const char* drawEnd = Font->CalcWordWrapPositionA(1.0f, textStart, textEnd, wrapWidth, wrapWidth - widthRemaining);
            if (textStart == drawEnd){
                ImGui::NewLine();
                drawEnd = Font->CalcWordWrapPositionA(1.0f, textStart, textEnd, wrapWidth, wrapWidth - widthRemaining);
            }

            if (segs[i].colour)
                ImGui::PushStyleColor(ImGuiCol_Text, segs[i].colour);
            
            ImGui::TextUnformatted(textStart, textStart == drawEnd ? nullptr : drawEnd);
            
            if (segs[i].colour)
                ImGui::PopStyleColor();
            
            if (segs[i].underline){
                ImVec2 lineEnd = ImGui::GetItemRectMax();
                ImVec2 lineStart = lineEnd;
                lineStart.x = ImGui::GetItemRectMin().x;
                ImGui::GetWindowDrawList()->AddLine(lineStart, lineEnd, segs[i].colour);

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
                    ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
            }

            if (textStart == drawEnd || drawEnd == textEnd){
                ImGui::SameLine(0.0f, 0.0f);
                break;
            }

            textStart = drawEnd;

            while (textStart < textEnd){
                const char c = *textStart;
                if (ImCharIsBlankA(c)) { textStart++; }
                else if (c == '\n') { textStart++; break; }
                else { break; }
            }
        } while (true);
    }
}

//this function is a mess to read. Unfortunately, regex was not an option, so we manually check each character.
//checks for the pattern [text](data), to create markup urls, returns a vector of Segments
std::vector<Segment> Helper::TextToSegments(std::string text)
{
    struct _parser {
        struct _text {
            const char beginChar = '[';
            const char endChar = ']';
        } text;

        struct _url {
            const char beginChar = '(';
            const char endChar = ')';
        } url;

        int textBeginPos = -1;
        int textEndPos = -1;
        int modBeginPos = -1;
        int modEndPos = -1;
        int prevEndPos = 0;

        void resetPositions() {
            textBeginPos = -1;
            textEndPos = -1;
            modBeginPos = -1;
            modEndPos = -1;
        };

        void pushPositions(std::string _text, std::vector<Segment>* pushTo, bool end) {
            std::string preMod, postMod, modText, modData;

            //std::cout << "prevEndPos: " << prevEndPos << " | textBeginPos: " <<  textBeginPos << " | textEndPos: " << textEndPos << " | modBeginPos: " << modBeginPos << " | modEndPos: " << modEndPos << std::endl;

            if (!end) {
                preMod = _text.substr(prevEndPos, (double)textBeginPos - (double)prevEndPos - 1);
                modText = _text.substr(textBeginPos, textEndPos);
                modData = _text.substr(modBeginPos, modEndPos);

                if (preMod.length() > 0)
                    pushTo->push_back(Segment(_strdup(preMod.c_str())));

                if (modData.length() > 0 && modData.at(0) == '#') {
                    std::string hex = modData.substr(1);

                    int red = 255;
                    int green = 255;
                    int blue = 255;

                    if (Helper::IsValidHex(hex) && hex.length() == 6)
                        sscanf(hex.c_str(), "%02x%02x%02x", &red, &green, &blue);

                    pushTo->push_back(Segment(_strdup(modText.c_str()), IM_COL32(red, green, blue, 255), false, ""));
                }
                else {
                    pushTo->push_back(Segment(_strdup(modText.c_str()), IM_COL32(127, 127, 255, 255), true, _strdup(modData.c_str())));
                }
            }
            else {
                postMod = _text.substr(prevEndPos);

                if (postMod.length() > 0)
                    pushTo->push_back(Segment(_strdup(postMod.c_str())));
            }

            //std::cout << "preMod: " << preMod << " | modText: " << modText << " | modData: " << modData << std::endl;

            prevEndPos = modBeginPos + modEndPos + 1;
        }
    } parser;

    std::vector<Segment> segs;

    for (unsigned int i = 0; i < text.length(); i++) {
        char currChar = text.at(i);

        if (currChar == parser.text.beginChar) {
            parser.textBeginPos = i + 1;
        }

        if (currChar == parser.text.endChar) {
            if (parser.textBeginPos < 0 || (text.at(i + 1) != parser.url.beginChar)) {
                parser.resetPositions();
                continue;
            }

            parser.textEndPos = i - parser.textBeginPos;

        }

        if (currChar == parser.url.beginChar) {
            if (parser.textBeginPos < 0 || parser.textEndPos < 0) {
                parser.resetPositions();
                continue;
            }

            parser.modBeginPos = i + 1;

        }

        if (currChar == parser.url.endChar) {
            if (parser.textBeginPos < 0 || parser.textEndPos < 0 || parser.modBeginPos < 0) {
                parser.resetPositions();
                continue;
            }

            parser.modEndPos = i - parser.modBeginPos;
            parser.pushPositions(text, &segs, false);
        }
    }

    parser.pushPositions(text, &segs, true);

    return segs;
}

std::string Helper::GenerateWSMessage(const char* uuid, const char* message) {
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

std::string Helper::GenerateLocalMessage(const char* username, const char* message, std::vector<int> color) {
    json j = {
        {"message", message},
        {"sender", {
            {"username", username},
            {"color", color}
        }}
    };

    return j.dump();
}

std::string Helper::GenerateWSCommand(const char* uuid, const char* command, const char* commandData) {
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

std::string getFileContents(const char* fileName) {
    std::ifstream fileR(fileName);
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


std::string removeAllChar(std::string str, char ch) {
    for (int i = 0; i < str.length(); ++i) {
        if (str[i] == ch)
            str.erase(i, 1);
    }

    return str;
}

bool Helper::CheckForUpdate(){
    Chat::SendLocalMessageUnformatted("Updater", "Checking for updates...");
    std::string response = Websocket::GrabLatestVersion();

    try {
        json parsed = json::parse(response);

        std::string server_ver_string = parsed["client"]["dll"];
        std::string server_ver_cleaned = removeAllChar(server_ver_string, '.');

        int server_ver = std::stoi(server_ver_cleaned, nullptr, 10);

        std::string client_ver_cleaned = removeAllChar(Globals::version, '.');
        int client_ver = std::stoi(client_ver_cleaned, nullptr, 10);

        std::cout << "Server (int): " << server_ver << ", Client (int): " << client_ver << std::endl;

        if (client_ver < server_ver) {
            std::stringstream message;
            message << "Your chat client is out of date. Please update before continuing - you can visit https://teardownchat.com to download the latest version.\n\n" << "Your version: " << Globals::version << " - Latest version: " << server_ver_string;
            Chat::SendLocalMessageUnformatted("Updater", message.str());
            
            return false;
        }
        else {
            Chat::SendLocalMessageUnformatted("Updater", "You're up to date!");
            return true;
        }
    } catch (const std::exception& e) {
        std::stringstream message;
        message << "Response from the server was not valid JSON. This is where it broke: " << response;

        Chat::SendLocalMessageUnformatted("Updater", "Unable to check the server for client update. Check the console for the error message.");
        std::cout << e.what() << std::endl;
        return true;
    }
    return true;
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
                    message << "Help for \"" << Chat::Commands[i].commandName << "\": \n\n" << Chat::Commands[i].helptext;
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

    Chat::RegisterCommand("online", [](std::vector<std::string> args) {
        Websocket::Send(Helper::GenerateWSCommand(Chat::uuid.c_str(), "listonline"));
    }, -1, "Shows a list of users currently online.");
}
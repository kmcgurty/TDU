#include <vector>
#include <mutex>
#include <iomanip>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "globals.h"
#include "Chat.h"
#include "Websocket.h"

using json = nlohmann::json;

int MessageHistoryLimit = 200;
std::vector<json> Messages;
json config;

char InputBuf[200];
std::once_flag chatInitialized;
bool scrollToBottom = false;

static char* Strdup(const char* s) {IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
static void  Strtrim(char* s) { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

void Chat::SendLocalData(std::string data) {
	json parsed = json::parse(data);

	std::stringstream colorHex;

	//we ignore alpha channel at the moment
	int r = parsed["sender"]["color"][0].get<int>();
	int g = parsed["sender"]["color"][1].get<int>();
	int b = parsed["sender"]["color"][2].get<int>();

	colorHex << std::hex << std::setw(2) << std::setfill('0') << r;
	colorHex << std::hex << std::setw(2) << std::setfill('0') << g;
	colorHex << std::hex << std::setw(2) << std::setfill('0') << b;

	parsed["sender"]["color"] = colorHex.str();

	std::cout << parsed["sender"]["color"] << " - " << parsed["sender"]["username"] << ": " << parsed["message"] << std::endl;
	
	if (Messages.size() >= MessageHistoryLimit) {
		Messages.erase(Messages.begin());
	}

	Messages.push_back(parsed);
	scrollToBottom = true;
}

void Chat::SendLocalMessageUnformatted(const char* username, std::string message) {
	std::vector<int> color = { 255, 89, 89, 1};

	std::string data = Helper::GenerateLocalMessage(username, message.c_str(), color);
	Chat::SendLocalData(data);
}

void Chat::RegisterCommand(const char* commandName, cmdCallback callback, int numArgs, const char* helptext) {
	Chat::Command command;

	command.commandName = commandName;
	command.helptext = helptext;
	command.callback = callback;
	command.numArgs = numArgs;

	Chat::Commands.push_back(command);
}

void Chat::RunCommand(std::string message) {
	int spaceIndex = message.find(' ') - 1;
	
	bool hasArgs = true;

	if (spaceIndex == std::string::npos - 1) {
		spaceIndex = message.length();
		hasArgs = false;
	}

	std::string typedCommand = message.substr(1, spaceIndex);
	std::string unsplitArgs;
	std::vector<std::string> args;
	
	if (hasArgs) {
		unsplitArgs = message.substr(double(spaceIndex) + 2, message.length());
		boost::split(args, unsplitArgs, boost::is_any_of(" "), boost::token_compress_on);
	}

	for (int i = 0; i < Chat::Commands.size(); i++) {
		auto command = Chat::Commands[i];

		if (command.commandName == typedCommand) {
			if (command.callback) {
				if (args.size() == command.numArgs || command.numArgs == -1) {
					command.callback(args);
				}
				else if (args.size() > command.numArgs) {
					std::stringstream message;
					message << "Too many arguments. Command \"" << command.commandName << "\" requires " << command.numArgs << " arguments. You supplied " << args.size() << ".";
					Chat::SendLocalMessageUnformatted("System", message.str());
				}
				else if (args.size() < command.numArgs) {
					std::stringstream message;
					message << "Too few arguments. Command \"" << command.commandName << "\" requires " << command.numArgs << " arguments. You supplied " << args.size() << ".";
					Chat::SendLocalMessageUnformatted("System", message.str());
				}
			}
			else {
				std::cout << "Callback command for \"" << command.commandName << "\"does not exist! Unable to call callback." << std::endl;
			}

			return;
		}

		if (i >= Chat::Commands.size()-1) {
			std::stringstream message;
			message << "Command \"" << typedCommand << "\" not found." << std::endl;
			Chat::SendLocalMessageUnformatted("System", message.str());
		}
	}
}

void ChatInit() {
	Chat::SendLocalMessageUnformatted("System", "Initializing chat...");
	
	Chat::IO = &ImGui::GetIO();
	Chat::SetupImGuiStyle();

	if (!Helper::CheckForUpdate()) return;
	if (!Helper::PullConfig()) return;

	Helper::RegisterCommands();

	Websocket::Open(Globals::WSuri);
}

//unused atm, but needed
static int TextEditCallbackStub(ImGuiInputTextCallbackData* data) {	
	std::cout << "Message history has not been implemented yet." << std::endl;
	return 1;
}

void Chat::Draw2() {
	// And this is my test case
	if (ImGui::Begin("Text Test", nullptr, ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::TextColored(ImColor(0, 255, 0, 255), "Half-manual wrapping");
		
		std::vector<Segment> segs;
		segs.push_back(Segment("this is a really super duper long segment that should wrap all on its own "));
		segs.push_back(Segment("http://google.com", IM_COL32(127, 127, 255, 255), true));
		segs.push_back(Segment(" Short text "));
		segs.push_back(Segment("http://github.com", IM_COL32(127, 127, 255, 255), true));
		
		Helper::RenderMultiLineText(segs);

		ImGui::NewLine();

		ImGui::Separator();
		ImGui::TextColored(ImColor(0, 255, 0, 255), "Broken native wrapping");

		ImGui::PushTextWrapPos(ImGui::GetContentRegionAvailWidth());
		for (int i = 0; i < sizeof(segs) / sizeof(segs[0]); ++i)
		{
			ImGui::TextUnformatted(segs[i].textStart, segs[i].textEnd);
			ImGui::SameLine();
		}
		ImGui::PopTextWrapPos();
	}
	ImGui::End();
}

void Chat::Draw()
{
	std::call_once(chatInitialized, ChatInit);

	Chat::IO->MouseDrawCursor = Chat::inputOpen;

	//ImGui::ShowDemoWindow();

	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

	if (Chat::inputOpen) {
		colors[ImGuiCol_WindowBg] = ImVec4(0.180f, 0.180f, 0.180f, 1.000f);
	}
	else {
		colors[ImGuiCol_WindowBg] = ImVec4(0.180f, 0.180f, 0.180f, 0.650f);
	}

	std::string title = "Teardown Chat - v" + Globals::version;

	ImGui::SetNextWindowSize(ImVec2(466, 277), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(30, Chat::IO->DisplaySize.y / 2 + 50), ImGuiCond_FirstUseEver);

	ImGui::Begin(title.c_str(), 0, ImGuiWindowFlags_NoCollapse);

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	const float messageSpacing = 8.0f;

	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false);
	for (int i = 0; i < Messages.size(); i++)
	{
		std::stringstream messageStream;

		messageStream << Messages[i]["sender"]["username"] << "\n" << Messages[i]["message"];

		std::string message = Messages[i]["message"];
		std::string username = Messages[i]["sender"]["username"];
		std::string color = Messages[i]["sender"]["color"];
		std::string formattedUsername = "[" + username + "](#" + color + ")";
		std::string formattedTimestamp = "";

		const float usernameWidth = ImGui::CalcTextSize(username.c_str(), NULL).x;

		if (Messages[i].contains("info") && Messages[i]["info"].contains("timestamp")) {
			std::string timestamp = Messages[i]["info"]["timestamp"];
			formattedTimestamp = " [" + timestamp + "](#CDCDCD): ";
		} else {
			formattedUsername += ": ";
		}

		Helper::RenderMultiLineText(Helper::TextToSegments(formattedUsername));
		Helper::RenderMultiLineText(Helper::TextToSegments(formattedTimestamp));
		ImGui::NewLine();
		//ImGui::SameLine(messageSpacing);
		Helper::RenderMultiLineText(Helper::TextToSegments(message));
		ImGui::NewLine();
	}

	if (scrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		scrollToBottom = false;
	}

	ImGui::EndChild();

	ImGui::Separator();

	if (Chat::inputOpen) {
		ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText("", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, &TextEditCallbackStub))
		{

			char* s = InputBuf;
			Strtrim(s);

			if (s[0]) {
				const char* message = Strdup(s);

				if (s[0] == Chat::commandPrefix)
					Chat::RunCommand(message);
				else
					Websocket::Send(Helper::GenerateWSMessage(Chat::uuid.c_str(), message));
			}
			Chat::inputOpen = false;
			strcpy(s, "");
		}
		if (Chat::focusInput) {
			//ImGui::SetKeyboardFocusHere(-1);
			Chat::focusInput = false;
		}
	}

	ImGui::End();
}
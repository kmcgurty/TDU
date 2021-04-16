#define WIN32_LEAN_AND_MEAN
#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "httplib.h"

#include "CLuaFunctions.h"
#include "Teardown.h"

#include "client_wss.hpp"
#include "Websocket.h"
#include "Chat.h"
#include "helper.h"
#include "json.hpp"
#include <boost/thread.hpp>



#if defined(_DEBUG)
    #include "client_ws.hpp"
    using WssClient = SimpleWeb::SocketClient<SimpleWeb::WS>;
    std::shared_ptr<WssClient::Connection> WSConnection = 0;
#else
    #include "client_wss.hpp"

    using WssClient = SimpleWeb::SocketClient<SimpleWeb::WSS>;
    std::shared_ptr<WssClient::Connection> WSConnection = 0;
#endif

using json = nlohmann::json;

boost::thread WSThread;

void handle_message(std::shared_ptr<WssClient::Connection> connection, std::shared_ptr<WssClient::InMessage> in_message) {
    std::string data = in_message->string();

    json parsed = json::parse(data);

    std::string dataType = parsed["info"]["type"].get<std::string>();

    if (dataType == "message") {
        Chat::SendLocalData(data);
    }
    else if(dataType == "command") {
        std::string commandName = parsed["command"].get<std::string>();

        if (commandName == "setuuid") {
            Chat::uuid = parsed["commandData"];
            Helper::UpdateConfig();
        }
    }
    else {
        std::cout << "Received data isn't a message or command: " << data << std::endl;
    }
}

void handle_open(std::shared_ptr<WssClient::Connection> connection) {
    WSConnection = connection;

    std::string message = "Connected to server " + Globals::WSuri;
    Chat::SendLocalMessageUnformatted("System", message);

    if (Chat::uuid == "") {
        Websocket::Send(Helper::GenerateWSCommand(Chat::uuid.c_str(), "registeruser"));
    }
    else {
        Websocket::Send(Helper::GenerateWSCommand(Chat::uuid.c_str(), "checkin"));
    }
}

void handle_close(std::shared_ptr<WssClient::Connection> connection, int status, const std::string& reason) {
    std::cout << "Closed connection with status code " << status << ". Pushing WSClose event." << std::endl;
    WSConnection = 0;
    

}

void handle_error(std::shared_ptr<WssClient::Connection> connection, const SimpleWeb::error_code& ec) {
    std::cout << "Client: Error: " << ec << ", error message: " << ec.message() << std::endl;

    handle_close(connection, 0, "Client error");
    Chat::SendLocalMessageUnformatted("SYSTEM ERROR", ec.message());

    WSConnection = 0;
}

void openWS(std::string URI) {
    boost::this_thread::sleep(boost::posix_time::seconds(5));

    std::cout << "Trying to connect with URI:" << URI << std::endl;


    #if defined(_DEBUG)
        WssClient client(URI);
    #else
        WssClient client(URI, false);
    #endif

    client.on_message = handle_message;
    client.on_open = handle_open;
    client.on_close = handle_close;
    client.on_error = handle_error;

    client.start();
}

void Websocket::Open(std::string URI) {
    boost::thread WSThread(openWS, URI);
}

void Websocket::Send(std::string data) {
    std::cout << "sending: " << data << std::endl;

    if (WSConnection) {
        WSConnection->send(data);
    }
    else {
        Chat::SendLocalMessageUnformatted("System", "Unable to send message. Are you connected to the internet, or is the server down?");
    }
}

std::string Websocket::GrabLatestVersion() {
    try {
        std::regex url_regex("^(http://)?([^/]+)(/?.*/?)(/.*)$");
        std::smatch matches;

        std::string url = Globals::UpdateURL;

        if (std::regex_search(url, matches, url_regex)) {
            std::stringstream urlstream;
            urlstream << matches[1] << matches[2] << matches[3];

            std::string base = urlstream.str();
            std::string path = matches[4].str();

            httplib::Client cli(base.c_str());
            auto res = cli.Get(path.c_str());
            
            if (res) {
                return res->body;
            }
            else {
                std::cout << "cli malformed" << std::endl;
            }

            return "ERROR - RES WAS NOT VALID";
        }
        else {
            std::cout << "Match not found\n";
        }

        return "ERROR - NO REGEX MATCH";
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    return "ERROR - WHAT THE HELL HAPPENED?";
}
#pragma once

#include <iostream>

namespace Websocket
{
	void Open(std::string URI);
	void Send(std::string data);
	std::string GrabLatestVersion();
}
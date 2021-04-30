#include "Teardown.h"
#include "multiplayer.h"

#include <regex>

int8_t lastState = 0;

void Multiplayer::GameStateListener() {
	int8_t currState = Teardown::pGame->State;

	if (lastState != currState) {
		lastState = currState;
		std::cout << "Game state change: " << (int)currState << std::endl;

		if (currState == Teardown::gameState::playing) {
			std::regex endOfPathExpr("((?:/[^/]+){2}(?=$))");
			std::smatch match;
			std::string fullPath = Teardown::pGame->levelXMLPath.c_str();

			std::regex_search(fullPath, match, endOfPathExpr);

			std::string currLevel = match.str(0);

			if (currLevel.size() > 0) {
				std::cout << "Current level: " << currLevel << std::endl;
			}
		}
	}
}
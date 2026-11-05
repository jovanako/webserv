#include "ConfigParser.hpp"
#include "ServerManager.hpp"
#include <iostream>

int main(int argc, char **argv) {
    ConfigParser parser;
	const std::vector<ServerConfig>& servers =  parser.parse();
	ServerManager serverManager(servers);

    return 0;
}
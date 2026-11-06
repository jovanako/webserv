#include "ConfigParser.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <csignal>

volatile sig_atomic_t g_running = 1;

static void handleSignal(int sig) {
	(void)sig;
	g_running = 0;
}

static void setupSignals() {
	std::signal(SIGPIPE, SIG_IGN);
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);
	std::signal(SIGQUIT, handleSignal);
}

int main(int argc, char **argv) {

    ConfigParser parser;
	const std::vector<ServerConfig>& servers =  parser.parse();
	ServerManager serverManager(servers);

    return 0;
}
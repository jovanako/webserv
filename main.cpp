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

int main(int argc, char **argv, char **envp) {
	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [configuration_file]" << std::endl;
		return 1;
	}
	setupSignals();
	try {
		std::string configPath = "webserv.conf";
		if (argc == 2) {
			configPath = argv[1];
		}
		ConfigParser parser(configPath, envp);
		
		std::cout << "Parsing config file at " << parser.getConfigFilePath() << std::endl;
		const std::vector<ServerConfig>& servers = parser.parse();
		std::cout << "Starting server listeners" << std::endl;
		ServerManager serverManager(servers);
		serverManager.run(g_running);
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	return 0;
}
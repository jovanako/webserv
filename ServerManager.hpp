#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include "ServerConfig.hpp"
#include "Client.hpp"
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <netdb.h>

class ServerManager {
	private:
		std::vector<ServerConfig>	_servers;
		std::vector<struct pollfd>	_pollFds;
		std::map<int, Client>		_clients;
		std::map<int, ServerConfig*> _listenSockets;

		void acceptClient(int listenFd);
		void removeClient(int clientFd);
		void initServers();
	public:
		ServerManager();
		ServerManager(const std::vector<ServerConfig>& servers);
		ServerManager(const ServerManager& other);
		ServerManager& operator=(const ServerManager& other);
		~ServerManager();
		
		void run();
};

#endif
#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include "ServerConfig.hpp"
#include "Client/Client.hpp"
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <netdb.h>
#include <cerrno>
#include <iostream>
#include <set>

class ServerManager {
	private:
		static const int			POLL_TIMEOUT_MS = 1000;

		std::vector<ServerConfig>	_servers;
		std::vector<struct pollfd>	_pollFds;
		std::map<int, Client>		_clients;
		std::map<int, size_t>		_listenSockets;
		std::map<int, int>			_cgiPipes;

		void acceptClient(int listenFd, std::vector<struct pollfd>& pendingFds);
		int acceptConnection(int listenFd, std::string& clientIp);
		void setClientNonBlocking(int clientFd);
		void registerClientSocket(int clientFd, std::vector<struct pollfd>& pendingFds);
		void configureClient(Client& client, int listenFd, const std::string& clientIp);
		void removeClient(int clientFd);
		void registerCgiPipe(int pipeFd, int clientFd, std::vector<struct pollfd>& pendingFds);
		void unregisterCgiPipe(int pipeFd);
		void unregisterClientCgiPipes(int clientFd);
		void syncClientPollEventsByFd(int clientFd, const Client& client);
		bool handleCgiPipeEvent(size_t pollIndex);
		void checkCgiTimeouts();
		bool handleClientEvent(size_t pollIndex, std::vector<struct pollfd>& pendingFds);
		bool handleClientInput(size_t pollIndex, int clientFd, Client& client);
		bool removeCompletedClient(int clientFd, Client& client);
		void processClient(int clientFd, Client& client, std::vector<struct pollfd>& pendingFds);
		void writeClientResponse(size_t pollIndex, Client& client);
		void updateClientPollEvents(size_t pollIndex, const Client& client);
		void initServers();
		int createListeningSocket(const ServerConfig& server,
			struct addrinfo*& serverInfo);
		bool setListeningSocketNonBlocking(int listenFd,
			const std::string& host, int port);
		bool bindListeningSocket(int listenFd, struct addrinfo* serverInfo,
			const std::string& host, const std::string& portStr);
		void enableAddressReuse(int listenFd);
		bool startListening(int listenFd);
		bool configureListeningSocket(int listenFd, struct addrinfo* serverInfo,
			const std::string& host, int port, const std::string& portStr);
		void registerListeningSocket(int listenFd, size_t serverIndex,
			const std::string& host, int port,
			std::set<std::pair<std::string, int> >& boundAddresses);
		bool waitForEvents();
		void addPendingClients(std::vector<struct pollfd>& pendingFds);
		bool handlePollEvent(size_t pollIndex, std::vector<struct pollfd>& pendingFds);
		void processPollEvents(std::vector<struct pollfd>& pendingFds);
		
	public:
		ServerManager();
		ServerManager(const std::vector<ServerConfig>& servers);
		ServerManager(const ServerManager& other);
		ServerManager& operator=(const ServerManager& other);
		~ServerManager();
		
		void run();
};

#endif
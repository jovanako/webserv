#include "ServerManager.hpp"

ServerManager::ServerManager() {}

ServerManager::ServerManager(const ServerManager& other) {
	*this = other;
}

ServerManager& ServerManager::operator=(const ServerManager& other) {
	if (this != &other) {
		_servers = other._servers;
		_pollFds = other._pollFds;
		_clients = other._clients;
		_listenSockets = other._listenSockets;
	}
	return *this;
}

ServerManager::~ServerManager() {}

void ServerManager::acceptClient(int listenFd) {

}

void ServerManager::removeClient(int clientFd) {

}

void ServerManager::initServers() {
	for (size_t i = 0; i < _servers.size(); ++i) {
		struct addrinfo hints, *serverInfo;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET; // IPv4
		hints.ai_socktype = SOCK_STREAM; // TCP

		std::string host = _servers[i].getHost();
		std::string port = _servers[i].getPortString();

		if (getaddrinfo(host.c_str(), port.c_str(), &hints, &serverInfo) != 0) {
			continue; // or handle error using gai_strerror()
		}

		int listenFd = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
		if (listenFd < 0) {
			freeaddrinfo(serverInfo);
			continue;
		}
		
		// allow immediate reuse of the port	
		int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		// enforce non-blocking mode
		fcntl(listenFd, F_SETFL, O_NONBLOCK);

		if (bind(listenFd, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
			close(listenFd);
			freeaddrinfo(serverInfo);
			continue;
		}

		freeaddrinfo(serverInfo);

		if (listen(listenFd, 128) < 0) {
			close(listenFd);
			continue;
		}

		// Map the new socket to its configuration so acceptClient() can pass it to the Client
		_listenSockets[listenFd] = &_servers[i];

		// Register the listening socket in the dynamic poll vector
		struct pollfd pfd;
		pfd.fd = listenFd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_pollFds.push_back(pfd);
	}
}

void ServerManager::run() {
	
}
#include "ServerManager.hpp"

ServerManager::ServerManager() {}

ServerManager::ServerManager(const std::vector<ServerConfig>& servers)
	: _servers(servers) {}	

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
	int clientFd = accept(listenFd, NULL, NULL); // check NULL if correct
	if (clientFd < 0)
		return; // handle error or EAGAIN
	
	fcntl(clientFd, F_SETFL, O_NONBLOCK);

	struct pollfd pfd;
	pfd.fd = clientFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	Client client(clientFd);

	std::map<int, ServerConfig*>::iterator it = _listenSockets.find(listenFd);
	if (it != _listenSockets.end() && it->second != NULL)
		client.setServer(*(it->second));
	_clients[clientFd] = client;
}

void ServerManager::removeClient(int clientFd) {
	close(clientFd);
	_clients.erase(clientFd);
	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it) {
		if (it->fd == clientFd) {
			_pollFds.erase(it);
			break;
		}
	}
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
	while (true) {
		poll(&_pollFds[0], _pollFds.size(), -1);
		for (size_t i = 0; i < _pollFds.size(); ) {
			
			// skip if no events occured
			if (_pollFds[i].revents == 0) {
				i++;
				continue;
			}

			int currentFd = _pollFds[i].fd;

			// catch unexpected disconnects and errors
			if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				removeClient(currentFd);
				continue;
			}

			// handle new connections (listening sockets)
			std::map<int, ServerConfig*>::iterator serverIter = _listenSockets.find(_pollFds[i].fd);
			if (serverIter != _listenSockets.end() && serverIter->second != NULL) {
				if (_pollFds[i].revents & POLLIN) {
					acceptClient(currentFd);
				}
				i++;
				continue;
			}

			// handle existing client traffic
			std::map<int, Client>::iterator clientIter = _clients.find(_pollFds[i].fd);
			if (clientIter != _clients.end()) {
				Client& client = clientIter->second;

				// 1. read incoming data if readable
				if (_pollFds[i].revents & POLLIN) {
					client.handleRead();
				}

				// 2. perform route matching and preparation if reading is complete
				if (clientIter->second.getClientState() == Client::PROCESSING) {
					clientIter->second.handleProcessing();
				}

				// 3. send response if socket is writable
				if (_pollFds[i].revents & POLLOUT) {
					clientIter->second.handleWrite();
				}

				// 4. remove client if client requested termination or completed response
				Client::ConnectionState state = client.getClientState();

				if (state == Client::DONE) {
					client.handleDone();
					removeClient(currentFd);
					continue; // do not increment i, elements shifted left
				}

				// 5. keep poll events synchronized with client state
				if (state == Client::WRITING_RESPONSE) {
					_pollFds[i].events = POLLOUT;					
				} else {
					_pollFds[i].events = POLLIN;
				}
			}
			i++;
		}
	}
}

/*
CGI I/O that can wait for data must be non-blocking and driven by poll()

Pipes created for child CGI stdin/stdout are I/O descriptors that 
can block. Reading or writing to pipes without monitoring them in 
your main poll() loop can cause deadlocks on large CGI payloads, 
violating the rule: "Calling read/recv or write/send on these 
descriptors without prior readiness will result in a grade of 0".

Clients must be able to upload files" and "At least GET, POST, 
DELETE methods

In Client.cpp, file serving, autoindex generation, POST file 
uploading, and DELETE handling are currently empty stubs or comments.*/
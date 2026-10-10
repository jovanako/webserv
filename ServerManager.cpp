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

void ServerManager::acceptClient(int listenFd, std::vector<struct pollfd>& pendingFds) {
	// extracts the first connection request and creates new fd for the client
	int clientFd = accept(listenFd, NULL, NULL);
	// if there are no pending connections, return to the polling loop
	if (clientFd < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			// log the actual error, but do NOT exit the program
			std::cerr << "Error: accept() failed for listenFd " << listenFd
					  << " (errno: " << errno << ")\n";
		}
		return; // safely exit the function to keep the server loop running
	}
	
	fcntl(clientFd, F_SETFL, O_NONBLOCK);

	struct pollfd pfd;
	pfd.fd = clientFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	
	pendingFds.push_back(pfd);

	Client client(clientFd);

	std::map<int, size_t>::iterator it = _listenSockets.find(listenFd);
	if (it != _listenSockets.end()) {
		client.setServer(_servers[it->second]);
	}

	client.setVirtualHosts(_servers);
	
	_clients[clientFd] = client;
}

void ServerManager::removeClient(int clientFd) {
	close(clientFd);
	_clients.erase(clientFd);
	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it) {
		if (it->fd == clientFd) {
			_pollFds.erase(it); // check if ok
			break;
		}
	}
}

void ServerManager::initServers() {
	std::set<std::pair<std::string, int> > boundAddresses;
	
	for (size_t i = 0; i < _servers.size(); ++i) {
		std::string host = _servers[i].getHost();
		int port = _servers[i].getPort();

		// check if this host:port combination is already bound
		if (boundAddresses.count(std::make_pair(host, port)) > 0) {
			std::cout << "Virtual host detected for " << host << ":" << port
					  << " - skipping socket creation.\n";
			continue;
		}

		struct addrinfo hints, *serverInfo;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET; // IPv4
		hints.ai_socktype = SOCK_STREAM; // TCP
		hints.ai_flags = AI_PASSIVE; // instructs getaddrinfo to return a bindable address if host is null

		std::string portStr = _servers[i].getPortString();

		if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &serverInfo) != 0) {
			std::cerr << "Error: getaddrinfo failed\n";
			continue;
		}

		int listenFd = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
		if (listenFd < 0) {
			freeaddrinfo(serverInfo);
			continue;
		}
		
		// allow immediate reuse of the port	
		int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		// retrieve the current flags
		int flags = fcntl(listenFd, F_GETFL, 0);
		if (flags == -1) {
			std::cerr << "Error: fcntl(F_GETFL) failed for host " << host << ":" << port << std::endl;;
			close(listenFd);
			continue; // skip this server block and move to the next one
		}

		flags |= O_NONBLOCK; // append the non-blocking flag

		if (fcntl(listenFd, F_SETFL, flags) == -1) {
			std::cerr << "Failed to set non-blocking flag" << std::endl;
			close(listenFd);
			continue;
		}

		if (bind(listenFd, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
			std::cerr << "Error: bind failed for " << host << ":" << portStr << "\n";
			close(listenFd);
			freeaddrinfo(serverInfo);
			continue;
		}

		freeaddrinfo(serverInfo);

		if (listen(listenFd, 128) < 0) {
			std::cerr << "Error: listen failed\n";
			close(listenFd);
			continue;
		}

		// add the successfully bound host and port to our tracking set
		boundAddresses.insert(std::make_pair(host, port));

		// Map the new socket to its configuration so acceptClient() can pass it to the Client
		_listenSockets[listenFd] = i;

		// Register the listening socket in the dynamic poll vector
		struct pollfd pfd;
		pfd.fd = listenFd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_pollFds.push_back(pfd);
	}
}

void ServerManager::run() {
	// initialize a temporary container to hold newly accepted client file descriptors during the current polling cycle
	std::vector<struct pollfd> pendingFds;

	// begins the infinite event loop that keeps the server running continuously
	while (true) {
		// calls the poll() function to monitor all stored file descriptors
		// blocking indefinitely (-1) until an I/O event occurs (0 would return without waiting)
		// -1 puts the process to sleep in the operating system until actual network activity happens
		if (poll(&_pollFds[0], _pollFds.size(), -1) < 0) {
			// handle error
			continue; // skips the rest of the loop iteration if poll() encounters an error, preventing server crash
		}
		for (size_t i = 0; i < _pollFds.size(); ) {
			
			// checks if any events were returned (revents) for the current socket
			// if none, it increments the index and moves to the next descriptor
			if (_pollFds[i].revents == 0) {
				i++;
				continue;
			}

			int currentFd = _pollFds[i].fd;

			// catches severe socket errors or invalid descriptors
			// and immediately terminates the client connection, skipping further processing without incrementing i
			if (_pollFds[i].revents & (POLLERR | POLLNVAL)) {
				removeClient(currentFd);
				continue;
			}

			// checks whether the active file descriptor belongs to a main listening server socket
			std::map<int, size_t>::iterator serverIter = _listenSockets.find(currentFd);
			// confirms that the descriptor is a valid initialized listening socket
			if (serverIter != _listenSockets.end()) {
				// if a new connection request is waiting (POLLIN), it accepts the client 
				// and pushes its descriptor into the temporary pendingFds vector
				if (_pollFds[i].revents & POLLIN) {
					acceptClient(currentFd, pendingFds);
				}
				i++;
				continue;
			}

			// handle existing client traffic
			std::map<int, Client>::iterator clientIter = _clients.find(_pollFds[i].fd);
			if (clientIter != _clients.end()) {
				Client& client = clientIter->second;

				// handle POLLHUP only if there's nodata left to read
				if ((_pollFds[i].revents & POLLHUP) && !(_pollFds[i].revents & POLLIN)) {
					removeClient(currentFd);
					continue;
				}

				// read incoming data if readable
				if (_pollFds[i].revents & POLLIN) {
					client.handleRead();
				}

				// if read caused client disconnect / error, clean up immediately
				if (client.getClientState() == Client::DONE) {
					client.handleDone();
					removeClient(currentFd);
					continue;
				}

				// perform route matching and preparation if reading is complete
				if (clientIter->second.getClientState() == Client::PROCESSING) {
					clientIter->second.handleProcessing();
				}

				// send response if socket is writable
				if ((_pollFds[i].revents & POLLOUT) &&
					(client.getClientState() == Client::WRITING_RESPONSE)) {
					clientIter->second.handleWrite();
				}

				// remove client if client requested termination or completed response
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
		if (!pendingFds.empty()) {
			_pollFds.insert(_pollFds.end(), pendingFds.begin(), pendingFds.end());
			pendingFds.clear();
		}
	}
}
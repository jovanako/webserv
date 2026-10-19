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

int ServerManager::acceptConnection(int listenFd) {
	// extracts the first connection request and creates new fd for the client
	int clientFd = accept(listenFd, NULL, NULL);
	// if there are no pending connections, return to the polling loop
	if (clientFd < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			// log the actual error, but do NOT exit the program
			std::cerr << "Error: accept() failed for listenFd " << listenFd
					  << " (errno: " << errno << ")\n";
		}
		return -1; // safely exit the function to keep the server loop running
	}
	return clientFd;
}

void ServerManager::setClientNonBlocking(int clientFd) {
	fcntl(clientFd, F_SETFL, O_NONBLOCK);
}

void ServerManager::registerClientSocket(int clientFd,
	std::vector<struct pollfd>& pendingFds) {
	struct pollfd pfd;
	pfd.fd = clientFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	pendingFds.push_back(pfd);
}

void ServerManager::configureClient(Client& client, int listenFd) {
	std::map<int, size_t>::iterator it = _listenSockets.find(listenFd);
	if (it != _listenSockets.end()) {
		client.setServer(_servers[it->second]);
	}

	client.setVirtualHosts(_servers);
}

void ServerManager::acceptClient(int listenFd, std::vector<struct pollfd>& pendingFds) {
	int clientFd = acceptConnection(listenFd);
	if (clientFd < 0) {
		return;
	}

	setClientNonBlocking(clientFd);
	registerClientSocket(clientFd, pendingFds);

	Client client(clientFd);
	configureClient(client, listenFd);
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

bool ServerManager::removeCompletedClient(int clientFd, Client& client) {
	if (client.getClientState() != Client::DONE) {
		return false;
	}

	client.handleDone();
	removeClient(clientFd);
	return true;
}

bool ServerManager::handleClientInput(size_t pollIndex, int clientFd, Client& client) {
	// handle POLLHUP only if there's no data left to read
	if ((_pollFds[pollIndex].revents & POLLHUP) &&
		!(_pollFds[pollIndex].revents & POLLIN)) {
		removeClient(clientFd);
		return true;
	}

	// read incoming data if readable
	if (_pollFds[pollIndex].revents & POLLIN) {
		client.handleRead();
	}

	// if read caused client disconnect / error, clean up immediately
	return removeCompletedClient(clientFd, client);
}

void ServerManager::processClient(Client& client) {
	// perform route matching and preparation if reading is complete
	if (client.getClientState() == Client::PROCESSING) {
		client.handleProcessing();
	}
}

void ServerManager::writeClientResponse(size_t pollIndex, Client& client) {
	// send response if socket is writable
	if ((_pollFds[pollIndex].revents & POLLOUT) &&
		(client.getClientState() == Client::WRITING_RESPONSE)) {
		client.handleWrite();
	}
}

void ServerManager::updateClientPollEvents(size_t pollIndex, const Client& client) {
	// keep poll events synchronized with client state
	if (client.getClientState() == Client::WRITING_RESPONSE) {
		_pollFds[pollIndex].events = POLLOUT;
	} else {
		_pollFds[pollIndex].events = POLLIN;
	}
}

bool ServerManager::handleClientEvent(size_t pollIndex) {
	int currentFd = _pollFds[pollIndex].fd;
	std::map<int, Client>::iterator clientIter = _clients.find(currentFd);
	if (clientIter == _clients.end()) {
		return false;
	}

	Client& client = clientIter->second;
	if (handleClientInput(pollIndex, currentFd, client)) {
		return true;
	}

	processClient(client);
	writeClientResponse(pollIndex, client);
	if (removeCompletedClient(currentFd, client)) {
		return true;
	}

	updateClientPollEvents(pollIndex, client);
	return false;
}

void ServerManager::registerListeningSocket(int listenFd, size_t serverIndex,
	const std::string& host, int port,
	std::set<std::pair<std::string, int> >& boundAddresses) {
	// add the successfully bound host and port to our tracking set
	boundAddresses.insert(std::make_pair(host, port));

	// Map the new socket to its configuration so acceptClient() can pass it to the Client
	_listenSockets[listenFd] = serverIndex;

	// Register the listening socket in the dynamic poll vector
	struct pollfd pfd;
	pfd.fd = listenFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);
}

int ServerManager::createListeningSocket(const ServerConfig& server,
	struct addrinfo*& serverInfo) {
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	std::string host = server.getHost();
	std::string portStr = server.getPortString();
	if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &serverInfo) != 0) {
		std::cerr << "Error: getaddrinfo failed\n";
		return -1;
	}

	int listenFd = socket(serverInfo->ai_family, serverInfo->ai_socktype,
		serverInfo->ai_protocol);
	if (listenFd < 0) {
		freeaddrinfo(serverInfo);
		return -1;
	}
	return listenFd;
}

bool ServerManager::setListeningSocketNonBlocking(int listenFd,
	const std::string& host, int port) {
	int flags = fcntl(listenFd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "Error: fcntl(F_GETFL) failed for host " << host << ":" << port << std::endl;
		close(listenFd);
		return false;
	}

	flags |= O_NONBLOCK;
	if (fcntl(listenFd, F_SETFL, flags) == -1) {
		std::cerr << "Failed to set non-blocking flag" << std::endl;
		close(listenFd);
		return false;
	}
	return true;
}

bool ServerManager::bindListeningSocket(int listenFd,
	struct addrinfo* serverInfo, const std::string& host,
	const std::string& portStr) {
	if (bind(listenFd, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0) {
		std::cerr << "Error: bind failed for " << host << ":" << portStr << "\n";
		close(listenFd);
		freeaddrinfo(serverInfo);
		return false;
	}

	freeaddrinfo(serverInfo);
	return true;
}

void ServerManager::enableAddressReuse(int listenFd) {
	int opt = 1;
	setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

bool ServerManager::startListening(int listenFd) {
	if (listen(listenFd, 128) < 0) {
		std::cerr << "Error: listen failed\n";
		close(listenFd);
		return false;
	}
	return true;
}

bool ServerManager::configureListeningSocket(int listenFd,
	struct addrinfo* serverInfo, const std::string& host, int port,
	const std::string& portStr) {
	// allow immediate reuse of the port
	enableAddressReuse(listenFd);

	if (!setListeningSocketNonBlocking(listenFd, host, port)) {
		return false;
	}

	if (!bindListeningSocket(listenFd, serverInfo, host, portStr)) {
		return false;
	}

	if (!startListening(listenFd)) {
		return false;
	}
	return true;
}

void ServerManager::initServers() {
	std::set<std::pair<std::string, int> > boundAddresses;
	
	for (size_t i = 0; i < _servers.size(); ++i) {
		std::string host = _servers[i].getHost();
		int port = _servers[i].getPort();

		// check if this host:port combination is already bound
		// if yes, skip and rely on the HTTP Host header to route traffic later
		if (boundAddresses.count(std::make_pair(host, port)) > 0) {
			std::cout << "Virtual host detected for " << host << ":" << port
					  << " - skipping socket creation.\n";
			continue;
		}

		struct addrinfo* serverInfo;
		std::string portStr = _servers[i].getPortString();
		int listenFd = createListeningSocket(_servers[i], serverInfo);
		if (listenFd < 0) {
			continue;
		}
		
		if (!configureListeningSocket(listenFd, serverInfo, host, port, portStr)) {
			continue;
		}

		registerListeningSocket(listenFd, i, host, port, boundAddresses);
	}
}

bool ServerManager::waitForEvents() {
	if (poll(&_pollFds[0], _pollFds.size(), -1) >= 0) {
		return true;
	}

	if (errno == EINTR) {
		return false;
	}

	std::cerr << "Error: poll() failed: " << strerror(errno) << std::endl;
	return false;
}

void ServerManager::addPendingClients(std::vector<struct pollfd>& pendingFds) {
	if (pendingFds.empty()) {
		return;
	}

	_pollFds.insert(_pollFds.end(), pendingFds.begin(), pendingFds.end());
	pendingFds.clear();
}

bool ServerManager::handlePollEvent(size_t pollIndex,
	std::vector<struct pollfd>& pendingFds) {
	int currentFd = _pollFds[pollIndex].fd;
	std::map<int, size_t>::iterator serverIter = _listenSockets.find(currentFd);

	if (_pollFds[pollIndex].revents & (POLLERR | POLLNVAL)) {
		if (serverIter != _listenSockets.end()) {
			// it's a listening socket error (critical failure)
			std::cerr << "Critical error on listening socket fd: " << currentFd << std::endl;
			close(currentFd);
			_listenSockets.erase(serverIter);
			_pollFds.erase(_pollFds.begin() + pollIndex);
		} else {
			// it is definitely a client socket, safe to remove
			removeClient(currentFd);
		}
		return true;
	}

	// if the event is on a listening socket, accept the new client
	if (serverIter != _listenSockets.end()) {
		if (_pollFds[pollIndex].revents & POLLIN) {
			acceptClient(currentFd, pendingFds);
		}
		return false;
	}

	// handle existing client traffic
	return handleClientEvent(pollIndex);
}

void ServerManager::processPollEvents(std::vector<struct pollfd>& pendingFds) {
	for (size_t i = 0; i < _pollFds.size(); ) {
		// checks if any events were returned for the current socket
		if (_pollFds[i].revents == 0) {
			++i;
			continue;
		}

		if (handlePollEvent(i, pendingFds)) {
			continue;
		}
		++i;
	}
}

void ServerManager::run() {
	// initialize a temporary container to hold newly accepted client file descriptors during the current polling cycle
	std::vector<struct pollfd> pendingFds;

	// begins the infinite event loop that keeps the server running continuously
	while (true) {
		if (!waitForEvents()) {
			continue;
		}

		processPollEvents(pendingFds);
		addPendingClients(pendingFds);
	}
}
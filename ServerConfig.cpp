#include "ServerConfig.hpp"

ServerConfig::ServerConfig() 
    : _host("0.0.0.0"), _port(8080), _clientMaxBodySize(1048576) {}

ServerConfig::ServerConfig(const ServerConfig& other) {
    *this = other;
}

ServerConfig& ServerConfig::operator=(const ServerConfig& other) {
    if (this != &other) {
        _host = other._host;
        _port = other._port;
        _serverNames = other._serverNames;
        _errorPages = other._errorPages;
        _clientMaxBodySize = other._clientMaxBodySize;
        _locations = other._locations;
    }
    return *this;
}

ServerConfig::~ServerConfig() {}

// Setters
void ServerConfig::setHost(const std::string& host) {
	_host = host;
}

void ServerConfig::setPort(int port) {
	_port = port;
}

void ServerConfig::setClientMaxBodySize(size_t size) {
	_clientMaxBodySize = size;
}

// Getters
const std::string& ServerConfig::getHost() const {
	return _host;
}

int ServerConfig::getPort() const {
	return _port;
}

const std::vector<std::string>& ServerConfig::getServerNames() const {
	return _serverNames;
}

const std::map<int, std::string>& ServerConfig::getErrorPages() const {
	return _errorPages;
}

size_t ServerConfig::getClientMaxBodySize() const {
	return _clientMaxBodySize;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const {
	return _locations;
}

void ServerConfig::addLocation(const LocationConfig& location) {
	_locations.push_back(location);
}

void ServerConfig::addServerName(const std::string& name) {
	_serverNames.push_back(name);
}

void ServerConfig::addErrorPage(int statusCode, const std::string& errorFilePath) {
	_errorPages[statusCode] = errorFilePath;
}
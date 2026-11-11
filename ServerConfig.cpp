#include "ServerConfig.hpp"

ServerConfig::ServerConfig() 
    : _host("0.0.0.0"),
      _port(8080),
      _root(""),
      _clientMaxBodySize(1048576),
      _autoindex(false),
      _uploadStore(""),
      _redirect(std::make_pair(0, std::string(""))) {}

ServerConfig::ServerConfig(const ServerConfig& other) {
    *this = other;
}

ServerConfig& ServerConfig::operator=(const ServerConfig& other) {
    if (this != &other) {
        _host = other._host;
        _port = other._port;
        _root = other._root;
        _index = other._index;
        _allowedMethods = other._allowedMethods;
        _serverNames = other._serverNames;
        _errorPages = other._errorPages;
        _clientMaxBodySize = other._clientMaxBodySize;
        _autoindex = other._autoindex;
        _cgiHandlers = other._cgiHandlers;
        _uploadStore = other._uploadStore;
        _redirect = other._redirect;
        _locations = other._locations;
    }
    return *this;
}

ServerConfig::~ServerConfig() {}

const std::string& ServerConfig::getHost() const {
	return _host;
}

int ServerConfig::getPort() const {
	return _port;
}

const std::string& ServerConfig::getRoot() const {
	return _root;
}

const std::vector<std::string>& ServerConfig::getIndex() const {
	return _index;
}

const std::vector<std::string>& ServerConfig::getAllowedMethods() const {
	return _allowedMethods;
}

std::string ServerConfig::createPortString() const {
	std::ostringstream oss;
    oss << _port;
    return oss.str();
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

bool ServerConfig::getAutoIndex() const {
	return _autoindex;
}

const std::map<std::string, std::string>& ServerConfig::getCgiHandlers() const {
	return _cgiHandlers;
}

const std::string& ServerConfig::getUploadStore() const {
	return _uploadStore;
}

const std::pair<int, std::string>& ServerConfig::getRedirect() const {
	return _redirect;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const {
	return _locations;
}

void ServerConfig::setHost(const std::string& host) {
	_host = host;
}

void ServerConfig::setPort(int port) {
	_port = port;
}

void ServerConfig::setRoot(const std::string& root) {
	_root = root;
}

void ServerConfig::setClientMaxBodySize(size_t size) {
	_clientMaxBodySize = size;
}

void ServerConfig::setAutoindex(bool autoindex) {
	_autoindex = autoindex;
}

void ServerConfig::setUploadStore(const std::string& uploadStore) {
	_uploadStore = uploadStore;
}

void ServerConfig::setRedirect(int statusCode, const std::string& url) {
	_redirect = std::make_pair(statusCode, url);
}

void ServerConfig::addIndex(const std::string& index) {
	_index.push_back(index);
}

void ServerConfig::addAllowedMethod(const std::string& method) {
	_allowedMethods.push_back(method);
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

void ServerConfig::addCgiHandler(const std::string& extension, const std::string& path) {
	if (_cgiHandlers.find(extension) != _cgiHandlers.end()) {
		throw std::runtime_error("Config Error: Duplicate CGI handler for extension " + extension);
	}
	_cgiHandlers[extension] = path;
}
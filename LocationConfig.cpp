#include "LocationConfig.hpp"
// Default constructor
LocationConfig::LocationConfig()
    : _path(""),
      _root(""),
      _autoindex(false),
      _uploadStore(""),
      _redirect(std::make_pair(0, std::string(""))) {}

LocationConfig::LocationConfig(const LocationConfig& other) {
    *this = other;
}

LocationConfig& LocationConfig::operator=(const LocationConfig& other) {
    if (this != &other) {
        _path = other._path;
        _allowedMethods = other._allowedMethods;
        _root = other._root;
        _autoindex = other._autoindex;
        _index = other._index;
        _cgiHandlers = other._cgiHandlers;
        _uploadStore = other._uploadStore;
        _redirect = other._redirect;
    }
    return *this;
}
LocationConfig::~LocationConfig() {}

// Setters
void LocationConfig::setPath(const std::string& path) {
	_path = path;
}
void LocationConfig::addAllowedMethod(const std::string& method) {
	_allowedMethods.push_back(method);
}
void LocationConfig::setRoot(const std::string& root) {
	_root = root;
}
void LocationConfig::setAutoindex(bool autoindex) {
	_autoindex = autoindex;
}
void LocationConfig::addIndex(const std::string& index) {
	_index.push_back(index);
}
void LocationConfig::addCgiHandler(const std::string& extension, const std::string& path) {
	_cgiHandlers[extension] = path;
}
void LocationConfig::setUploadStore(const std::string& uploadStore) {
	_uploadStore = uploadStore;
}
void LocationConfig::setRedirect(int statusCode, const std::string& url) {
	_redirect = std::make_pair(statusCode, url);
}

// Getters
const std::string& LocationConfig::getPath() const {
	return _path;
}
const std::vector<std::string>& LocationConfig::getAllowedMethods() const {
	return _allowedMethods;
}
const std::string& LocationConfig::getRoot() const {
	return _root;
}
bool LocationConfig::getAutoindex() const {
	return _autoindex;
}
const std::vector<std::string>& LocationConfig::getIndex() const {
	return _index;
}
const std::map<std::string, std::string>& LocationConfig::getCgiHandlers() const {
	return _cgiHandlers;
}

const std::string& LocationConfig::getUploadStore() const {
	return _uploadStore;
}
const std::pair<int, std::string>& LocationConfig::getRedirect() const {
	return _redirect;
}
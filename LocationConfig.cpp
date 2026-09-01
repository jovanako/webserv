#include "LocationConfig.hpp"
// Default constructor
LocationConfig::LocationConfig()
    : _path(""),
      _root(""),
      _autoindex(false),
      _index("index.html"),
      _cgiExtension(""),
      _cgiPath(""),
      _uploadStore(""),
      _redirect(std::make_pair(0, "")) {}

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
        _cgiExtension = other._cgiExtension;
        _cgiPath = other._cgiPath;
        _uploadStore = other._uploadStore;
        _redirect = other._redirect;
    }
    return *this;
}
LocationConfig::~LocationConfig() {

}

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
void LocationConfig::setIndex(const std::string& index) {
	_index = index;
}
void LocationConfig::setCgi(const std::string& extension, const std::string& path) {
	_cgiExtension = extension;
	_cgiPath = path;
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
const std::string& LocationConfig::getIndex() const {
	return _index;
}
const std::string& LocationConfig::getCgiExtension() const {
	return _cgiExtension;
}
const std::string& LocationConfig::getCgiPath() const {
	return _cgiPath;
}
const std::string& LocationConfig::getUploadStore() const {
	return _uploadStore;
}
const std::pair<int, std::string>& LocationConfig::getRedirect() const {
	return _redirect;
}
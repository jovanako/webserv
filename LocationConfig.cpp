#include "LocationConfig.hpp"

LocationConfig::LocationConfig()
    : _path(""),
      _root(""),
      _autoindex(false),
      _hasAutoindex(false),
      _uploadStore(""),
      _clientMaxBodySize(1048576),
      _hasClientMaxBodySize(false),
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
        _hasAutoindex = other._hasAutoindex;
        _index = other._index;
        _cgiHandlers = other._cgiHandlers;
        _uploadStore = other._uploadStore;
        _clientMaxBodySize = other._clientMaxBodySize;
        _hasClientMaxBodySize = other._hasClientMaxBodySize;
        _redirect = other._redirect;
        _errorPages = other._errorPages;
    }
    return *this;
}
LocationConfig::~LocationConfig() {}

const std::string& LocationConfig::getPath() const {
	return _path;
}

const std::string& LocationConfig::getRoot() const {
	return _root;
}

const std::vector<std::string>& LocationConfig::getIndex() const {
	return _index;
}

const std::vector<std::string>& LocationConfig::getAllowedMethods() const {
	return _allowedMethods;
}

bool LocationConfig::getAutoindex() const {
	return _autoindex;
}

bool LocationConfig::hasAutoindex() const {
	return _hasAutoindex;
}

const std::map<std::string, std::string>& LocationConfig::getCgiHandlers() const {
	return _cgiHandlers;
}

const std::string& LocationConfig::getUploadStore() const {
	return _uploadStore;
}

size_t LocationConfig::getClientMaxBodySize() const {
	return _clientMaxBodySize;
}

bool LocationConfig::hasClientMaxBodySize() const {
	return _hasClientMaxBodySize;
}

const std::pair<int, std::string>& LocationConfig::getRedirect() const {
	return _redirect;
}

const std::map<int, std::string>& LocationConfig::getErrorPages() const {
	return _errorPages;
}

void LocationConfig::setPath(const std::string& path) {
	_path = path;
}

void LocationConfig::setRoot(const std::string& root) {
	_root = root;
}

void LocationConfig::setIndex(const std::vector<std::string>& indexVector) {
    _index = indexVector;
}

void LocationConfig::setAllowedMethods(const std::vector<std::string>& methodsVector) {
	_allowedMethods = methodsVector;
}

void LocationConfig::setCgiHandlers(const std::map<std::string, std::string>& cgiHandlers) {
	_cgiHandlers = cgiHandlers;
}

void LocationConfig::setErrorPages(const std::map<int, std::string>& errorPages) {
	_errorPages = errorPages;
}

void LocationConfig::setAutoindex(bool autoindex) {
	_autoindex = autoindex;
	_hasAutoindex = true;
}

void LocationConfig::setUploadStore(const std::string& uploadStore) {
	_uploadStore = uploadStore;
}

void LocationConfig::setClientMaxBodySize(size_t size) {
	_clientMaxBodySize = size;
	_hasClientMaxBodySize = true;
}

void LocationConfig::setRedirect(int statusCode, const std::string& url) {
	_redirect = std::make_pair(statusCode, url);
}

void LocationConfig::addIndex(const std::string& index) {
	_index.push_back(index);
}

void LocationConfig::addAllowedMethod(const std::string& method) {
	_allowedMethods.push_back(method);
}

void LocationConfig::addCgiHandler(const std::string& extension, const std::string& path) {
	if (_cgiHandlers.find(extension) != _cgiHandlers.end()) {
		throw std::runtime_error("Config Error: Duplicate CGI handler for extension " + extension);
	}
	_cgiHandlers[extension] = path;
}

void LocationConfig::addErrorPage(int statusCode, const std::string& errorFilePath) {
	_errorPages[statusCode] = errorFilePath;
}

std::string LocationConfig::resolveScriptName(const std::string& uri) const {
	size_t segStart = 1;
	while (segStart <= uri.length()) {
		size_t slashPos = uri.find('/', segStart);
		size_t segEnd = (slashPos == std::string::npos) ? uri.length() : slashPos;
		std::string segment = uri.substr(segStart, segEnd - segStart);

		size_t dotPos = segment.find_last_of('.');
		if (dotPos != std::string::npos) {
			std::string ext = segment.substr(dotPos);
			if (_cgiHandlers.find(ext) != _cgiHandlers.end()) {
				return uri.substr(0, segEnd);
			}
		}
		if (slashPos == std::string::npos) {
			break;
		}
		segStart = slashPos + 1;
	}
	return "";
}

std::string LocationConfig::getCgiHandler(const std::string& scriptName) const {
	size_t dotPos = scriptName.find_last_of('.');
	if (dotPos == std::string::npos) {
		return "";
	}
	std::string extension = scriptName.substr(dotPos);
	std::map<std::string, std::string>::const_iterator it = _cgiHandlers.find(extension);
	if (it == _cgiHandlers.end()) {
		return "";
	}
	return it->second;
}
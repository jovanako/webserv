#include "ConfigParser.hpp"

ConfigParser::ConfigParser(): _currentTokenIndex(0) {}

ConfigParser::ConfigParser(std::string filePath) : _configFilePath(filePath) {}

ConfigParser::ConfigParser(const ConfigParser& other) {
	*this = other;
}

ConfigParser& ConfigParser::operator=(const ConfigParser& other) {
	if (this != &other) {
		_configFilePath = other._configFilePath;
		_tokens = other._tokens;
		_currentTokenIndex = other._currentTokenIndex;
		_servers = other._servers;
	}
	return *this;
}

ConfigParser::~ConfigParser() {}

void ConfigParser::tokenize() {
	std::ifstream configFile(_configFilePath.c_str());
	if (!configFile.is_open()) {
		throw std::runtime_error("Could not open config file");
	}

	std::string line;
	while (std::getline(configFile, line)) {
		// get rid of comments
		size_t commentPos = line.find('#');
		if (commentPos != std::string::npos) {
			line.erase(commentPos);
		}

		// pad with ws so that we get isolated tokens
		for (size_t i = 0; i < line.length(); ++i) {
			if (line[i] == '{' || line[i] == '}' || line[i] == ';') {
				line.insert(i + 1, " ");
				line.insert(i, " ");
				i += 2;
			}
		}

		// extract the separated tokens
		std::istringstream iss(line);
		std::string word;
		while (iss >> word) {
			_tokens.push_back(word);
		}
	}
}

void ConfigParser::parseServerBlock() {
	ServerConfig server;
	verifyToken("{");

	while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != "}") {
		std::string directive = _tokens[_currentTokenIndex++];

		if (directive == "listen") {
			server.setPort(std::atoi(_tokens[_currentTokenIndex++].c_str()));
			verifyToken(";");
		}
		else if (directive == "server_name") {
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				server.addServerName(_tokens[_currentTokenIndex++]);
			}
			verifyToken(";");
		}
		else if (directive == "client_max_body_size") {
			server.setClientMaxBodySize(parseSize(_tokens[_currentTokenIndex++]));
			verifyToken(";");
		}
		else if (directive == "error_page") {
			parseErrorPage(server);
		}
		else if (directive == "location") {
			parseLocationBlock(server);
		}
		else {
			throw std::runtime_error("Config Error: Unknown server directive");
		}
	}
	verifyToken("}");
	_servers.push_back(server);
}

void ConfigParser::parseErrorPage(ServerConfig& server) {
	std::string errorPath;
	size_t i = _currentTokenIndex;

	while (i < _tokens.size()) {
		if (i + 1 < _tokens.size() && _tokens[i + 1] == ";") {
			errorPath = _tokens[i];
			break;
		}
		i++;
	}

	if (i >= _tokens.size()) {
		throw std::runtime_error("Config Error: Missing ';' in error_page directive");
	}

	while (_currentTokenIndex != i) {
		server.addErrorPage(std::atoi(_tokens[_currentTokenIndex].c_str()), errorPath);
		_currentTokenIndex++;
	}

	_currentTokenIndex++;

	verifyToken(";");
}

void ConfigParser::parseLocationBlock(ServerConfig &server) {
	LocationConfig location;
	bool index_present = false;

	std::vector<std::string> cgiExts;
	std::vector<std::string> cgiPaths;

	if (_currentTokenIndex < _tokens.size()) {
		location.setPath(_tokens[_currentTokenIndex++]);
	} else {
		throw std::runtime_error("Config Error: Expected path after 'location'");
	}

	verifyToken("{");

	while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != "}") {
		std::string directive = _tokens[_currentTokenIndex++];
		
		if (directive == "root") {
			if(_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				location.setRoot(_tokens[_currentTokenIndex++]);
			} else {
				throw std::runtime_error("Config Error: Missing value for 'root'");
			}
			verifyToken(";");
		}
		else if (directive == "index") {
			index_present = true;
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				location.addIndex(_tokens[_currentTokenIndex++]);
			}
			verifyToken(";");
		}
		else if (directive == "allow_methods") {
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				location.addAllowedMethod(_tokens[_currentTokenIndex++]);
			}
			verifyToken(";");
		}
		else if (directive == "autoindex") {
			if (_tokens[_currentTokenIndex] == "on") {
				location.setAutoindex(true);
			} else if (_tokens[_currentTokenIndex] == "off") {
				location.setAutoindex(false);
			} else {
				throw std::runtime_error("Config Error: invalid autoindex");
			}
			_currentTokenIndex++;
			verifyToken(";");
		}
		else if (directive == "upload_store") {
			location.setUploadStore(_tokens[_currentTokenIndex++]);
			verifyToken(";");
		}
		else if (directive == "cgi_ext"){
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				cgiExts.push_back(_tokens[_currentTokenIndex++]);
			}
			verifyToken(";");
		}
		else if (directive == "cgi_path") {
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				cgiPaths.push_back(_tokens[_currentTokenIndex++]);
			}
			verifyToken(";");
		}
		else if (directive == "return") {
			location.setRedirect(std::atoi(_tokens[_currentTokenIndex].c_str()), _tokens[_currentTokenIndex + 1]);
			_currentTokenIndex += 2;
			verifyToken(";");
		}
		else {
			throw std::runtime_error("Config Error: Unknown location directive");
		}
	}
	verifyToken("}");

	if (!index_present)
		location.addIndex("index.html");

	if (cgiExts.size() != cgiPaths.size()) {
		throw std::runtime_error("Config Error: Count mismatch between cgi_ext and cgi_path");
	}
	for (size_t i = 0; i < cgiExts.size(); ++i) {
		location.addCgiHandler(cgiExts[i], cgiPaths[i]);
	}
	server.addLocation(location);
}


void ConfigParser::verifyToken(const std::string& expected) {
    if (_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex] != expected) {
        throw std::runtime_error("Config Error: Expected '" + expected + "' not found."); // Note: std::to_string(_currentTokenIndex)) Use a custom to_string equivalent if strictly C++98
    }
    _currentTokenIndex++;
}

size_t ConfigParser::parseSize(const std::string& sizeStr) {
	if (sizeStr.empty())
		return 0;
	
	char lastChar = sizeStr[sizeStr.length() - 1];
	size_t multiplier = 1;
	std::string numPart = sizeStr;
	std::string calculatedNumPart = sizeStr.substr(0, sizeStr.length() - 1);

	if (lastChar == 'K' || lastChar == 'k') {
		multiplier = 1024;
		numPart = calculatedNumPart;
	}
	else if (lastChar == 'M' || lastChar == 'm') {
		multiplier = 1048576;
		numPart = calculatedNumPart;
	}
	else if (lastChar == 'G' || lastChar == 'g') {
		multiplier = 1073741824;
		numPart = calculatedNumPart;
	}

	std::istringstream iss(numPart);
    size_t value;
    if (!(iss >> value)) {
        throw std::runtime_error("Config Error: Invalid client_max_body_size value '" + sizeStr + "'");
    }

	return value * multiplier;
}

std::vector<ServerConfig> ConfigParser::parse() {
	tokenize();
	
	_currentTokenIndex = 0;

	while (_currentTokenIndex < _tokens.size()) {
		if (_tokens[_currentTokenIndex] == "server") {
			_currentTokenIndex++;
			parseServerBlock();
		} else {
			throw std::runtime_error("Config Error: 'server' token missing, found token: '" + _tokens[_currentTokenIndex] + "'");
		}
	}

	if (_servers.empty()) {
		throw std::runtime_error("Config Error: No server blocks in configuration file");
	}

	return _servers;
}
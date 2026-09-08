#include "ConfigParser.hpp"

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

	if (_currentTokenIndex < _tokens.size()) {
		location.setPath(_tokens[_currentTokenIndex++]);
	} else {
		throw std::runtime_error("Config Error: Expected path after 'location'");
	}

	verifyToken("{");

	while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != "}") {
		if (_tokens[_currentTokenIndex] == "root") {
			locations.
		}
		else if (_tokens[_currentTokenIndex] == "index") {

		}
		else if (_tokens[_currentTokenIndex] == "allow_methods") {

		}
		else if (_tokens[_currentTokenIndex] == "autoindex") {

		}
		else if (_tokens[_currentTokenIndex] == "upload_store") {

		}
		else if (_tokens[_currentTokenIndex] == "cgi_ext" || _tokens[_currentTokenIndex] == "cgi_path") {
			// create a flag for an already populated cgiHandlers
		}
		else if (_tokens[_currentTokenIndex] == "return") {

		}
	}
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

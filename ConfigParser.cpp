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

std::string ConfigParser::getNextToken() {
	if (_currentTokenIndex >= _tokens.size()) {
		throw std::runtime_error("Config Error: Unexpected end of file.");
	}
	return _tokens[_currentTokenIndex++];
}

void ConfigParser::parseServerBlock() {
	ServerConfig server;
	verifyToken("{");

	while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != "}") {
		std::string directive = getNextToken();

		if (directive == "listen") {
			std::string token = getNextToken();
			std::string host = "0.0.0.0"; // default host if none is provided
			std::string portStr = token;

			// check if the token contains a colon (interface:port)
			size_t colonPos = token.find(':');
			if (colonPos != std::string::npos) {
				host = token.substr(0, colonPos);
				portStr = token.substr(colonPos + 1);
			}
			// check if the token is just an IP address (contains dots but no colon)
			else if (token.find('.') != std::string::npos) {
				host = token;
				portStr = "80"; // default HTTP port
			}

			// validate that the port string is not empty and contains only digits
			if (portStr.empty()) {
				throw std::runtime_error("Config Error: Missing port in listen directive");
			}
			for (size_t i = 0; i < portStr.length(); ++i) {
				if (!std::isdigit(static_cast<unsigned char>(portStr[i]))) {
					throw std::runtime_error("Config Error: Invalid port '" + portStr + "'");
				}
			}

			// convert and validate port range (1 - 65535)
			int port = std::atoi(portStr.c_str());
			if (port < 1 || port > 65535) {
				throw std::runtime_error("Config Error: Port out of range '" + portStr + "'");
			}

			// save both to the ServerConfig object
			server.setHost(host);
			server.setPort(port);
			verifyToken(";");
		}
		else if (directive == "server_name") {
			if (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] == ";") {
				throw std::runtime_error("Config Error: 'server_name' directive requires at least one name argument");
			}
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				server.addServerName(getNextToken());
			}
			verifyToken(";");
		}
		else if (directive == "client_max_body_size") {
			server.setClientMaxBodySize(parseSize(getNextToken()));
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
	std::vector<std::string> args;
	std::string token = getNextToken();

	// sequentially gather all arguments until we hit the semicolon
	while (token != ";") {
		args.push_back(token);
		token = getNextToken();
	}

	// an error_page directive needs at least one code and one path (min 2 args)
	if (args.size() < 2) {
		throw std::runtime_error("Config Error: error_page requires at least one status code and a file path");
	}

	// the last argument immediately before the semicolon is always the path
	std::string errorPath = args[args.size() - 1];

	// all preceding arguments are the status codes
	for (size_t i = 0; i < args.size() - 1; ++i) {

		//ensure the status code contains only digits
		for (size_t j = 0; j < args[i].length(); ++j) {
			if (!std::isdigit(static_cast<unsigned char>(args[i][j]))) {
				throw std::runtime_error("Config Error: Invalid error_page status code '" + args[i] + "'");
			}
		}
		server.addErrorPage(std::atoi(args[i].c_str()), errorPath);
	}
}

void ConfigParser::parseLocationBlock(ServerConfig &server) {
	LocationConfig location;
	bool index_present = false;

	location.setPath(getNextToken());

	verifyToken("{");

	while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != "}") {
		std::string directive = getNextToken();
		
		if (directive == "root") {
			location.setRoot(getNextToken());
			verifyToken(";");
		}
		else if (directive == "index") {
			// check if the very next token is the semicolon (meaning no arguments were given)
			if (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] == ";") {
				throw std::runtime_error("Config Error: 'index' directive requires at least one file argument");
			}

			index_present = true;
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				location.addIndex(getNextToken());
			}
			verifyToken(";");
		}
		else if (directive == "allow_methods") {
			if (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] == ";") {
				throw std::runtime_error("Config Error: 'allow_methods' directive requires at least one method argument");
			}

			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";") {
				std::string method = getNextToken();

				// validate against supported HTTP methods
				if (method != "GET" && method != "POST" && method != "DELETE") {
					throw std::runtime_error("ConfigError: Invalid or unsupported method '" + method + "' in allow_methods directive");
				}
				location.addAllowedMethod(method);
			}
			verifyToken(";");
		}
		else if (directive == "autoindex") {
			std::string autoindexVal = getNextToken();
			if (autoindexVal == "on") {
				location.setAutoindex(true);
			} else if (autoindexVal == "off") {
				location.setAutoindex(false);
			} else {
				throw std::runtime_error("Config Error: invalid autoindex");
			}
			verifyToken(";");
		}
		else if (directive == "upload_store") {
			location.setUploadStore(getNextToken());
			verifyToken(";");
		}
		else if (directive == "cgi_pass"){
			std::string extension = getNextToken();
			if (extension == ";") {
				throw std::runtime_error("Config Error: 'cgi_pass' requires an extension argument (e.g., .py)");
			}
			std::string path = getNextToken();
			if (path == ";") {
				throw std::runtime_error("Config Error: 'cgi_pass' requires an executable path argument");
			}
			location.addCgiHandler(extension, path);
			verifyToken(";");
		}
		else if (directive == "return") {
			std::string codeStr = getNextToken();

			// ensure the redirect code contains only digits
			for (size_t i = 0; i < codeStr.length(); ++i) {
				if (!std::isdigit(static_cast<unsigned char>(codeStr[i]))) {
					throw std::runtime_error("Config Error: Invalid return status code '" + codeStr + "'");
				}
			}

			int code = std::atoi(codeStr.c_str());
			std::string url = getNextToken();
			location.setRedirect(code, url);
			verifyToken(";");
		}
		else {
			throw std::runtime_error("Config Error: Unknown location directive");
		}
	}
	verifyToken("}");

	if (!index_present)
		location.addIndex("index.html");
		
	server.addLocation(location);
}


void ConfigParser::verifyToken(const std::string& expected) {
    if (_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex] != expected) {
        throw std::runtime_error("Config Error: Expected Token '" + expected + "' not found.");
    }
    _currentTokenIndex++;
}

size_t ConfigParser::parseSize(const std::string& sizeStr) {
	if (sizeStr.empty())
		return 0;
	
	char lastChar = sizeStr[sizeStr.length() - 1];
	size_t multiplier = 1;
	std::string numPart = sizeStr;

	if (lastChar == 'K' || lastChar == 'k') {
		multiplier = 1024;
		numPart = sizeStr.substr(0, sizeStr.length() - 1);
	} else if (lastChar == 'M' || lastChar == 'm') {
		multiplier = 1048576;
		numPart = sizeStr.substr(0, sizeStr.length() - 1);
	} else if (lastChar == 'G' || lastChar == 'g') {
		multiplier = 1073741824;
		numPart = sizeStr.substr(0, sizeStr.length() - 1);
	}

	std::istringstream iss(numPart);
    size_t value;
    if (!(iss >> value) || !iss.eof()) {
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
#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ServerConfig.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

class ConfigParser {
	private:
		std::string                 _configFilePath;
   		std::vector<std::string>    _tokens;
    	size_t                      _currentTokenIndex;
    	std::vector<ServerConfig>   _servers;

		// Helper functions for parsing
		void tokenize();
		void parseServerBlock();
		void parseErrorPage(ServerConfig& server);
		void parseLocationBlock(ServerConfig& server);
		void verifyToken(const std::string& expected);
		size_t parseSize(const std::string& sizeStr);
	public:
		ConfigParser();
		ConfigParser(const ConfigParser& other);
		ConfigParser& operator=(const ConfigParser& other);
		~ConfigParser();

		std::vector<ServerConfig> parse();
};

#endif
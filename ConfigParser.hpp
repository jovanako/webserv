#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ServerConfig.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

class ConfigParser {
	private:
		std::string                 _configFilePath;
		std::string                 _configDir;
   		std::vector<std::string>    _tokens;
    	size_t                      _currentTokenIndex;
    	std::vector<ServerConfig>   _servers;

		void		tokenize();
		std::string	getNextToken();
		std::string resolveDefaultConfigPath() const;
		std::string resolveConfigPath(const std::string& path) const;
		std::string normalizePath(const std::string& path, char **envp) const;
		std::string extractDirectory(const std::string& filePath) const;
		void		appendLineTokens(const std::string& line);
		std::vector<std::string>	getTokensUntilSemicolon();
		std::string					getErrorPagePath(const std::vector<std::string>& args) const;
		void		verifyToken(const std::string& expected);
		size_t		parseSize(const std::string& sizeStr);

		void		parseServerBlock();
		void		parseServerDirective(ServerConfig& server, const std::string& directive);
		void		addDefaultLocation(ServerConfig& server) const;
		void		parseLocationBlock(ServerConfig& server);
		void		parseLocationDirective(LocationConfig& location, const std::string& directive);
		void		addServerDefaultsToLocation(const ServerConfig& server, LocationConfig& location) const;
		
		void		parseListenDirective(ServerConfig& server);
		void		parseServerNames(ServerConfig& server);

		template <typename T> void parseIndex(T& config);
		template <typename T> void parseRoot(T& config);
		template <typename T> void parseAutoindex(T& config);
		template <typename T> void parseUploadStore(T& config);
		template <typename T> void parseMaxBodySize(T& config);
		template <typename T> void parseCgi(T& config);
		template <typename T> void parseReturn(T& config);
		template <typename T> void parseErrorPage(T& config);
		template <typename T> void parseAllowedMethods(T& config);

		template <typename T> void addErrorPageCodes(T& config, const std::vector<std::string>& args, const std::string& errorPath);
		
	public:
		ConfigParser();
		ConfigParser(char **envp);
		ConfigParser(std::string filePath, char **envp = NULL);
		ConfigParser(const ConfigParser& other);
		ConfigParser& operator=(const ConfigParser& other);
		~ConfigParser();

		const std::string&			getConfigFilePath() const;
		const std::string&			getConfigDir() const;
		std::vector<ServerConfig>	parse();
};

#endif
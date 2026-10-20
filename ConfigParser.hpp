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

		void		tokenize();
		void		removeComment(std::string& line) const;
		void		isolatePunctuation(std::string& line) const;
		void		appendLineTokens(const std::string& line);
		std::string	getNextToken();
		void		parseServerBlock();
		void		parseListenTarget(const std::string& token,
			std::string& host, std::string& portStr) const;
		int		parseListenPort(const std::string& portStr) const;
		void		parseListenDirective(ServerConfig& server);
		void		parseServerNames(ServerConfig& server);
		void		parseClientMaxBodySize(ServerConfig& server);
		void		parseServerDirective(ServerConfig& server,
			const std::string& directive);
		void		parseErrorPage(ServerConfig& server);
		std::vector<std::string> getTokensUntilSemicolon();
		std::string getErrorPagePath(const std::vector<std::string>& args) const;
		void		addErrorPageCodes(ServerConfig& server,
			const std::vector<std::string>& args, const std::string& errorPath);
		void		parseLocationBlock(ServerConfig& server);
		void		parseLocationIndex(LocationConfig& location, bool& indexPresent);
		void		parseLocationMethods(LocationConfig& location);
		void		parseLocationRoot(LocationConfig& location);
		void		parseLocationAutoindex(LocationConfig& location);
		void		parseLocationUploadStore(LocationConfig& location);
		void		parseLocationCgi(LocationConfig& location);
		void		parseLocationReturn(LocationConfig& location);
		void		verifyToken(const std::string& expected);
		size_t		parseSizeUnit(const std::string& sizeStr,
			std::string& numPart) const;
		size_t		parseSizeNumber(const std::string& numPart,
			const std::string& originalSize) const;
		size_t		multiplySizeValue(size_t value, size_t multiplier,
			const std::string& originalSize) const;
		size_t		parseSize(const std::string& sizeStr);
		
	public:
		ConfigParser();
		ConfigParser(std::string filePath);
		ConfigParser(const ConfigParser& other);
		ConfigParser& operator=(const ConfigParser& other);
		~ConfigParser();

		std::vector<ServerConfig>	parse();
};

#endif
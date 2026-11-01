#ifndef CGIREQUEST_HPP
#define CGIREQUEST_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include "../ServerConfig.hpp"
#include "../LocationConfig.hpp"
#include "../HttpRequest.hpp"


class CgiRequest {
	private:
		std::string					_executable;
		std::string					_scriptPath;
		std::string					_workingDirectory;
		std::vector<std::string>	_envVars;
		const RequestBody&			_body;

		CgiRequest();
		CgiRequest& operator=(const CgiRequest& other);
		
		void		initPaths(const LocationConfig& location, const HttpRequest& request, std::string& scriptName, std::string& pathInfo);
		void		initWorkingDirectory();
		void		initContentEnvVars(const HttpRequest& request);
		void		initStandardEnvVars(const ServerConfig& server, const LocationConfig& location, const HttpRequest& request, const std::string& scriptName, const std::string& pathInfo);
		void		initHttpHeaderEnvVars(const HttpRequest& request);
		void		addEnvVar(const std::string& key, const std::string& value);
		void		addHeader(const HttpRequest& request, const std::string& headerKey, const std::string& envVarName);
		std::string	resolveScriptPath(const LocationConfig& location, const std::string& scriptName) const;
		std::string	resolvePathTranslated(const LocationConfig& location, const std::string& pathInfo) const;
		std::string	resolveServerName(const ServerConfig& server, const HttpRequest& request) const;
	
	public:
		CgiRequest(const ServerConfig& server, const LocationConfig& location, const HttpRequest& request);
		CgiRequest(const CgiRequest& other);
		~CgiRequest();

		const std::string&					getExecutable() const;
		const std::string&					getScriptPath() const;
		const std::string&					getWorkingDirectory() const;
		const std::vector<std::string>&		getEnvVars() const;
		const RequestBody&					getBody() const;
		std::vector<char*>					buildArgv() const;
		std::vector<char*>					buildEnvp() const;
};

#endif
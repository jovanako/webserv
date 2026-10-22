#ifndef CGIREQUEST_HPP
#define CGIREQUEST_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"
#include "HttpRequest.hpp"


class CgiRequest {
	private:
		std::string					_executable;
		std::string					_scriptPath;
		std::string					_workingDirectory;
		std::vector<std::string>	_envVars;
		int							_bodyFileDescriptor;

		CgiRequest();
		void addHeader(const HttpRequest& request, const std::string& headerKey, const std::string& envVarName);
	
	public:
		CgiRequest(const LocationConfig& location, const HttpRequest& request);
		CgiRequest(const CgiRequest& other);
		CgiRequest& operator=(const CgiRequest& other);
		~CgiRequest();

		const std::string&					getExecutable() const;
		const std::string&					getScriptPath() const;
		const std::string&					getWorkingDirectory() const;
		const std::vector<std::string>&		getEnvVars() const;
		int									getBodyFileDescriptor() const;
};

#endif
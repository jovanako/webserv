#include "CgiRequest.hpp"

CgiRequest::CgiRequest()
	: _executable(""),
	  _scriptPath(""),
	  _workingDirectory(""),
	  _envVars(),
	  _bodyFileDescriptor(-1) {}

CgiRequest::CgiRequest(const ServerConfig& server, const LocationConfig& location, const HttpRequest& request) {
	std::string scriptName = resolveScriptName(location, request);
	std::string pathInfo = request.getUri().substr(scriptName.length());

	size_t dotPos = scriptName.find_last_of('.');
	std::string extension = (dotPos != std::string::npos) ? scriptName.substr(dotPos) : "";

	std::map<std::string, std::string> cgiHandlers = location.getCgiHandlers();
	std::map<std::string, std::string>::iterator it = cgiHandlers.find(extension);
	if (it == cgiHandlers.end()) {
		// TODO - throw
	} else {
		_executable = it->second;
	}

	std::string fullUriPath = request.makeFullPath(location);
	_scriptPath = fullUriPath.substr(0, fullUriPath.length() - pathInfo.length());
	size_t lastSlash = _scriptPath.find_last_of('/');
	if (lastSlash == std::string::npos) {
		_workingDirectory = ".";
	} else if (lastSlash == 0) {
		_workingDirectory = "/";
	} else {
		_workingDirectory = _scriptPath.substr(0, lastSlash);
	}

	addHeader(request, "content-length", "CONTENT_LENGTH");
	addHeader(request, "content-type", "CONTENT_TYPE");

	_envVars.push_back("GATEWAY_INTERFACE=CGI/1.1");
	_envVars.push_back("PATH_INFO=" + pathInfo);

	_envVars.push_back("REMOTE_ADDR=" + request.getRemoteAddr());
	_envVars.push_back("REMOTE_HOST=" + request.getRemoteAddr());
	_envVars.push_back("REQUEST_METHOD=" + request.getMethod());

	
	const std::map<std::string, std::string>& headers = request.getHeaders();
	for (std::map<std::string, std::string>::const_iterator headerIt = headers.begin(); headerIt != headers.end(); ++headerIt) {
		if (headerIt->first == "content-length" || headerIt->first == "content-type") {
			continue;
		}
		std::string envVarName = "HTTP_";
		for (size_t i = 0; i < headerIt->first.length(); ++i) {
			if (headerIt->first[i] == '-') {
				envVarName += '_';
			} else {
				envVarName += static_cast<char>(std::toupper(static_cast<unsigned char>(headerIt->first[i])));
			}
		}
		_envVars.push_back(envVarName + "=" + headerIt->second);
	}
	
	_envVars.push_back("SCRIPT_NAME=" + scriptName);

	_envVars.push_back("SERVER_NAME=" + resolveServerName(server, request));
	_envVars.push_back("SERVER_PORT=" + server.getPortString());
	_envVars.push_back("SERVER_PROTOCOL=" + request.getVersion());
	_envVars.push_back("SERVER_SOFTWARE=webserv/1.0");
}

CgiRequest::CgiRequest(const CgiRequest& other) {
	*this = other;
}

CgiRequest& CgiRequest::operator=(const CgiRequest& other) {
	if (this != &other) {
		_executable = other._executable;
		_scriptPath = other._scriptPath;
		_workingDirectory = other._workingDirectory;
		_envVars = other._envVars;
		_bodyFileDescriptor = other._bodyFileDescriptor;
	}
	return *this;
}

CgiRequest::~CgiRequest() {}

const std::string& CgiRequest::getExecutable() const {
	return _executable;
}

const std::string& CgiRequest::getScriptPath() const {
	return _scriptPath;
}

const std::string& CgiRequest::getWorkingDirectory() const {
	return _workingDirectory;
}

const std::vector<std::string>& CgiRequest::getEnvVars() const {
	return _envVars;
}

int CgiRequest::getBodyFileDescriptor() const {
	return _bodyFileDescriptor;
}

void CgiRequest::addHeader(const HttpRequest& request, const std::string& headerKey, const std::string& envVarName) {
	std::map<std::string, std::string> headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find(headerKey);
	if (it != headers.end()) {
		_envVars.push_back(envVarName + "=" + it->second);
	}
}

std::string CgiRequest::resolveScriptName(const LocationConfig& location, const HttpRequest& request) const {
	const std::string& uri = request.getUri();
	const std::map<std::string, std::string>& cgiHandlers = location.getCgiHandlers();

	size_t segStart = 1;
	while (segStart <= uri.length()) {
		size_t slashPos = uri.find('/', segStart);
		size_t segEnd = (slashPos == std::string::npos) ? uri.length() : slashPos;
		std::string segment = uri.substr(segStart, segEnd - segStart);

		size_t dotPos = segment.find_last_of('.');
		if (dotPos != std::string::npos) {
			std::string ext = segment.substr(dotPos);
			if (cgiHandlers.find(ext) != cgiHandlers.end()) {
				return uri.substr(0, segEnd);
			}
		}
		if (slashPos == std::string::npos) {
			break;
		}
		segStart = slashPos + 1;
	}
	return uri;
}

std::string CgiRequest::resolveServerName(const ServerConfig& server, const HttpRequest& request) const {
	const std::vector<std::string>& serverNames = server.getServerNames();
	if (serverNames.empty()) {
		return server.getHost();
	}

	const std::map<std::string, std::string>& headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("host");
	if (it != headers.end() && !it->second.empty()) {
		size_t colonPos = it->second.find(':');
		std::string hostName = (colonPos != std::string::npos)
			? it->second.substr(0, colonPos)
			: it->second;
		for (size_t i = 0; i < serverNames.size(); ++i) {
			if (serverNames[i] == hostName) {
				return serverNames[i];
			}
		}
	}
	return serverNames[0];
}

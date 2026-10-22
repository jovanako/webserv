#include "CgiRequest.hpp"

CgiRequest::CgiRequest()
	: _executable(""),
	  _scriptPath(""),
	  _workingDirectory(""),
	  _envVars(),
	  _bodyFileDescriptor(-1) {}

CgiRequest::CgiRequest(const LocationConfig& location, const HttpRequest& request) {
	size_t dotPos = request.getUri().find_last_of('.');
	std::string extension = request.getUri().substr(dotPos);

	std::map<std::string, std::string> cgiHandlers = location.getCgiHandlers();
	std::map<std::string, std::string>::iterator it = cgiHandlers.find(extension);
	if (it == cgiHandlers.end()) {
		// TODO - throw
	}
	_executable = it->second;
	
	_scriptPath = request.makeFullPath(location);
	std::string scriptDir = _scriptPath.substr(_scriptPath.find_last_of('/'));
	_workingDirectory = location.getRoot() + _scriptPath;

	addHeader(request, "content-length", "CONTENT_LENGTH");
	addHeader(request, "content-type", "CONTENT_TYPE");

	_envVars.push_back("GATEWAY_INTERFACE=CGI/1.1");

	// _envVars.push_back("REMOTE_ADDR=" + );
	// _envVars.push_back("REMOTE_HOST=" + );
	_envVars.push_back("REQUEST_METHOD=" + request.getMethod());

	
	addHeader(request, "host", "HTTP_HOST");
	addHeader(request, "user-agent", "HTTP_USER_AGENT");
	addHeader(request, "accept", "HTTP_ACCEPT");
	
	// _envVars.push_back("SCRIPT_NAME" +);

	// _envVars.push_back("SERVER_NAME=" + );
	// _envVars.push_back("SERVER_PORT=" + );
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

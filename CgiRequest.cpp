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
	_workingDirectory = location.getRoot(); // TODO get directory from _scriptPath
	
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


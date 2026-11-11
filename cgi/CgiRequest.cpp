#include "CgiRequest.hpp"

static std::string toLowerCopy(const std::string& value) {
	std::string lowerValue = value;
	for (size_t i = 0; i < lowerValue.length(); ++i) {
		lowerValue[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(lowerValue[i])));
	}
	return lowerValue;
}

static std::string formatHttpHeaderEnvKey(const std::string& headerKey) {
	std::string envVarName = "HTTP_";
	for (size_t i = 0; i < headerKey.length(); ++i) {
		if (headerKey[i] == '-') {
			envVarName += '_';
		} else {
			envVarName += static_cast<char>(std::toupper(
				static_cast<unsigned char>(headerKey[i])));
		}
	}
	return envVarName;
}

static bool isSkippedHttpHeader(const std::string& headerKey) {
	return headerKey == "content-length"
		|| headerKey == "content-type"
		|| headerKey == "transfer-encoding"
		|| headerKey == "proxy";
}

CgiRequest::CgiRequest(const ServerConfig& server, const LocationConfig& location, const HttpRequest& request)
	: _executable(""),
	  _scriptPath(""),
	  _scriptFileName(""),
	  _workingDirectory(""),
	  _envVars(),
	  _body(request.getBody()) {
	std::string scriptName;
	std::string pathInfo;

	initPaths(location, request, scriptName, pathInfo);
	initWorkingDirectory();
	initContentEnvVars(request);
	initStandardEnvVars(server, location, request, scriptName, pathInfo);
	initHttpHeaderEnvVars(request);
}

CgiRequest::CgiRequest(const CgiRequest& other)
	: _executable(other._executable),
	  _scriptPath(other._scriptPath),
	  _scriptFileName(other._scriptFileName),
	  _workingDirectory(other._workingDirectory),
	  _envVars(other._envVars),
	  _body(other._body) {}

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

const RequestBody& CgiRequest::getBody() const {
	return _body;
}

std::vector<char*> CgiRequest::buildArgv() const {
	std::vector<char*> argv;
	argv.reserve(3);
	argv.push_back(const_cast<char*>(_executable.c_str()));
	argv.push_back(const_cast<char*>(_scriptFileName.c_str()));
	argv.push_back(NULL);
	return argv;
}

std::vector<char*> CgiRequest::buildEnvp() const {
	std::vector<char*> envp;
	envp.reserve(_envVars.size() + 1);
	for (size_t i = 0; i < _envVars.size(); ++i) {
		envp.push_back(const_cast<char*>(_envVars[i].c_str()));
	}
	envp.push_back(NULL);
	return envp;
}

void CgiRequest::initPaths(const LocationConfig& location, const HttpRequest& request, std::string& scriptName, std::string& pathInfo) {
	scriptName = location.resolveScriptName(request.getUri());
	_executable = location.getCgiHandler(scriptName);
	if (_executable.empty()) {
		throw std::runtime_error("No CGI handler matched");
	}

	pathInfo = request.getUri().substr(scriptName.length());
	_scriptPath = resolveScriptPath(location, scriptName);
}

void CgiRequest::initWorkingDirectory() {
	size_t lastSlash = _scriptPath.find_last_of('/');
	if (lastSlash == std::string::npos) {
		_workingDirectory = ".";
		_scriptFileName = _scriptPath;
	} else if (lastSlash == 0) {
		_workingDirectory = "/";
		_scriptFileName = _scriptPath.substr(1);
	} else {
		_workingDirectory = _scriptPath.substr(0, lastSlash);
		_scriptFileName = _scriptPath.substr(lastSlash + 1);
	}
}

void CgiRequest::initContentEnvVars(const HttpRequest& request) {
	if (request.getHeaders().find("content-length") != request.getHeaders().end()) {
		addHeader(request, "content-length", "CONTENT_LENGTH");
	} else if (request.getBody().getSize() > 0) {
		std::ostringstream oss;
		oss << request.getBody().getSize();
		addEnvVar("CONTENT_LENGTH", oss.str());
	}
	addHeader(request, "content-type", "CONTENT_TYPE");
}

void CgiRequest::initStandardEnvVars(const ServerConfig& server, const LocationConfig& location, const HttpRequest& request, const std::string& scriptName, const std::string& pathInfo) {
	addEnvVar("GATEWAY_INTERFACE", "CGI/1.1");
	addEnvVar("PATH_INFO", pathInfo);
	if (!pathInfo.empty()) {
		addEnvVar("PATH_TRANSLATED", resolvePathTranslated(location, pathInfo));
	}
	addEnvVar("QUERY_STRING", request.getQueryString());
	addEnvVar("REMOTE_ADDR", request.getRemoteAddr());
	addEnvVar("REMOTE_HOST", request.getRemoteAddr());
	addEnvVar("REQUEST_METHOD", request.getMethod());

	// as set by NGINX
	std::string requestUri = request.getUri();
	if (!request.getQueryString().empty()) {
		requestUri += "?" + request.getQueryString();
	}
	addEnvVar("REQUEST_URI", requestUri);
	addEnvVar("DOCUMENT_ROOT", location.getRoot());

	// Required by php-cgi
	addEnvVar("REDIRECT_STATUS", "200");

	addEnvVar("SCRIPT_NAME", scriptName);
	addEnvVar("SCRIPT_FILENAME", _scriptFileName);
	addEnvVar("SERVER_NAME", resolveServerName(server, request));
	addEnvVar("SERVER_PORT", server.createPortString());
	addEnvVar("SERVER_PROTOCOL", request.getVersion());
	addEnvVar("SERVER_SOFTWARE", "webserv/1.0");
}

void CgiRequest::initHttpHeaderEnvVars(const HttpRequest& request) {
	const std::map<std::string, std::string>& headers = request.getHeaders();
	for (std::map<std::string, std::string>::const_iterator headerIt = headers.begin(); headerIt != headers.end(); ++headerIt) {
		if (isSkippedHttpHeader(headerIt->first)) {
			continue;
		}
		addEnvVar(formatHttpHeaderEnvKey(headerIt->first), headerIt->second);
	}
}

void CgiRequest::addEnvVar(const std::string& key, const std::string& value) {
	_envVars.push_back(key + "=" + value);
}

void CgiRequest::addHeader(const HttpRequest& request, const std::string& headerKey, const std::string& envVarName) {
	const std::map<std::string, std::string>& headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find(headerKey);
	if (it != headers.end()) {
		addEnvVar(envVarName, it->second);
	}
}

std::string CgiRequest::resolveScriptPath(const LocationConfig& location, const std::string& scriptName) const {
	std::string locPath = location.getPath();
	std::string strippedScript = (scriptName.find(locPath) == 0)
		? scriptName.substr(locPath.length())
		: scriptName;

	std::string fullPath = location.getRoot();
	if (!fullPath.empty() && fullPath[fullPath.length() - 1] == '/') {
		fullPath.erase(fullPath.length() - 1);
	}
	if (strippedScript.empty() || strippedScript[0] != '/') {
		fullPath += "/";
	}
	fullPath += strippedScript;
	return fullPath;
}

std::string CgiRequest::resolvePathTranslated(const LocationConfig& location, const std::string& pathInfo) const {
	if (pathInfo.empty()) {
		return "";
	}

	std::string fullPath = location.getRoot();
	if (!fullPath.empty() && fullPath[fullPath.length() - 1] == '/') {
		fullPath.erase(fullPath.length() - 1);
	}
	if (pathInfo[0] != '/') {
		fullPath += "/";
	}
	fullPath += pathInfo;
	return fullPath;
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
		std::string lowerHost = toLowerCopy(hostName);
		for (size_t i = 0; i < serverNames.size(); ++i) {
			if (toLowerCopy(serverNames[i]) == lowerHost) {
				return serverNames[i];
			}
		}
	}
	return serverNames[0];
}

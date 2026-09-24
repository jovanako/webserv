#include "Client.hpp"
#include <sstream>

Client::Client() 
	: _socketFd(-1),
	  _clientState(READING_HEADER),
	  _bytesSent(0)
  {}

Client::Client(int fd)
	: _socketFd(fd),
	  _clientState(READING_HEADER),
	  _bytesSent(0) {}

Client::Client(const Client& other) {
	*this = other;
}

Client& Client::operator=(const Client& other) {
	if(this != &other)
	{
		_socketFd = other._socketFd;
		_clientState = other._clientState;
		_request = other._request;
		_response = other._response;
		_readBuffer = other._readBuffer;
		_writeBuffer = other._writeBuffer;
		_bytesSent = other._bytesSent;
	}
	return *this;
}

Client::~Client() {}

Client::ConnectionState Client::getClientState() const {
	return _clientState;
}

void Client::setClientState(ConnectionState state) {
	_clientState = state;
}

void Client::setServer(const ServerConfig& server) {
	_server = server;
}

// check if we put in header
static bool isHostHeader(std::string key) {
	for (size_t i = 0; i < key.length(); ++i) {
		key[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[i])));
	}
	return key == "host";
}

static bool isValidUri(const std::string& uri) {
	if (uri.empty() || uri[0] != '/')
		return false;
	if (uri.length() > Client::MAX_URI_LENGTH)
		return false;

	for (size_t i = 0; i < uri.length(); ++i) {
		unsigned char c = static_cast<unsigned char>(uri[i]);
		// don't allow spaces, control chars, and non-ASCII bytes
		if (c <= 32 || c >= 127)
			return false;
	}
	return true;
}

void Client::handleReadHeader() {
	char buf[BUFFER_SIZE];
	ssize_t bytesRead = recv(_socketFd, buf, sizeof(buf), 0);

	if (bytesRead <= 0) {
		// 0 means client closed connection; < 0 means read error
		// handle error
		_clientState = DONE;
		return;
	}

	_readBuffer.append(buf, bytesRead);

	size_t headerEnd = _readBuffer.find("\r\n\r\n");
	if (headerEnd != std::string::npos) {
		// change state?
		parseHeaders();
		if (_clientState == WRITING_RESPONSE) {
			return; // exit immediately so the error response can be sent
		}
		bool hasHost = false;
		if (_request.getVersion() == "HTTP/1.1") {
			std::map<std::string, std::string> headers = _request.getHeaders();
			for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
				if (isHostHeader(it->first)) {
					hasHost = true;
					break; 
				}
			}
			if (!hasHost) {
				_response.setStatusCode(400); // 400 Bad Request strictly required for HTTP/1.1
				finalizeResponse();
				return;
			}
		}
		// Only keep leftover bytes that belong to the body by deleting the header
		_readBuffer.erase(0, headerEnd + 4);

		// If there is a body, handle it
		if (_request.getContentLength() > 0) {
			_clientState = READING_BODY;
			
			// If we have already read part of the body
			if (!_readBuffer.empty()) {
				handleReadBody();
			}
		}
		// Otherwise, if there is no body
		else {
			_clientState = PROCESSING;
		}
	}
}

void Client::handleReadBody() {
	char buf[BUFFER_SIZE];
	size_t contentLen = _request.getContentLength();

	if (_readBuffer.length() < contentLen) {
		ssize_t bytesRead = recv(_socketFd, buf, sizeof(buf), 0);
		
		if (bytesRead <= 0) {
			// 0 means client closed connection; < 0 means read error
			// handle error
			_clientState = DONE;
			return;
		}
		_readBuffer.append(buf, bytesRead);
	}

	if (_readBuffer.length() >= contentLen) {
		_request.appendBody(_readBuffer.data(), contentLen);

		// Erase the body - leftovers are from next http request
		_readBuffer.erase(0, contentLen);

		_clientState = PROCESSING;
	}
}

void Client::finalizeResponse() {
	if (shouldKeepAlive()) {
		_response.setHeader("Connection", "keep-alive");
	} else {
		_response.setHeader("Connection", "close");
	}

	std::ostringstream oss;
	oss << _response.getBody().size();
	_response.setHeader("Content-Length", oss.str());

	setClientState(WRITING_RESPONSE);
}

void Client::resetForNextRequest() {
	_request = HttpRequest();
	_response = HttpResponse();
	_writeBuffer.clear();
	_bytesSent = 0;
	_clientState = READING_HEADER;
}

void Client::handleProcessing() {
	std::string uri = _request.getUri();
	const LocationConfig* matchedLocation = NULL;
	size_t longestMatch = 0;
	bool cgiFlag = false;

	const std::vector<LocationConfig>& locations = _server.getLocations();
	for (std::vector<LocationConfig>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		std::string locPath = it->getPath();

		if (uri.find(locPath) == 0) {
			if (locPath.length() > longestMatch) {
				longestMatch = locPath.length();
				matchedLocation = &(*it);
			}
		}
	}
	if (matchedLocation == NULL) {
		_response.setStatusCode(404);
		// 404 page?
		finalizeResponse();
		return;
	}

	std::string method = _request.getMethod();
	bool isAllowed = false;
	const std::vector<std::string> allowedMethods = matchedLocation->getAllowedMethods();
	for (std::vector<std::string>::const_iterator it = allowedMethods.begin(); it != allowedMethods.end(); ++it) {
		if (*it == method) {
			isAllowed = true;
			break;
		}
	}

	if (!isAllowed) {
		_response.setStatusCode(405);
		// 405 method not allowed
		finalizeResponse();
		return;
	}

	if (_request.getContentLength() > _server.getClientMaxBodySize()) {
		_response.setStatusCode(413);
		// 413 Payload too large error
		finalizeResponse();
		return;
	}

	const std::pair<int, std::string> redirect = matchedLocation->getRedirect();
	if (!(redirect.first == 0 && redirect.second == "")) {
		_response.setStatusCode(redirect.first); // check if it starts with 3?
		_response.setHeader("Location", redirect.second);
		finalizeResponse();
		return;
	}

	const std::map<std::string, std::string> cgiHandlers = matchedLocation->getCgiHandlers();
	for (std::map<std::string, std::string>::const_iterator iter = cgiHandlers.begin(); iter != cgiHandlers.end(); ++iter) {
		if (uri.find(iter->first) != std::string::npos) {
			cgiFlag = true;
			// exeAssigns new contents to the vector, replacing its current contents, and modifying its size accordingly.
	execve(iter->second.c_str(), NULL, NULL); // TO DO fork() ...
			break;
		}
	}

	if (method == "POST" && matchedLocation->getUploadStore() != "") {
		// save the body payload to the designated directory
	}

	if (method == "GET") {
		std::string fullPath = matchedLocation->getRoot() + uri;
		struct stat path_stat;
		if (stat(fullPath.c_str(), &path_stat) == 0) {
			if (S_ISDIR(path_stat.st_mode)) {
				bool autoindex = matchedLocation->getAutoindex();
				// handle directory logic
			}
			else if (S_ISREG(path_stat.st_mode)) {
				// read the file and serve it
			}
		}
		else {
			//the path does not exist on disc
			_response.setStatusCode(404);
		}
	}

	

	// construct responsehandleWriteResponse
	if (!cgiFlag) {
		finalizeResponse();
	}
}

bool Client::shouldKeepAlive() const {
	if (_response.getStatusCode() >= 400 && _response.getStatusCode() != 404 && _response.getStatusCode() != 405) {
		return false;
	}

	std::map<std::string, std::string> headers = _request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("connection");
	std::string connVal = "";
	if (it != headers.end()) {
		connVal = it->second;
	}

	for (size_t i = 0; i < connVal.length(); ++i) {
		connVal[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(connVal[i])));
	}

	if (_request.getVersion() == "HTTP/1.1") {
		return (connVal != "close");
	} else if (_request.getVersion() == "HTTP/1.0") {
		return (connVal == "keep-alive");
	}
	return false;
}


void Client::handleWriteResponse() {
	if(_bytesSent == 0 && _writeBuffer.empty()) {
		std::vector<char> raw = _response.createResponse();
		_writeBuffer.assign(raw.begin(), raw.end());
	}
	size_t remainingBytes = _writeBuffer.size() - _bytesSent;

	ssize_t bytesWritten = send(_socketFd, &_writeBuffer[_bytesSent], remainingBytes, 0);
	if(bytesWritten <= 0) {
		setClientState(DONE); //handle error?
		return;
	}

	_bytesSent += bytesWritten;

	if(_bytesSent >= _writeBuffer.size()) {
		if (shouldKeepAlive()) {
			resetForNextRequest(); // set state back to reading header and clears request/response

			if (_readBuffer.find("\r\n\r\n") != std::string::npos) {
				handleReadHeader();
			}
		} else {
			setClientState(DONE);
		}
	}
}

void Client::handleCgiPipeWait() {

}

void Client::handleDone() {

}

void Client::handleRead() {
	if (_clientState == READING_HEADER) {
		handleReadHeader();
	} else if (_clientState == READING_BODY) {
		handleReadBody();
	}
}

void Client::handleWrite() {
	if (_clientState == WRITING_RESPONSE) {
		handleWriteResponse();
	}
}

// void Client::handleEvent() {
// 	switch(_clientState)
// 	{
// 		case READING_HEADER :
// 			handleReadHeader();
// 			break;
// 		case READING_BODY :
// 			handleReadBody();
// 			break;
// 		case PROCESSING :
// 			handleProcessing();
// 			break;
// 		case WRITING_RESPONSE :
// 			handleWriteResponse();
// 			break;
// 		case CGI_PIPE_WAIT :
// 			handleCgiPipeWait();
// 			break;
// 		case DONE :
// 			handleDone();
// 			break;
// 		default:
// 			break;
// 	}
// }

void Client::parseHeaders() {
	// 1. Find the end of the first line
	size_t firstLineEnd = _readBuffer.find("\r\n");
	if (firstLineEnd == std::string::npos) return; // Safety check

	// Extract just the request line (excluding the \r\n)
	std::string requestLine = _readBuffer.substr(0, firstLineEnd);

	// 2. Parse the request line safely using a string stream
	// This automatically handles spaces and prevents out-of-bounds crashes
	std::istringstream iss(requestLine);
	std::string method, uri, version; // check in the 42 rules if this multiple declaration is ok

	if (!(iss >> method >> uri >> version)) {
		_response.setStatusCode(400); // Bad Request
		finalizeResponse();
		return;
	}

	std::string extra;
	if (iss >> extra) {
		_response.setStatusCode(400);
		finalizeResponse();
		return;
	}

	if (!isValidUri(uri)) {
		if (uri.length() > MAX_URI_LENGTH) {
			_response.setStatusCode(414); // 414 uri too long
		} else {
			_response.setStatusCode(400); // 400 bad request
		}
		finalizeResponse();
		return;
	}
	
	_request.setMethod(method);
	_request.setUri(uri);
	if (version == "HTTP/1.0" || version == "HTTP/1.1") {
		_request.setVersion(version);
	}
	else {
		_response.setStatusCode(505); // HTTP version not supported
		finalizeResponse();
		return;
	}
	_request.setRequestState(HttpRequest::PARSE_HEADERS);

	// 3. Parse the headers line by line
	size_t lineStart = firstLineEnd + 2;
	size_t headerBlockEnd = _readBuffer.find("\r\n\r\n");

	while (lineStart < headerBlockEnd) {
		// Find the end of the current line
		size_t lineEnd = _readBuffer.find("\r\n", lineStart);
		if (lineEnd == std::string::npos) break;

		// Extract the single line
		std::string line = _readBuffer.substr(lineStart, lineEnd - lineStart);

		// Find the colon in this specific line
		size_t colonPos = line.find(':');
		if (colonPos != std::string::npos) {
			std::string key = line.substr(0, colonPos);
			std::string value = line.substr(colonPos + 1);

			// Trim leading spaces from the value (e.g., " localhost" -> "localhost")
			size_t valueStart = value.find_first_not_of(" \t");
			if (valueStart != std::string::npos) {
				value = value.substr(valueStart);
			} else {
				value.clear();
			}

			_request.addHeader(key, value);
		}

		// Move to the next line
		lineStart = lineEnd + 2;
	}
}


#include "Client.hpp"
#include <sstream>

Client::Client() 
	: _socketFd(-1),
	  _state(READING_HEADER)
  {

}

Client::Client(int fd)
	: _socketFd(fd),
	_state(READING_HEADER)
 {

}

Client::Client(const Client& other) {
	*this = other;
}

Client& Client::operator=(const Client& other) {
	if(this != &other)
	{
		_socketFd = other._socketFd;
		_state = other._state;
		_request = other._request;
		_response = other._response;
	}
	return *this;
}

Client::~Client() {}

Client::ConnectionState Client::getClientState() const {
	return _state;
}

void Client::setClientState(ConnectionState state) {
	_state = state;
}

void Client::handleReadHeader() {
	char buf[BUFFER_SIZE];
	ssize_t bytesRead = recv(_socketFd, buf, sizeof(buf), 0);

	if (bytesRead <= 0) {
		// 0 means client closed connection; < 0 means read error
		_state = DONE;
		return;
	}

	_readBuffer.append(buf, bytesRead);

	size_t headerEnd = _readBuffer.find("\r\n\r\n");
	if (headerEnd != std::string::npos) {
		//...HttpRequest parse header feed variables
		
		// Only keep leftover bytes that belong to the body by deleting the header
		_readBuffer.erase(0, headerEnd + 4);

		// If there is a body, handle it
		if (_request.getContentLength() > 0) {
			_state = READING_BODY;
			
			// If we have already read part of the body
			if (!_readBuffer.empty()) {
				handleReadBody();
			}
		}
		// Otherwise, if there is no body
		else {
			_state = PROCESSING;
		}
	}
}

void Client::handleReadBody() {
	char buf[BUFFER_SIZE];
	size_t contentLen = _request.getContentLength();
	size_t lengthRead = _readBuffer.length();

	if (lengthRead < contentLen) {
		ssize_t bytesRead = recv(_socketFd, buf, sizeof(buf), 0);
		
		if (bytesRead <= 0) {
			// 0 means client closed connection; < 0 means read error
			_state = DONE;
			return;
		}
		_readBuffer.append(buf, bytesRead);
	}

	if (lengthRead >= contentLen) {
		_request.appendBody(_readBuffer.data(), contentLen);

		// Erase the body - leftovers are from next http request
		_readBuffer.erase(0, contentLen);


		_state = PROCESSING;
	}
}

void Client::handleProcessing() {
	
}

void Client::handleWriteResponse() {

}

void Client::handleCgiPipeWait() {

}

void Client::handleDone() {

}

void Client::handleEvent() {
	switch(_state)
	{
		case READING_HEADER :
			handleReadHeader();
			break;
		case READING_BODY :
			handleReadBody();
			break;
		case PROCESSING :
			handleProcessing();
			break;
		case WRITING_RESPONSE :
			handleWriteResponse();
			break;
		case CGI_PIPE_WAIT :
			handleCgiPipeWait();
			break;
		case DONE :
			handleDone();
			break;
		default:
			break;
	}
}

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
		_request.setErrorCode(400); // Bad Request
		_request.setRequestState(HttpRequest::PARSE_ERROR);
		return;
	}
	
	_request.setMethod(method);
	_request.setUri(uri);
	_request.setVersion(version);
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
			}

			_request.addHeader(key, value);
		}

		// Move to the next line
		lineStart = lineEnd + 2;
	}
}

/*
parseHeaders explanation (concepts and method also explained in README):

Why this version is safer:

*	std::istringstream: Instead of manually chaining find and erase to get the 
	method, URI, and version, a string stream treats the string exactly like 
	std::cin. It safely extracts the three words separated by spaces. If there 
	aren't exactly three words, the if (!(iss >> ...)) check catches it instantly.

*	Targeted find(): By calculating lineEnd - lineStart, we give substr() the exact 
	length it needs, rather than an absolute position.

*	Whitespace Trimming: The HTTP standard requires servers to ignore spaces right 
	after the colon (e.g., Host: localhost). The find_first_not_of logic safely strips 
	that padding.
*/
#include "Client.hpp"
#include <sstream>

Client::Client() 
	: _socketFd(-1),
	  _clientState(READING_HEADER),
	  _bytesSent(0),
	  _clientIp(""),
	  _lastActivity(std::time(NULL))
	  {}

Client::Client(int fd)
	: _socketFd(fd),
	  _clientState(READING_HEADER),
	  _bytesSent(0),
	  _clientIp(""),
	  _lastActivity(std::time(NULL))
	  {}

Client::Client(const Client& other) {
	*this = other;
}

Client& Client::operator=(const Client& other) {
	if(this != &other)
	{
		_socketFd = other._socketFd;
		_clientState = other._clientState;
		_parser = other._parser;
		_handler = other._handler;
		_responseBuilder = other._responseBuilder;
		_response = other._response;
		_cgiExecutor = other._cgiExecutor;
		_server = other._server;
		_writeBuffer = other._writeBuffer;
		_bytesSent = other._bytesSent;
		_virtualHosts = other._virtualHosts;
		_clientIp = other._clientIp;
		_lastActivity = other._lastActivity;
	}
	return *this;
}

Client::~Client() {}

Client::ConnectionState Client::getClientState() const {
	return _clientState;
}

int Client::getSocketFd() const {
	return _socketFd;
}

int Client::getCgiPipeFd() const {
	return _cgiExecutor.getPipeFd();
}

bool Client::hasCgiTimedOut() const {
	return _clientState == CGI_PIPE_WAIT && _cgiExecutor.cgiHasTimedOut();
}

const ServerConfig& Client::getServer() const {
	return _server;
}

const std::vector<ServerConfig>& Client::getVirtualHosts() const {
	return _virtualHosts;
}

const std::string& Client::getClientIp() const {
	return _clientIp;
}

void Client::setClientState(ConnectionState state) {
	_clientState = state;
}

void Client::setServer(const ServerConfig& server) {
	_server = server;
}

void Client::setVirtualHosts(const std::vector<ServerConfig>& hosts) {
	_virtualHosts = hosts;
}

void Client::setClientIp(const std::string& clientIp) {
	_clientIp = clientIp;
	_parser.getRequest().setRemoteAddr(_clientIp);
}

bool Client::getHostHeaderValue(std::string& hostValue) {
	if (_parser.getRequest().getVersion() != "HTTP/1.1") {
		return true;
	}

	const std::map<std::string, std::string>& headers = _parser.getRequest().getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("host");
	if (it == headers.end()) {
		_response.setStatusCode(400); // 400 Bad Request strictly required for HTTP/1.1
		finalizeResponse();
		return false;
	}

	hostValue = it->second;
	return true;
}

std::string Client::extractHostName(const std::string& hostValue) const {
	// checking if we got the right server block (in case we are listening on the same port more than once)
	size_t colonPos = hostValue.find(':');
	if (colonPos != std::string::npos) {
		return hostValue.substr(0, colonPos);
	}
	return hostValue;
}

void Client::applyVirtualHostSelection(const std::string& hostName) {
	for (size_t i = 0; i < _virtualHosts.size(); ++i) {
		if (_virtualHosts[i].getPort() == _server.getPort()) {
			const std::vector<std::string>& serverNames = _virtualHosts[i].getServerNames();
			bool matchFound = false;

			for (size_t j = 0; j < serverNames.size(); ++j) {
				if (serverNames[j] == hostName) {
					_server = _virtualHosts[i];
					matchFound = true;
					break;
				}
			}
			if (matchFound) {
				break;
			}
		}
	}
}

bool Client::selectVirtualHost() {
	std::string hostValue = "";
	if (!getHostHeaderValue(hostValue)) {
		return false;
	}

	std::string hostName = extractHostName(hostValue);
	applyVirtualHostSelection(hostName);
	return true;
}

void Client::prepareRequestBody(size_t headerEnd) {
	// Only keep leftover bytes that belong to the body by deleting the header
	_parser.eraseBuffer(0, headerEnd + 4);

	// If there is a body, handle it
	if (_parser.getRequest().getContentLength() > 0 || _parser.getRequest().isChunked()) {
		_clientState = READING_BODY;

		// If we have already read part of the body
		if (!_parser.getReadBuffer().empty()) {
			handleReadBody();
		}
	}
	// Otherwise, if there is no body
	else {
		_clientState = PROCESSING;
	}
}

void Client::handleReadHeader() {
	size_t headerEnd;
	if(!_parser.hasHeaderEnd(headerEnd) && !readChunk()) {
		return;
	}
	if(_parser.hasHeaderEnd(headerEnd)) {
		if(!_parser.parseHeaders(_response)) {
			if (_response.getStatusCode() >= 400) {
				finalizeResponse();
			}
			return;
		}
		
		if (!selectVirtualHost()) {
			return;
		}
	
		if (_parser.getRequest().getContentLength() > _server.getClientMaxBodySize()) {
			_response.setStatusCode(413); // 413 Payload too large error
			finalizeResponse();
			return;
		}


		prepareRequestBody(headerEnd);
	}
}

bool Client::readChunk() {
	char buf[BUFFER_SIZE];
	ssize_t bytesRead = recv(_socketFd, buf, sizeof(buf), 0);
		
	if (bytesRead <= 0) {
		if (bytesRead < 0) {
			std::cerr << "Read error on fd " << _socketFd << "\n";
		} else {
			std::cerr << "Client disconnected on fd " << _socketFd << "\n";
		}
		_clientState = DONE;
		return false;
	}

	_lastActivity = std::time(NULL);
	_parser.appendData(buf, bytesRead);
	return true;
}

void Client::handleReadBody() {
	bool finishedParsing = false;
	if (_parser.getRequest().isChunked()) {
		finishedParsing = _parser.parseChunkedBody(_response, _server);
	} else {
		finishedParsing = _parser.processFixedLengthBody(_server, _response);
	}

	if(finishedParsing) {
		_clientState = PROCESSING;
	} else if (_response.getStatusCode() >= 400) {
		finalizeResponse();
	}
}

void Client::setupErrorResponse(int statusCode) {
	_responseBuilder.buildErrorResponse(statusCode, _response, _server, _parser.getRequest());
	setClientState(WRITING_RESPONSE);
}

void Client::finalizeResponse() {
	_responseBuilder.buildResponse(_response, _parser.getRequest());
	setClientState(WRITING_RESPONSE);
}

void Client::resetForNextRequest() {
	_cgiExecutor.cleanup();
	_parser.reset();
	_parser.getRequest().setRemoteAddr(_clientIp);
	_response = HttpResponse();
	_writeBuffer.clear();
	_bytesSent = 0;
	_lastActivity = std::time(NULL);
	_clientState = READING_HEADER;
}

bool Client::startCgiExecution() {
	const HttpRequest& request = _parser.getRequest();
	const LocationConfig* matchedLocation = _handler.findMatchingLocation(_server, request.getUri());
	if (!matchedLocation) {
		setupErrorResponse(404);
		return false;
	}

	CgiRequest cgiReq(_server, *matchedLocation, request);
	if (!_cgiExecutor.start(cgiReq)) {
		setupErrorResponse(_cgiExecutor.getErrorCode());
		return false;
	}

	setClientState(CGI_PIPE_WAIT);
	return true;
}

void Client::handleProcessing() {
	int statusCode;
	try {
		statusCode = _handler.processRequest(_parser.getRequest(), _response, _server);
		if (statusCode == 0) {
			startCgiExecution();
			return;
		}
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		setupErrorResponse(500);
		return;
	}

	if(statusCode >= 400) {
		setupErrorResponse(statusCode);
		return;
	}

	finalizeResponse();
}

void Client::handleWriteResponse() {
	if(_bytesSent == 0 && _writeBuffer.empty()) {
		std::vector<char> raw = _response.createResponse();
		_writeBuffer.assign(raw.begin(), raw.end());
	}
	size_t remainingBytes = _writeBuffer.size() - _bytesSent;

	ssize_t bytesWritten = send(_socketFd, &_writeBuffer[_bytesSent], remainingBytes, 0);
	if(bytesWritten <= 0) {
		setClientState(DONE);
		std::cerr << "Write error/disconnect on fd " << _socketFd << "\n";
		return;
	}

	_lastActivity = std::time(NULL);
	_bytesSent += bytesWritten;

	if(_bytesSent >= _writeBuffer.size()) {
		if(_response.getHeader("Connection") == "keep-alive") {
			resetForNextRequest();
			size_t dummy;
			if (_parser.hasHeaderEnd(dummy)) {
				handleReadHeader();
			}
		} else {
			setClientState(DONE);
		}
	}
}

void Client::handleCgiPipeWait() {
	if (_clientState != CGI_PIPE_WAIT) {
		return;
	}

	_cgiExecutor.readOutput();
	CgiExecutor::State state = _cgiExecutor.getState();

	if (state == CgiExecutor::CGI_RUNNING) {
		return;
	}

	if (state == CgiExecutor::CGI_ERROR) {
		int errCode = _cgiExecutor.getErrorCode();
		_cgiExecutor.cleanup();
		setupErrorResponse(errCode > 0 ? errCode : 500);
		return;
	}

	if (state == CgiExecutor::CGI_COMPLETE) {
		CgiResponse cgiResp;
		if (!cgiResp.parse(_cgiExecutor.getOutputBuffer())) {
			_cgiExecutor.cleanup();
			setupErrorResponse(502);
			return;
		}
		cgiResp.applyTo(_response);
		_cgiExecutor.cleanup();
		finalizeResponse();
	}
}

void Client::handleCgiTimeout() {
	if (_clientState != CGI_PIPE_WAIT) {
		return;
	}
	_cgiExecutor.handleTimeout();
	_cgiExecutor.cleanup();
	setupErrorResponse(504);
}

void Client::cleanupCgi() {
	_cgiExecutor.cleanup();
}

void Client::handleDone() {
	_cgiExecutor.cleanup();
	_parser.reset();
}

void Client::handleRead() {
	if (_clientState == READING_HEADER) {
		handleReadHeader();
	} else if (_clientState == READING_BODY) {
		if (readChunk()) {
			handleReadBody();
		}
	}
}

void Client::handleWrite() {
	if (_clientState == WRITING_RESPONSE) {
		handleWriteResponse();
	}
}

bool Client::clientHasTimedOut() const {
	if (_clientState == CGI_PIPE_WAIT) {
		return false;
	}
	return (std::time(NULL) - _lastActivity) >= CLIENT_TIMEOUT_SEC;
}


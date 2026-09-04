#include "Client.hpp"

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

}

void Client::handleReadBody() {

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
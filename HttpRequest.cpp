#include "HttpRequest.hpp"

HttpRequest::HttpRequest()
	: _state(PARSE_REQUEST_LINE),
	  _contentLength(0),
	  _errorCode(0) {}

HttpRequest::HttpRequest(const HttpRequest& other) {
	*this = other;
}

HttpRequest& HttpRequest::operator=(const HttpRequest& other){
	if (this != &other) {
		_state = other._state;
		_method = other._method;
		_uri = other._uri;
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_contentLength = other._contentLength;
		_errorCode = other._errorCode;
	}
	return *this;
}

HttpRequest::~HttpRequest() {}

// Setters
void HttpRequest::setState(ParseState state) {
	_state = state;
}

void HttpRequest::setErrorCode(int code) {
	_errorCode = code;
}

void HttpRequest::setMethod(const std::string& method) {
	_method = method;
}

void HttpRequest::setUri(const std::string& uri) {
	_uri = uri;
}

void HttpRequest::setVersion(const std::string& version) {
	_version = version;
}

void HttpRequest::setContentLength(size_t len) {
	_contentLength = len;
}

// Getters
int HttpRequest::getErrorCode() const {
	return _errorCode;
}

HttpRequest::ParseState HttpRequest::getState() const {
	return _state;
}

const std::string& HttpRequest::getMethod() const {
	return _method;
}

const std::string& HttpRequest::getUri() const {
	return _uri;
}

const std::string& HttpRequest::getVersion() const {
	return _version;
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
	return _headers;
}

const std::vector<char>& HttpRequest::getBody() const {
	return _body;
}

size_t HttpRequest::getContentLength() const {
	return _contentLength;
}

void HttpRequest::addHeader(const std::string& key, const std::string& value) {
	_headers[key] = value;
}

void HttpRequest::appendBody(const char* data, size_t size) {
    if (data && size > 0) {
        _body.insert(_body.end(), data, data + size); // Appends the full chunk safely
        // _contentLength can either track total accumulated bytes or the header's Content-Length
    }
}
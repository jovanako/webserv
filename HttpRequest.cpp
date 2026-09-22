#include "HttpRequest.hpp"

HttpRequest::HttpRequest()
	: _parseState(PARSE_REQUEST_LINE),
	  _contentLength(0),
	  _errorCode(0) {}

HttpRequest::HttpRequest(const HttpRequest& other) {
	*this = other;
}

HttpRequest& HttpRequest::operator=(const HttpRequest& other){
	if (this != &other) {
		_parseState = other._parseState;
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
void HttpRequest::setRequestState(ParsingState state) {
	_parseState = state;
}

void HttpRequest::setErrorCode(int code) {
	_errorCode = code;
}

void HttpRequest::setMethod(const std::string& method) {
	if (method == "GET" || method == "POST" || method == "DELETE") {
		_method = method;
	} else {
		_errorCode = 501; // 501 Not Implemented
		_parseState = PARSE_ERROR;
	}
}

void HttpRequest::setUri(const std::string& uri) {
	_uri = uri; // check validity
}

void HttpRequest::setVersion(const std::string& version) {
	if (version == "HTTP/1.0" || version == "HTTP/1.1") {
		_version = version;
	} else {
		_errorCode = 501; // 501 Not Implemented
		_parseState = PARSE_ERROR;
	}
}

void HttpRequest::setContentLength(size_t len) {
	_contentLength = len;
}

// Getters
int HttpRequest::getErrorCode() const {
	return _errorCode;
}

HttpRequest::ParsingState HttpRequest::getRequestState() const {
	return _parseState;
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

static void handleError(HttpRequest& request) {
	request.setErrorCode(400);
	request.setRequestState(HttpRequest::PARSE_ERROR);
	return;
}

static bool isValidHeaderKey(const std::string& key) {
	if (key.empty())
		return false;

	// Characters explicitly forbidden in an HTTP header field-name token
	const std::string invalidChars = " \t\r\n:()<>@,;:\\\"/[]?={}";
	
	for (size_t i = 0; i < key.length(); ++i) {
		unsigned char c = static_cast<unsigned char>(key[i]);

		// non-printable ASCII, control chars, and DEL
		if (c <= 32 || c >= 127)
			return false;

		// RFC separator chars
		if (invalidChars.find(static_cast<char>(c)) != std::string::npos)
			return false;
	}
	return true;
}

static bool isValidIpv4(const std::string& host) {
    if (host.empty() || host[0] == '.' ||  host[host.length() - 1] != '.')
		return false;

	std::istringstream ss(host);
    std::string segment;
    int count = 0;

    while (std::getline(ss, segment, '.')) {
        if (segment.empty() || segment.length() > 3)
            return false;

        for (size_t i = 0; i < segment.length(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(segment[i])))
                return false;
        }

        // Convert and check range [0, 255]
        std::istringstream numStream(segment);
        int val;
        numStream >> val;
        if (val < 0 || val > 255)
            return false;

        // Reject leading zeros (e.g. "01" is invalid)
        if (segment.length() > 1 && segment[0] == '0')
            return false;

        count++;
    }

    // Must have exactly 4 octets and no trailing dot
    return count == 4;
}

static bool isValidDomain(const std::string& host) {
    if (host.empty() || host.length() > 253)
        return false;

    // Check each dot-separated label
    size_t start = 0;
    while (start < host.length()) {
        size_t end = host.find('.', start);
        if (end == std::string::npos)
            end = host.length();

        std::string label = host.substr(start, end - start);

        // Label length must be between 1 and 63 characters
        if (label.empty() || label.length() > 63)
            return false;

        // Label cannot start or end with a hyphen
        if (label[0] == '-' || label[label.length() - 1] == '-')
            return false;

        // All characters in label must be alphanumeric or hyphen
        for (size_t i = 0; i < label.length(); ++i) {
            char c = label[i];
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-')
                return false;
        }

        start = end + 1;
    }

    return true;
}

static bool isValidHost(const std::string& hostHeaderValue) {
    if (hostHeaderValue.empty())
        return false;

    std::string host = hostHeaderValue;

    // Split off the optional port (:port)
    size_t colonPos = hostHeaderValue.find(':');
    if (colonPos != std::string::npos) {
        host = hostHeaderValue.substr(0, colonPos);
        std::string portStr = hostHeaderValue.substr(colonPos + 1);

        if (portStr.empty())
            return false;

        for (size_t i = 0; i < portStr.length(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(portStr[i])))
                return false;
        }

        std::istringstream ss(portStr);
        long port;
        ss >> port;
        if (port < 1 || port > 65535)
            return false;
    }

    if (isValidIpv4(host))
        return true;

    return isValidDomain(host);
}

void HttpRequest::addHeader(const std::string& key, const std::string& value) {
	if (!isValidHeaderKey(key)) {
		return handleError(*this);
	}
	std::string lowerKey = key;
	for (size_t i = 0; i < lowerKey.length(); ++i) {
		lowerKey[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerKey[i])));
	}

	// reject if host, content-length or transfer-encoding have duplicates
	if (lowerKey == "host" || lowerKey == "content-length" || lowerKey == "transfer-encoding") {
		if(_headers.find(lowerKey) != _headers.end()) {
			return handleError(*this);
		}
	}

	if (lowerKey == "host" && !isValidHost(value)) {
		return handleError(*this);
	}

	if (lowerKey == "content-length") {
		if (value.empty()) {
			return handleError(*this);
		}

		for (size_t i = 0; i < value.length(); ++i) {
			if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
				return handleError(*this);
			}
		}

		std::istringstream iss(value);
		size_t len;
		iss >> len;
		setContentLength(len);
	}
	
	_headers[lowerKey] = value;
	return;
}

void HttpRequest::appendBody(const char* data, size_t size) {
    if (data && size > 0) {
        _body.insert(_body.end(), data, data + size); // Appends the full chunk safely
        // _contentLength can either track total accumulated bytes or the header's Content-Length
    }
}
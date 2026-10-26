#include "HttpRequest.hpp"

HttpRequest::HttpRequest()
	: _parseState(PARSE_REQUEST_LINE),
	  _contentLength(0),
	  _errorCode(0),
	  _isChunked(false),
	  _remoteAddr("") {}

HttpRequest::HttpRequest(const HttpRequest& other) {
	*this = other;
}

HttpRequest& HttpRequest::operator=(const HttpRequest& other){
	if (this != &other) {
		_parseState = other._parseState;
		_method = other._method;
		_uri = other._uri;
		_queryString = other._queryString;
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_contentLength = other._contentLength;
		_errorCode = other._errorCode;
		_isChunked = other._isChunked;
		_remoteAddr = other._remoteAddr;
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

void HttpRequest::setRemoteAddr(const std::string& remoteAddr) {
	_remoteAddr = remoteAddr;
}

void HttpRequest::setMethod(const std::string& method) {
	// check for invalid characters (a method token should only contain uppercase letters)
    // maybe see all methods and check for random words
	for (size_t i = 0; i < method.length(); ++i) {
        if (method[i] < 'A' || method[i] > 'Z') {
            _errorCode = 400; // 400 Bad Request
            _parseState = PARSE_ERROR;
            return;
        }
    }

	// check for supported methods
	if (method == "GET" || method == "POST" || method == "DELETE") {
		_method = method;
	} else {
		_errorCode = 501; // 501 Not Implemented
		_parseState = PARSE_ERROR;
	}
}

static std::string decodePercentEncoding(const std::string& input) {
	std::string decoded;

	for (size_t i = 0; i < input.length(); ++i) {
		if (input[i] == '%') {
			std::string hexStr = input.substr(i + 1, 2);

			int value;
			std::stringstream iss(hexStr);
			if (iss >> std::hex >> value) {
				decoded += static_cast<char>(value);
			}
			i += 2; // skip over the two hex digits we just processed
		} else {
			decoded += input[i]; // normal character, just append it 
		}
	}
	return decoded;
}

int HttpRequest::validateUriStructure(const std::string& uri) const {
	if (uri.empty() || uri[0] != '/') {
		return 400;
	}

	if (uri.length() > MAX_URI_LENGTH) {
		return 414;
	}

	if (uri.find("/../") != std::string::npos
		|| (uri.length() >= 3 && uri.substr(uri.length() - 3) == "/..")
		|| uri.find("/..?") != std::string::npos) {
		return 403;
	}
	return 0;
}

bool HttpRequest::hasValidUriCharacters(const std::string& uri) const {
	for (size_t i = 0; i < uri.length(); ++i) {
		unsigned char c = static_cast<unsigned char>(uri[i]);
		if (c <= 32 || c >= 127) {
			return false;
		}

		if (c == '%') {
			if (i + 2 >= uri.length()
				|| !std::isxdigit(static_cast<unsigned char>(uri[i + 1]))
				|| !std::isxdigit(static_cast<unsigned char>(uri[i + 2]))) {
				return false;
			}
			i += 2;
		}
	}
	return true;
}

void HttpRequest::storeUriParts(const std::string& uri) {
	size_t queryPos = uri.find('?');
	if (queryPos != std::string::npos) {
		std::string rawUri = uri.substr(0, queryPos);
		std::string rawQuery = uri.substr(queryPos + 1);
		_uri = decodePercentEncoding(rawUri);
		_queryString = decodePercentEncoding(rawQuery);
	} else {
		_uri = decodePercentEncoding(uri);
		_queryString = "";
	}
}

void HttpRequest::setUri(const std::string& uri) {
	int structureError = validateUriStructure(uri);
	if (structureError != 0) {
		_errorCode = structureError;
		_parseState = PARSE_ERROR;
		return;
	}

	if (!hasValidUriCharacters(uri)) {
		_errorCode = 400;
		_parseState = PARSE_ERROR;
		return;
	}
	storeUriParts(uri);
}

void HttpRequest::setVersion(const std::string& version) {
	if (version == "HTTP/1.0" || version == "HTTP/1.1") {
		_version = version;
	} else {
		_errorCode = 505; // 505 HTTP version not supported
		_parseState = PARSE_ERROR;
	}
}

void HttpRequest::setContentLength(size_t len) {
	_contentLength = len;
}

// Getters
HttpRequest::ParsingState HttpRequest::getRequestState() const {
	return _parseState;
}

int HttpRequest::getErrorCode() const {
	return _errorCode;
}

bool HttpRequest::isChunked() const {
	return _isChunked;
}

const std::string& HttpRequest::getRemoteAddr() const {
	return _remoteAddr;
}

const std::string& HttpRequest::getMethod() const {
	return _method;
}

const std::string& HttpRequest::getUri() const {
	return _uri;
}

const std::string& HttpRequest::getQueryString() const {
	return _queryString;
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

static void handleError(HttpRequest& request, int code) {
	request.setErrorCode(code);
	request.setRequestState(HttpRequest::PARSE_ERROR);
	return;
}

static bool isValidHeaderKey(const std::string& key) {
	if (key.empty())
		return false;

	// Characters explicitly forbidden in an HTTP header field-name token
	const std::string invalidChars = "()<>@,;:\\\"/[]?={}";
	
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

static bool isValidIpv4Segment(const std::string& segment) {
	if (segment.empty() || segment.length() > 3)
		return false;

	for (size_t i = 0; i < segment.length(); ++i) {
		if (!std::isdigit(static_cast<unsigned char>(segment[i])))
			return false;
	}

	std::istringstream numStream(segment);
	int value;
	numStream >> value;
	if (value < 0 || value > 255)
		return false;

	if (segment.length() > 1 && segment[0] == '0')
		return false;

	return true;
}

static bool isValidIpv4(const std::string& host) {
	if (host.empty() || host[0] == '.' || host[host.length() - 1] == '.')
		return false;

	std::istringstream stream(host);
	std::string segment;
	int segmentCount = 0;

	while (std::getline(stream, segment, '.')) {
		if (!isValidIpv4Segment(segment))
			return false;
		segmentCount++;
	}

	return segmentCount == 4;
}

static bool isValidDomainLabel(const std::string& label, bool isTopLevel) {
	if (label.empty() || label.length() > 63)
		return false;

	if (label[0] == '-' || label[label.length() - 1] == '-')
		return false;

	bool hasAlpha = false;
	for (size_t i = 0; i < label.length(); ++i) {
		unsigned char c = static_cast<unsigned char>(label[i]);
		if (!std::isalnum(c) && c != '-')
			return false;
		if (std::isalpha(c))
			hasAlpha = true;
	}

	return !isTopLevel || hasAlpha;
}

static bool isValidDomain(const std::string& host) {
	if (host.empty() || host.length() > 253)
		return false;

	size_t start = 0;
	while (start < host.length()) {
		size_t end = host.find('.', start);
		if (end == std::string::npos)
			end = host.length();

		std::string label = host.substr(start, end - start);
		if (!isValidDomainLabel(label, end == host.length()))
			return false;

		start = end + 1;
	}

	return true;
}

static bool isValidPort(const std::string& portStr) {
	if (portStr.empty() || portStr.length() > 5)
		return false;

	for (size_t i = 0; i < portStr.length(); ++i) {
		if (!std::isdigit(static_cast<unsigned char>(portStr[i])))
			return false;
	}

	std::istringstream stream(portStr);
	long port;
	stream >> port;
	return port >= 1 && port <= 65535;
}

static bool splitHostAndPort(const std::string& hostHeaderValue,
	std::string& host) {
	size_t colonPos = hostHeaderValue.find(':');
	if (colonPos == std::string::npos) {
		host = hostHeaderValue;
		return true;
	}

	host = hostHeaderValue.substr(0, colonPos);
	std::string portStr = hostHeaderValue.substr(colonPos + 1);
	return isValidPort(portStr);
}

static bool isValidHost(const std::string& hostHeaderValue) {
	if (hostHeaderValue.empty())
		return false;

	std::string host;
	if (!splitHostAndPort(hostHeaderValue, host))
		return false;

	return isValidIpv4(host) || isValidDomain(host);
}

static std::string toLowerCopy(const std::string& value) {
	std::string lowerValue = value;
	for (size_t i = 0; i < lowerValue.length(); ++i) {
		lowerValue[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(lowerValue[i])));
	}
	return lowerValue;
}

static bool isDuplicateRestrictedHeader(const std::string& key,
	const std::map<std::string, std::string>& headers) {
	if (key != "host" && key != "content-length"
		&& key != "transfer-encoding") {
		return false;
	}
	return headers.find(key) != headers.end();
}

int HttpRequest::validateTransferEncoding(const std::string& value) {
	if (_headers.find("content-length") != _headers.end())
		return 400;

	if (toLowerCopy(value) != "chunked")
		return 501;

	_isChunked = true;
	return 0;
}

int HttpRequest::validateContentLength(const std::string& value) {
	if (_headers.find("transfer-encoding") != _headers.end())
		return 400;

	if (value.empty())
		return 400;

	for (size_t i = 0; i < value.length(); ++i) {
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			return 400;
	}

	std::istringstream stream(value);
	size_t length;
	if (!(stream >> length))
		return 400;

	setContentLength(length);
	return 0;
}

void HttpRequest::addHeader(const std::string& key, const std::string& value) {
	if (!isValidHeaderKey(key)) {
		return handleError(*this, 400);
	}
	std::string lowerKey = toLowerCopy(key);

	// reject if host, content-length or transfer-encoding have duplicates
	if (isDuplicateRestrictedHeader(lowerKey, _headers)) {
		return handleError(*this, 400);
	}

	if (lowerKey == "host" && !isValidHost(value)) {
		return handleError(*this, 400);
	}

	// a single HTTP request cannot contain both a Content-Length header and a Transfer-Encoding header
	if (lowerKey == "transfer-encoding") {
		int errorCode = validateTransferEncoding(value);
		if (errorCode != 0)
			return handleError(*this, errorCode);
	}

	if (lowerKey == "content-length") {
		int errorCode = validateContentLength(value);
		if (errorCode != 0)
			return handleError(*this, errorCode);
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

std::string HttpRequest::makeFullPath(const LocationConfig& matchedLocation) const {
	std::string path = matchedLocation.getPath();
	std::string strippedUri = _uri.substr(path.length());

	std::string fullPath = matchedLocation.getRoot();

	if (!fullPath.empty() && fullPath[fullPath.length() - 1] == '/') {
		fullPath.erase(fullPath.length() - 1);
	}
	if (strippedUri.empty() || strippedUri[0] != '/') {
		fullPath += "/";
	}
	fullPath += strippedUri;

	return fullPath;
}
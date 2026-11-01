#include "CgiResponse.hpp"

static std::string toLowerCopy(const std::string& value) {
	std::string lowerValue = value;
	for (size_t i = 0; i < lowerValue.length(); ++i) {
		lowerValue[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(lowerValue[i])));
	}
	return lowerValue;
}

static std::string trimWhitespace(const std::string& str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return "";
	}
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, last - first + 1);
}

CgiResponse::CgiResponse()
	: _statusCode(200),
	  _headers(),
	  _body() {}

CgiResponse::CgiResponse(const CgiResponse& other)
	: _statusCode(other._statusCode),
	  _headers(other._headers),
	  _body(other._body) {}

CgiResponse& CgiResponse::operator=(const CgiResponse& other) {
	if (this != &other) {
		_statusCode = other._statusCode;
		_headers = other._headers;
		_body = other._body;
	}
	return *this;
}

CgiResponse::~CgiResponse() {}

int CgiResponse::getStatusCode() const {
	return _statusCode;
}

const std::map<std::string, std::string>& CgiResponse::getHeaders() const {
	return _headers;
}

const std::vector<char>& CgiResponse::getBody() const {
	return _body;
}

bool CgiResponse::findHeaderBoundary(const std::vector<char>& rawOutput,
	size_t& headerEnd, size_t& bodyStart) const {
	for (size_t i = 0; i < rawOutput.size(); ++i) {
		if (i + 3 < rawOutput.size()
			&& rawOutput[i] == '\r' && rawOutput[i + 1] == '\n'
			&& rawOutput[i + 2] == '\r' && rawOutput[i + 3] == '\n') {
			headerEnd = i;
			bodyStart = i + 4;
			return true;
		}
		if (i + 1 < rawOutput.size()
			&& rawOutput[i] == '\n' && rawOutput[i + 1] == '\n') {
			headerEnd = i;
			bodyStart = i + 2;
			return true;
		}
	}
	return false;
}

bool CgiResponse::parseStatusHeader(const std::string& value) {
	std::istringstream iss(value);
	int code = 0;
	if (!(iss >> code) || code < 100 || code > 599) {
		return false;
	}
	_statusCode = code;
	return true;
}

bool CgiResponse::parseHeaderLine(const std::string& line) {
	std::string trimmedLine = trimWhitespace(line);
	if (trimmedLine.empty()) {
		return true;
	}

	size_t colonPos = trimmedLine.find(':');
	if (colonPos == std::string::npos || colonPos == 0) {
		return false;
	}

	std::string key = trimWhitespace(trimmedLine.substr(0, colonPos));
	std::string value = trimWhitespace(trimmedLine.substr(colonPos + 1));
	std::string lowerKey = toLowerCopy(key);

	if (lowerKey == "status") {
		return parseStatusHeader(value);
	}

	_headers[key] = value;
	return true;
}

bool CgiResponse::parseHeaderBlock(const std::string& headerBlock) {
	std::istringstream stream(headerBlock);
	std::string line;
	while (std::getline(stream, line)) {
		if (!line.empty() && line[line.length() - 1] == '\r') {
			line.erase(line.length() - 1);
		}
		if (!parseHeaderLine(line)) {
			return false;
		}
	}
	return true;
}

bool CgiResponse::parse(const std::vector<char>& rawOutput) {
	_statusCode = 200;
	_headers.clear();
	_body.clear();

	if (rawOutput.empty()) {
		return false;
	}

	size_t headerEnd = 0;
	size_t bodyStart = 0;
	if (!findHeaderBoundary(rawOutput, headerEnd, bodyStart)) {
		return false;
	}

	std::string headerBlock(rawOutput.begin(), rawOutput.begin() + headerEnd);
	if (!parseHeaderBlock(headerBlock)) {
		return false;
	}

	if (bodyStart < rawOutput.size()) {
		_body.assign(rawOutput.begin() + bodyStart, rawOutput.end());
	}

	return true;
}

void CgiResponse::applyTo(HttpResponse& response) const {
	int finalStatus = _statusCode;
	bool hasContentType = false;
	bool hasLocation = false;

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
		it != _headers.end(); ++it) {
		std::string lowerKey = toLowerCopy(it->first);
		if (lowerKey == "content-type") {
			hasContentType = true;
		} else if (lowerKey == "location") {
			hasLocation = true;
		}
		response.setHeader(it->first, it->second);
	}

	if (finalStatus == 200 && hasLocation) {
		finalStatus = 302;
	}

	response.setStatusCode(finalStatus);
	if (!hasContentType && !_body.empty()) {
		response.setHeader("Content-Type", "text/html");
	}
	response.setBody(_body);
}


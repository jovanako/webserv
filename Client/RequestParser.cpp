	#include "RequestParser.hpp"

    RequestParser::RequestParser() {}
    RequestParser::~RequestParser() {}
    RequestParser::RequestParser(const RequestParser& other) {
        *this = other;
    }

    RequestParser& RequestParser::operator=(const RequestParser& other) {
        if(this != &other) {
            _readBuffer = other._readBuffer;
            _request = other._request;
        }
        return *this;
    }

    void RequestParser::appendData(const char* buf, ssize_t bytesRead) {
        _readBuffer.append(buf, bytesRead);
    }
    void RequestParser::eraseBuffer(size_t pos, size_t len) {
        _readBuffer.erase(pos, len);
    }
    void RequestParser::reset()
    {
        _request = HttpRequest();
    }

    HttpRequest& RequestParser::getRequest() {
        return _request;
    }
    const HttpRequest& RequestParser::getRequest() const {
        return _request;
    }
    const std::string& RequestParser::getReadBuffer() const {
        return _readBuffer;
    }
    bool RequestParser::hasHeaderEnd(size_t& headerEnd) const {
        headerEnd = _readBuffer.find("\r\n\r\n");
        return headerEnd != std::string::npos;
    }

    bool RequestParser::parseRequestLineParts(const std::string& requestLine,
			std::string& method, std::string& uri, std::string& version) const {
        // 2. Parse the request line safely using a string stream
        // This automatically handles spaces and prevents out-of-bounds crashes
        std::istringstream stream(requestLine);
        // check in the 42 rules if this multiple declaration is ok
        if (!(stream >> method >> uri >> version)) {
            return false;
        }

        std::string extra;
        if (stream >> extra) {
            return false;
        }
        return true;
    }

	bool RequestParser::applyRequestLine(const std::string& method, const std::string& uri,
			const std::string& version, HttpResponse& response) {
        _request.setMethod(method);
        _request.setUri(uri);

        if (version == "HTTP/1.0" || version == "HTTP/1.1") {
            _request.setVersion(version);
            response.setVersion(version);
        } else {
            response.setStatusCode(505); // HTTP version not supported
            _request.setRequestState(HttpRequest::PARSE_ERROR);
            //finalizeResponse();
            return false;
        }

        if (_request.getRequestState() == HttpRequest::PARSE_ERROR) {
            response.setStatusCode(_request.getErrorCode());
            //finalizeResponse();
            return false;
        }

        _request.setRequestState(HttpRequest::PARSE_HEADERS);
        return true;
    }
        

	bool RequestParser::parseRequestLine(size_t& firstLineEnd, HttpResponse& response) {
        // 1. Find the end of the first line
        firstLineEnd = _readBuffer.find("\r\n");
        if (firstLineEnd == std::string::npos) return false; // Safety check

        // Extract just the request line (excluding the \r\n)
        std::string requestLine = _readBuffer.substr(0, firstLineEnd);
        std::string method;
        std::string uri;
        std::string version;
        if (!parseRequestLineParts(requestLine, method, uri, version)) {
            response.setStatusCode(400); // Bad Request
            //finalizeResponse();
            return false;
        }

        return applyRequestLine(method, uri, version, response);
    }

    static void trimHeaderValue(std::string& value) {
	// Trim leading and trailing spaces from the value
	size_t valueStart = value.find_first_not_of(" \t");
	if (valueStart == std::string::npos) {
		value.clear();
		return;
	}

	size_t valueEnd = value.find_last_not_of(" \t");
	value = value.substr(valueStart, valueEnd - valueStart + 1);
}

	bool RequestParser::storeParsedHeader(const std::string& key,
			const std::string& value, HttpResponse& response) {
        _request.addHeader(key, value);
        if (_request.getRequestState() == HttpRequest::PARSE_ERROR) {
            response.setStatusCode(_request.getErrorCode());
            //finalizeResponse();
            return false;
        }
        return true;
    }

	bool RequestParser::parseHeaderLine(const std::string& line, HttpResponse& response) {
        // Find the colon in this specific line
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            return true;
        }

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        trimHeaderValue(value);

        return storeParsedHeader(key, value, response);
    }

    bool RequestParser::parseHeaders(HttpResponse& response) {
	size_t firstLineEnd;
	if (!parseRequestLine(firstLineEnd, response)) {
		return false;
	}

	// 3. Parse the headers line by line
	size_t lineStart = firstLineEnd + 2;
	size_t headerBlockEnd = _readBuffer.find("\r\n\r\n");

	while (lineStart < headerBlockEnd) {
		// Find the end of the current line
		size_t lineEnd = _readBuffer.find("\r\n", lineStart);
		if (lineEnd == std::string::npos) break;

		// Extract the single line
		std::string line = _readBuffer.substr(lineStart, lineEnd - lineStart);

		if (!parseHeaderLine(line, response)) {
			return false;
		}

		// Move to the next line
		lineStart = lineEnd + 2;
	}
    return true;
}

    bool RequestParser::readChunkSize(size_t& lineEnd, size_t& chunkSize, HttpResponse& response) {
        // find the end of the chunk size line
        lineEnd = _readBuffer.find("\r\n");
        if (lineEnd == std::string::npos) {
            return false; // wait for more data to arrive
        }

        // extract chunk size (and ignore potential chunk extensions starting with ';')
        std::string hexSize = _readBuffer.substr(0, lineEnd);
        size_t semiPos = hexSize.find(';');
        if (semiPos != std::string::npos) {
            hexSize = hexSize.substr(0, semiPos);
        }

        // convert chunk size to integer
        std::stringstream stream;
        stream << std::hex << hexSize;
        if (!(stream >> chunkSize)) {
            response.setStatusCode(400);
            //buildErrorResponse(400); // bad request
            return false;
        }
        return true;
    }

	bool RequestParser::finishChunkedBody(size_t chunkLineEnd) {
	// check if the final "\r\n\r\n" is completely in the buffer
	if (_readBuffer.length() >= chunkLineEnd + 4 &&
		_readBuffer.substr(chunkLineEnd, 4) == "\r\n\r\n") {
		_readBuffer.erase(0, chunkLineEnd + 4);
		//_clientState = PROCESSING;
        return true;
	}
    return false;
}
	bool RequestParser::appendChunkData(size_t chunkLineEnd, size_t chunkSize) {	// check if the entire chunk data + trailing "\r\n" has been received
	if (_readBuffer.length() < chunkLineEnd + 2 + chunkSize + 2) {
		return false; // wait for the rest of the chunk
	}

	// append the chunk data to the request
	_request.appendBody(_readBuffer.data() + chunkLineEnd + 2, chunkSize);

	// erase the processed chunk size, chunk data, and both \r\n from the buffer
	_readBuffer.erase(0, chunkLineEnd + 2 + chunkSize + 2);
	return true;
}

    bool RequestParser::parseChunkedBody(HttpResponse& response) {
	while (true) {
		size_t pos;
		size_t chunkSize;
		if (!readChunkSize(pos, chunkSize, response)) {
			return false;
		}

		// a chunk size of 0 indicates the end of the request body
		if (chunkSize == 0) {
			return finishChunkedBody(pos); // wait for final "\r\n" if not fully received yet
		}

		if (!appendChunkData(pos, chunkSize)) {
			return false;
		}
	}
}
    bool RequestParser::processFixedLengthBody(const ServerConfig& server, HttpResponse& response) {
	size_t contentLen = _request.getContentLength();

	if (_readBuffer.length() > server.getClientMaxBodySize()) {
		//buildErrorResponse(413); // pazload too large
        response.setStatusCode(413);
		return false;
	}

	if (_readBuffer.length() >= contentLen) {
		_request.appendBody(_readBuffer.data(), contentLen);
		_readBuffer.erase(0, contentLen);
		//_clientState = PROCESSING;
        return true;
	}
    return false;
}

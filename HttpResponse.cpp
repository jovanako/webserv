#include "HttpResponse.hpp"
#include <string>
#include <sstream>

HttpResponse::HttpResponse() : _statusCode(0) {}

HttpResponse::HttpResponse(const HttpResponse& other) {
    *this = other;
}

HttpResponse &HttpResponse::operator=(const HttpResponse& other) {
    if(this != &other)
    {
        _statusCode = other._statusCode;
        _statusMessage = other._statusMessage;
        _version = other._version;
        _headers = other._headers;
        _body = other._body;
    }
    return *this;
}

HttpResponse::~HttpResponse() {}

std::string HttpResponse::getStatusMessage(int code) const {
    switch (code) {
        // 2xx Success
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        
        // 3xx Redirection
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        
        // 4xx Client Error
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
        
        // 5xx Server Error
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 505: return "HTTP Version Not Supported";
        
        default:  return "Unknown Status";
    }
}

// Setters for generating the response
void HttpResponse::setStatusCode(int code) {
    _statusCode = code;
	_statusMessage = getStatusMessage(code);
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
	_headers[key] = value;
}

void HttpResponse::setBody(const std::vector<char>& body) {
    _body = body;
}

void HttpResponse::setBody(const std::string& body) {
    _body.assign(body.begin(), body.end());
}

// Converts the object into raw bytes ready to pass to send()
std::vector<char> HttpResponse::createResponse() const {
    std::vector<char> response;

    // 1. Status Line (e.g., "HTTP/1.1 200 OK\r\n")
    std::string version = _version.empty() ? "HTTP/1.0" : _version;
    
    // C++98 compliant integer-to-string conversion
    std::ostringstream oss;
    oss << _statusCode;
    std::string statusCodeStr = oss.str();

    std::string statusLine = version + " " + statusCodeStr + " " + _statusMessage + "\r\n";
    response.insert(response.end(), statusLine.begin(), statusLine.end());

    // 2. Headers (e.g., "Content-Type: text/html\r\n")
    // C++98 compliant iterator loop (auto and range-based loops are forbidden)
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        std::string headerLine = it->first + ": " + it->second + "\r\n";
        response.insert(response.end(), headerLine.begin(), headerLine.end());
    }

    // 3. Mandatory empty line separating headers from the body ("\r\n")
    std::string crlf = "\r\n";
    response.insert(response.end(), crlf.begin(), crlf.end());

    // 4. Body (appended as raw bytes)
    response.insert(response.end(), _body.begin(), _body.end());

    return response;
}

#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <cctype>
#include <unistd.h>

class HttpRequest {
public:
	static const size_t MAX_URI_LENGTH = 8192;

    enum ParsingState {
        PARSE_REQUEST_LINE,
        PARSE_HEADERS,
        PARSE_BODY,
        PARSE_DONE,
        PARSE_ERROR
    };

private:
    ParsingState _parseState;
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::vector<char> _body;
    size_t _contentLength;
    int _errorCode;
	bool _isChunked;

public:
    HttpRequest();
	HttpRequest(const HttpRequest& other);
	HttpRequest& operator=(const HttpRequest& other);
    ~HttpRequest();
	
    // Setters for building the request during parsing
    void setRequestState(ParsingState state);
    void setErrorCode(int code);
    void setMethod(const std::string& method);
    void setUri(const std::string& uri);
    void setVersion(const std::string& version);
    void setContentLength(size_t len);
    void addHeader(const std::string& key, const std::string& value);
    void appendBody(const char* data, size_t size);

    // Getters
	ParsingState getRequestState() const;
    int getErrorCode() const;
	bool isChunked() const;
    const std::string&                  getMethod() const;
    const std::string&                  getUri() const;
    const std::string&                  getVersion() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::vector<char>&            getBody() const;
    size_t                              getContentLength() const;
};

#endif
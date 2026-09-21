#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <cctype>

class HttpRequest {
public:
    enum ParsingState {
        PARSE_REQUEST_LINE,
        PARSE_HEADERS,
        PARSE_BODY,
        PARSE_DONE,
        PARSE_ERROR
    };

private:
    ParsingState                        _parseState;
    std::string                         _method;
    std::string                         _uri;
    std::string                         _version;
    std::map<std::string, std::string>  _headers;
    std::vector<char>                   _body;
    size_t                              _contentLength;
    int                                 _errorCode;

public:
    HttpRequest();
	HttpRequest(const HttpRequest& other);
	HttpRequest& operator=(const HttpRequest& other);
    ~HttpRequest();

    // State and parsing helpers
    ParsingState getRequestState() const;
    void setRequestState(ParsingState state);
    void setErrorCode(int code);
    int getErrorCode() const;

    // Setters for building the request during parsing
    void setMethod(const std::string& method);
    void setUri(const std::string& uri);
    void setVersion(const std::string& version);
    void addHeader(const std::string& key, const std::string& value);
    void appendBody(const char* data, size_t size);
    void setContentLength(size_t len);

    // Getters for routing and processing
    const std::string&                  getMethod() const;
    const std::string&                  getUri() const;
    const std::string&                  getVersion() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::vector<char>&            getBody() const;
    size_t                              getContentLength() const;
};

#endif
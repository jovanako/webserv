#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>
#include <vector>

class HttpResponse {
private:
    int                                 _statusCode;
    std::string                         _statusMessage;
    std::string                         _version;
    std::map<std::string, std::string>  _headers;
    std::vector<char>                   _body;

    std::string getStatusMessage(int code) const;

public:
    HttpResponse();
    ~HttpResponse();

    // Setters for generating the response
    void setStatusCode(int code);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::vector<char>& body);
    void setBody(const std::string& body);

    // Converts the object into raw bytes ready to pass to send()
    std::vector<char> createResponse() const;
};

#endif
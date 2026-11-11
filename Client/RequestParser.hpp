#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include "../HttpRequest.hpp"
#include "../HttpResponse.hpp"
#include "../ServerConfig.hpp"
#include <string>
#include <sstream>

class RequestParser
{
private:
    std::string _readBuffer;
    HttpRequest _request;
    
	bool parseRequestLineParts(const std::string& requestLine,
			std::string& method, std::string& uri, std::string& version) const;
	bool applyRequestLine(const std::string& method, const std::string& uri,
			const std::string& version, HttpResponse& response);
	bool parseRequestLine(size_t& firstLineEnd, HttpResponse& response);
	bool storeParsedHeader(const std::string& key,
			const std::string& value, HttpResponse& response);
	bool parseHeaderLine(const std::string& line, HttpResponse& response);

    bool readChunkSize(size_t& lineEnd, size_t& chunkSize, HttpResponse& response);
	bool finishChunkedBody(size_t chunkLineEnd, HttpResponse& response);
	bool appendChunkData(size_t chunkLineEnd, size_t chunkSize, HttpResponse& response);

public:
    RequestParser();
    ~RequestParser();
    RequestParser(const RequestParser& other);
    RequestParser& operator=(const RequestParser& other);

    void appendData(const char* buf, ssize_t bytesRead);
    void eraseBuffer(size_t pos, size_t len);
    void reset();

    HttpRequest& getRequest();
    const HttpRequest& getRequest() const;
    const std::string& getReadBuffer() const;
    bool hasHeaderEnd(size_t& headerEnd) const;

    bool parseHeaders(HttpResponse& response);
    bool parseChunkedBody(HttpResponse& response, size_t maxBodySize);
    bool processFixedLengthBody(size_t maxBodySize, HttpResponse& response);
};

#endif
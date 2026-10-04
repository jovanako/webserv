#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>
#include <vector>

class HttpResponse {
	private:
		std::string							_version;
		int									_statusCode;
		std::string							_statusMessage;
		std::map<std::string, std::string>	_headers;
		std::vector<char>					_body;
		
	public:
		HttpResponse();
		HttpResponse(const HttpResponse& other);
		HttpResponse& operator=(const HttpResponse& other);
		~HttpResponse();
		
		std::string			getVersion() const;
		int					getStatusCode() const;
		std::string			getStatusMessage(int code) const;
		std::vector<char>	getBody() const;
		
		void setVersion(const std::string& version);
		void setStatusCode(int code);
		void setHeader(const std::string& key, const std::string& value);
		void setBody(const std::vector<char>& body);
		void setBody(const std::string& body);

		std::vector<char> createResponse() const;
};

#endif
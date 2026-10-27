#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include "../HttpRequest.hpp"
#include "../HttpResponse.hpp"
#include "../ServerConfig.hpp"
#include "RequestHandler.hpp"
#include <string>
#include <map>
#include <fstream>
#include <sstream>

class ResponseBuilder {
    private:
	    bool		loadCustomErrorPage(const ServerConfig& server, int statusCode, std::string& body,
			std::string& mimeType) const;
		std::string	createDefaultErrorPage(int statusCode, const HttpResponse& response) const;
		void		applyErrorResponseBody(HttpResponse& response, const std::string& body,
			const std::string& mimeType);    
        bool		shouldKeepAlive(const HttpRequest& request, const HttpResponse& response) const;

    public:
        ResponseBuilder();
        ~ResponseBuilder();
        ResponseBuilder(const ResponseBuilder& other);
        ResponseBuilder& operator=(const ResponseBuilder& other);

		void buildErrorResponse(int statusCode, HttpResponse& response, const ServerConfig& server, const HttpRequest& request);
        void		buildResponse(HttpResponse& response, const HttpRequest& request);
};

#endif
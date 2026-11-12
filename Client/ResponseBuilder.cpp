#include "ResponseBuilder.hpp"

ResponseBuilder::ResponseBuilder() {

}

ResponseBuilder::~ResponseBuilder() {

}

ResponseBuilder::ResponseBuilder(const ResponseBuilder& other) {
    *this = other;
}

ResponseBuilder& ResponseBuilder::operator=(const ResponseBuilder& other) {
    (void)other;
    return *this;
}

bool ResponseBuilder::loadCustomErrorPage(const ServerConfig& server, const HttpRequest& request, int statusCode,
	std::string& body, std::string& mimeType) const {
	std::string filePath;
	if (!request.getUri().empty()) {
		RequestHandler handler;
		const LocationConfig* matchedLocation = handler.findMatchingLocation(server, request.getUri());
		if (matchedLocation) {
			const std::map<int, std::string>& locErrors = matchedLocation->getErrorPages();
			std::map<int, std::string>::const_iterator locIt = locErrors.find(statusCode);
			if (locIt != locErrors.end()) {
				filePath = locIt->second;
			}
		}
	}

	if (filePath.empty()) {
		const std::map<int, std::string>& errorPages = server.getErrorPages();
		std::map<int, std::string>::const_iterator it = errorPages.find(statusCode);
		if (it == errorPages.end()) {
			return false;
		}
		filePath = it->second;
	}

	struct stat st;
	if (stat(filePath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
		return false;
	}

	std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	body = stream.str();
	mimeType = RequestHandler::getMimeType(filePath);
	return true;
}

std::string ResponseBuilder::createDefaultErrorPage(int statusCode, const HttpResponse& response) const {
	std::ostringstream stream;
	stream << "<!DOCTYPE html><html><head><title>" << statusCode << " "
		<< response.getStatusMessage(statusCode)
		<< "</title></head><body><center><h1>" << statusCode << " "
		<< response.getStatusMessage(statusCode)
		<< "</h1></center><hr><center>webserv</center></body></html>";
	return stream.str();
}

void ResponseBuilder::buildErrorResponse(int statusCode, HttpResponse& response, const ServerConfig& server, const HttpRequest& request) {
	response.setStatusCode(statusCode);

	std::string body;
	std::string mimeType = "text/html";
	if (!loadCustomErrorPage(server, request, statusCode, body, mimeType)) {
		body = createDefaultErrorPage(statusCode, response);
    }

	response.setHeader("Content-Type", mimeType);
	response.setBody(body);

    buildResponse(response, request);
}

bool ResponseBuilder::shouldKeepAlive(const HttpRequest& request, const HttpResponse& response) const {
	if (response.getStatusCode() >= 400 && response.getStatusCode() != 404 && response.getStatusCode() != 405) {
		return false;
	}

	std::map<std::string, std::string> headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("connection");
	std::string connVal = "";
	if (it != headers.end()) {
		connVal = it->second;
	}

	for (size_t i = 0; i < connVal.length(); ++i) {
		connVal[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(connVal[i])));
	}

	if (request.getVersion() == "HTTP/1.1") {
		return (connVal != "close");
	} else if (request.getVersion() == "HTTP/1.0") {
		return (connVal == "keep-alive");
	}
	return false;
}

void ResponseBuilder::buildResponse(HttpResponse& response, const HttpRequest& request) {
	if (shouldKeepAlive(request, response)) {
		response.setHeader("Connection", "keep-alive");
	} else {
		response.setHeader("Connection", "close");
	}

	std::ostringstream oss;
	oss << response.getBody().size();
	response.setHeader("Content-Length", oss.str());
}

#include "RequestHandler.hpp"

RequestHandler::RequestHandler() {}

RequestHandler::~RequestHandler() {}

RequestHandler::RequestHandler(const RequestHandler& other) {
    *this = other;
}

RequestHandler& RequestHandler::operator=(const RequestHandler& other) {
    (void)other;
    return *this;
}

std::string RequestHandler::getFileExtension(const std::string& path) {
	size_t dotPos = path.find_last_of('.');
	size_t slashPos = path.find_last_of('/');

	// if there is no dot or the dot is part of a folder name
	if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos)) {
		return "";
	}

	return path.substr(dotPos);
}

std::string RequestHandler::lowercaseExtension(const std::string& extension) {
    std::string lowerExtension = extension;
	for (size_t i = 0; i < lowerExtension.length(); ++i) {
		lowerExtension[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(lowerExtension[i])));
	}
	return lowerExtension;
}

std::string RequestHandler::mimeTypeForExtension(const std::string& extension) {
	if (extension == ".html" || extension == ".htm") return "text/html";
	if (extension == ".css") return "text/css";
	if (extension == ".js") return "application/javascript";
	if (extension == ".json") return "application/json";
	if (extension == ".png") return "image/png";
	if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
	if (extension == ".gif") return "image/gif";
	if (extension == ".ico") return "image/x-icon";
	if (extension == ".txt") return "text/plain";
	if (extension == ".pdf") return "application/pdf";

	// fallback for anything else
	return "application/octet-stream";
}

std::string RequestHandler::getMimeType(const std::string& path) {
	std::string ext = getFileExtension(path);
	if (ext.empty()) {
		return "application/octet-stream";
	}

	// convert extension to lowercase to handle ".PNG" or ".Html" safely
	ext = lowercaseExtension(ext);
	return mimeTypeForExtension(ext);
}

int RequestHandler::processRequest(const HttpRequest& request, HttpResponse& response, const ServerConfig& server) const {
    std::string uri = request.getUri();
	const LocationConfig* matchedLocation = findMatchingLocation(server, uri);
	if(!matchedLocation) {
        return 404;
    }

    std::string method = request.getMethod();
    if(!isMethodAllowed(*matchedLocation, method)) {
        return 405;
    }
    int redirectCode = handleRedirect(*matchedLocation, response);
    if (redirectCode > 0) {
        return redirectCode;
    }
    if(tryCgiHandler(*matchedLocation, uri)) {
        return 0;
    }
    std::string fullPath = request.makeFullPath(*matchedLocation);
    return dispatchMethod(*matchedLocation, method, fullPath, uri, request, response);
}

const LocationConfig* RequestHandler::findMatchingLocation(const ServerConfig& server, const std::string& uri) const {
	const LocationConfig* matchedLocation = NULL;
	size_t longestMatch = 0;

	const std::vector<LocationConfig>& locations = server.getLocations();
	for (std::vector<LocationConfig>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		
		std::string locPath = it->getPath();

		if (uri.find(locPath) == 0 && locPath.length() > longestMatch) {

			bool isValidMatch = (locPath == "/") ||
								(uri.length() == locPath.length()) ||
								(locPath[locPath.length() - 1] == '/') ||
								(uri[locPath.length()] == '/');
			
			if (isValidMatch) {
				longestMatch = locPath.length();
				matchedLocation = &(*it);
			}
		}
	}
	return matchedLocation;
}

bool		RequestHandler::isMethodAllowed(const LocationConfig& location, const std::string& method) const {
	const std::vector<std::string> allowedMethods = location.getAllowedMethods();
	for (std::vector<std::string>::const_iterator it = allowedMethods.begin();
		it != allowedMethods.end(); ++it) {
		if (*it == method) {
			return true;
		}
	}
	return false;
}

int RequestHandler::handleRedirect(const LocationConfig& location, HttpResponse& response) const {
    const std::pair<int, std::string> redirect = location.getRedirect();
	if (redirect.first == 0 || redirect.second == "") {
		return 0;
	}

	if (redirect.first >= 300 && redirect.first < 400) {
		response.setStatusCode(redirect.first);
		response.setHeader("Location", redirect.second);
		return redirect.first;
	}

	return 500;
}

bool		RequestHandler::tryCgiHandler(const LocationConfig& location, const std::string& uri) const {
	std::string scriptName = location.resolveScriptName(uri);
	if (scriptName.empty()) {
		return false;
	}
	std::string handler = location.getCgiHandler(scriptName);
	return !handler.empty();
}

int RequestHandler::dispatchMethod(const LocationConfig& location, const std::string& method, const std::string& fullPath, const std::string& uri, const HttpRequest& request, HttpResponse& response) const {
	if (method == "GET") {
		return handleGet(location, fullPath, uri, response);
	}

	if (method == "POST") {
			return handlePost(location, uri, request, response);
	}

	if (method == "DELETE") {
		return handleDelete(fullPath, response);
	}
	return 405;
}

int RequestHandler::handleGet(const LocationConfig& matchedLocation, std::string fullPath, const std::string uri, HttpResponse& response) const {
    struct stat pathStat;
	// check if file exists
	if (stat(fullPath.c_str(), &pathStat) == 0) {
		return serveExistingPath(matchedLocation, fullPath, pathStat, uri, response);
	}
	return 404;
}

int	RequestHandler::handlePost(const LocationConfig& matchedLocation, const std::string uri, const HttpRequest& request, HttpResponse& response) const {
	if (matchedLocation.getUploadStore().empty() && matchedLocation.getRoot().empty()) {
		response.setStatusCode(200);
		response.setHeader("Content-Length", "0");
		return 200;
	}

	std::string path = matchedLocation.getPath();
	std::string strippedUri = uri.substr(path.length());
	if (strippedUri == "" || strippedUri[strippedUri.length() - 1] == '/') {
		response.setStatusCode(200);
		response.setHeader("Content-Length", "0");
		return 200;
	}
	
	std::string savePath = buildUploadPath(matchedLocation, uri);

	// open an output file stream in binary mode to prevent corruption
	std::ofstream outFile(savePath.c_str(), std::ios::out | std::ios::binary);

	if (!outFile.is_open()) {
		return 500; // check error code
	}

	if (!writeUploadBody(outFile, savePath, request)) {
		return 500;
	}
	outFile.close();

	setUploadResponse(uri, response);
	return 201;
}

int	RequestHandler::handleDelete(const std::string& fullPath, HttpResponse& response) const {
	struct stat pathStat;
	// check if file exists
	if (stat(fullPath.c_str(), &pathStat) < 0) {
		return 404; // Not found
	}
		// std::remove returns 0 on success, and non-zero on failure
	if (std::remove(fullPath.c_str()) == 0) {
		response.setStatusCode(204);
		response.setHeader("Content-Length", "0");
        return 204;
	}
	return 403;
}

bool RequestHandler::findIndexFile(const LocationConfig& matchedLocation, std::string& fullPath, struct stat& pathStat) const {
	const std::vector<std::string>& indices = matchedLocation.getIndex();
	for (size_t i = 0; i < indices.size(); ++i) {
		std::string indexPath = fullPath + indices[i];
		struct stat indexStat;

		if (stat(indexPath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
			fullPath = indexPath;
			pathStat = indexStat;
			return true;
		}
	}
	return false;
}


static std::string autoindexBaseUri(const std::string& requestUri) {
	std::string baseUri = requestUri;
	if (!baseUri.empty() && baseUri[baseUri.length() - 1] != '/') {
		baseUri += "/";
	}
	return baseUri;
}

static void appendAutoindexHeader(std::ostringstream& html,
	const std::string& requestUri) {
	html << "<!DOCTYPE html>\n"
		 << "<html>\n<head>\n"
		 << "<title>Index of " << requestUri << "</title>\n"
		 << "</head>\n<body>\n"
		 << "<h1>Index of " << requestUri << "</h1>\n"
		 << "<hr>\n<pre>\n";
}

static void appendAutoindexEntries(std::ostringstream& html, DIR* dir,
	const std::string& baseUri) {
	struct dirent* entry = readdir(dir);
	while (entry != NULL) {
		std::string filename = entry->d_name;
		// absolute path
		std::string href = baseUri + filename;
		// make it a link
		html << "<a href=\"" << href << "\">" << filename << "</a>\n";
		entry = readdir(dir);
	}
}

static void appendAutoindexFooter(std::ostringstream& html) {
	html << "</pre>\n<hr>\n</body>\n</html>";
}

static std::string generateAutoindex(const std::string& fullPath, const std::string& requestUri) {
	DIR* dir = opendir(fullPath.c_str());
	if(dir == NULL) {
		return ("");
	}

	std::ostringstream html;
	appendAutoindexHeader(html, requestUri);
	std::string baseUri = autoindexBaseUri(requestUri);
	appendAutoindexEntries(html, dir, baseUri);
	closedir(dir);
	appendAutoindexFooter(html);

	return html.str();
}

int RequestHandler::serveAutoindex(const LocationConfig& matchedLocation, const std::string& fullPath, const std::string& uri, HttpResponse& response) const {
    if (!matchedLocation.getAutoindex()) {
		return 403;
	}

	std::string htmlBody = generateAutoindex(fullPath, uri);
	// opendir failed (likely due to permissions)
	if (htmlBody.empty()) {
		return 403;
	}

	// successfully generated the listing; set the response data
	response.setStatusCode(200);
	response.setBody(htmlBody);

	std::ostringstream lengthStream;
	lengthStream << htmlBody.length();
	response.setHeader("Content-Type", "text/html");
	response.setHeader("Content-Length", lengthStream.str());
	//todo build and send response
	return 200;
}

int RequestHandler::serveFile(const std::string& fullPath, HttpResponse& response) const {
	// std::ios::in mode flag telling the stream to open file for reading (input)
	// std::ios::binary mode flag that forces C++ to read the file byte-by-byte (not in text mode)
	std::ifstream file(fullPath.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		return 403;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	std::string fileContent = stream.str();
	file.close();

	response.setStatusCode(200);
	response.setBody(fileContent);

	std::ostringstream lengthStream;
	lengthStream << fileContent.length();
	response.setHeader("Content-Length", lengthStream.str());
	response.setHeader("Content-Type", getMimeType(fullPath));
	return 200;
}

int RequestHandler::redirectDirectoryRequest(const std::string& uri, HttpResponse& response) const {
    // enforce trailing slash on the URI
	if (uri.empty() || uri[uri.length() - 1] == '/') {
		return 0;
	}

	response.setStatusCode(301);
	response.setHeader("Location", uri + "/");
	return 301;
}

int RequestHandler::serveExistingPath(const LocationConfig& matchedLocation, std::string& fullPath, struct stat& pathStat, const std::string& uri, HttpResponse& response) const {
	// check if it's a directory
	if (S_ISDIR(pathStat.st_mode)) {
        int redirect = redirectDirectoryRequest(uri, response);
        if(redirect) {
			return redirect;
		}
		if (!findIndexFile(matchedLocation, fullPath, pathStat)) {
			return serveAutoindex(matchedLocation, fullPath, uri, response);
		}
	} if (S_ISREG(pathStat.st_mode)) {
		return serveFile(fullPath, response);
	}

	return 403;
}

std::string RequestHandler::buildUploadPath(const LocationConfig& matchedLocation, const std::string& uri) const {
	size_t lastSlash = uri.find_last_of('/');
	std::string filename;
	if (lastSlash != std::string::npos) {
		filename = uri.substr(lastSlash);
	} else {
		filename = "/" + uri;
	}

	// create absolute path to save the file
	std::string uploadStore = matchedLocation.getUploadStore();
	if (uploadStore != "") {
		return uploadStore + filename;
	}
	return matchedLocation.getRoot() + filename;
}

bool RequestHandler::writeUploadBody(std::ofstream& outFile, const std::string& savePath, const HttpRequest& request) const {
	if (!request.getBody().writeTo(outFile)) {
		outFile.close();
		std::remove(savePath.c_str());
		return false;
	}
	return true;
}

void	    RequestHandler::setUploadResponse(const std::string& uri, HttpResponse& response) const {
    response.setStatusCode(201);
	response.setHeader("Content-Length", "0");
	response.setHeader("Location", uri);
}


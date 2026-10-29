#include "Client.hpp"
#include <sstream>

Client::Client() 
	: _socketFd(-1),
	  _clientState(READING_HEADER),
	  _bytesSent(0),
	  _clientIp("")
  {}

Client::Client(int fd)
	: _socketFd(fd),
	  _clientState(READING_HEADER),
	  _bytesSent(0),
	  _clientIp("") {}

Client::Client(const Client& other) {
	*this = other;
}

Client& Client::operator=(const Client& other) {
	if(this != &other)
	{
		_socketFd = other._socketFd;
		_clientState = other._clientState;
		_parser = other._parser;
		_handler = other._handler;
		_responseBuilder = other._responseBuilder;
		_response = other._response;
		_server = other._server;
		_writeBuffer = other._writeBuffer;
		_bytesSent = other._bytesSent;
		_virtualHosts = other._virtualHosts;
		_clientIp = other._clientIp;
	}
	return *this;
}

Client::~Client() {}

Client::ConnectionState Client::getClientState() const {
	return _clientState;
}

int Client::getSocketFd() const {
	return _socketFd;
}

const ServerConfig& Client::getServer() const {
	return _server;
}

const std::vector<ServerConfig>& Client::getVirtualHosts() const {
	return _virtualHosts;
}

const std::string& Client::getClientIp() const {
	return _clientIp;
}

void Client::setClientState(ConnectionState state) {
	_clientState = state;
}

void Client::setServer(const ServerConfig& server) {
	_server = server;
}

void Client::setVirtualHosts(const std::vector<ServerConfig>& hosts) {
	_virtualHosts = hosts;
}

void Client::setClientIp(const std::string& clientIp) {
	_clientIp = clientIp;
	_parser.getRequest().setRemoteAddr(_clientIp);
}

/*static std::string getFileExtension(const std::string& path) {
	// find the last dot and the last slash
	size_t dotPos = path.find_last_of('.');
	size_t slashPos = path.find_last_of('/');

	// if there is no dot or the dot is part of a folder name
	if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos)) {
		return "";
	}

	// extract the extension
	return path.substr(dotPos);
}

static std::string lowercaseExtension(const std::string& extension) {
	std::string lowerExtension = extension;
	for (size_t i = 0; i < lowerExtension.length(); ++i) {
		lowerExtension[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(lowerExtension[i])));
	}
	return lowerExtension;
}

static std::string mimeTypeForExtension(const std::string& extension) {
	// Map common extensions
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

static std::string getMimeType(const std::string& path) {
	std::string ext = getFileExtension(path);
	if (ext.empty()) {
		return "application/octet-stream";
	}

	// convert extension to lowercase to handle ".PNG" or ".Html" safely
	ext = lowercaseExtension(ext);
	return mimeTypeForExtension(ext);
}

bool Client::findHeaderEnd(size_t& headerEnd) const {
	headerEnd = _readBuffer.find("\r\n\r\n");
	return headerEnd != std::string::npos;
}*/

bool Client::readHeaderChunk() {
	char buf[BUFFER_SIZE];
	ssize_t bytesRead = recv(_socketFd, buf, sizeof(buf), 0);
	if (bytesRead < 0) {
		// if there's simply no data right now, don't close the connection
		// just return and let poll() wait for the next event
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return false;
		}

		// for actual network errors (e.g., ECONNRESET), mark as done to trigger cleanup
		std::cerr << "Read error on fd " << _socketFd << " (errno: " << errno << ")\n";
		_clientState = DONE;
		return false;
	} else if (bytesRead == 0) {
		std::cout << "Client disconnected on fd " << _socketFd << "\n";
		_clientState = DONE;
		return false;
	}

	_parser.appendData(buf, bytesRead);
	return true;
}

/*bool Client::receiveHeaderData(size_t& headerEnd) {
	if (findHeaderEnd(headerEnd)) {
		return true;
	}

	if (!readHeaderChunk()) {
		return false;
	}
	return findHeaderEnd(headerEnd);
}*/

bool Client::getHostHeaderValue(std::string& hostValue) {
	if (_parser.getRequest().getVersion() != "HTTP/1.1") {
		return true;
	}

	const std::map<std::string, std::string>& headers = _parser.getRequest().getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("host");
	if (it == headers.end()) {
		_response.setStatusCode(400); // 400 Bad Request strictly required for HTTP/1.1
		finalizeResponse();
		return false;
	}

	hostValue = it->second;
	return true;
}

std::string Client::extractHostName(const std::string& hostValue) const {
	// checking if we got the right server block (in case we are listening on the same port more than once)
	size_t colonPos = hostValue.find(':');
	if (colonPos != std::string::npos) {
		return hostValue.substr(0, colonPos);
	}
	return hostValue;
}

void Client::applyVirtualHostSelection(const std::string& hostName) {
	for (size_t i = 0; i < _virtualHosts.size(); ++i) {
		if (_virtualHosts[i].getPort() == _server.getPort()) {
			const std::vector<std::string>& serverNames = _virtualHosts[i].getServerNames();
			bool matchFound = false;

			for (size_t j = 0; j < serverNames.size(); ++j) {
				if (serverNames[j] == hostName) {
					_server = _virtualHosts[i];
					matchFound = true;
					break;
				}
			}
			if (matchFound) {
				break;
			}
		}
	}
}

bool Client::selectVirtualHost() {
	std::string hostValue = "";
	if (!getHostHeaderValue(hostValue)) {
		return false;
	}

	std::string hostName = extractHostName(hostValue);
	applyVirtualHostSelection(hostName);
	return true;
}

void Client::prepareRequestBody(size_t headerEnd) {
	// Only keep leftover bytes that belong to the body by deleting the header
	_parser.eraseBuffer(0, headerEnd + 4);

	// If there is a body, handle it
	if (_parser.getRequest().getContentLength() > 0 || _parser.getRequest().isChunked()) {
		_clientState = READING_BODY;

		// If we have already read part of the body
		if (!_parser.getReadBuffer().empty()) {
			handleReadBody();
		}
	}
	// Otherwise, if there is no body
	else {
		_clientState = PROCESSING;
	}
}

void Client::handleReadHeader() {
	size_t headerEnd;
	//walk me through this later
	if(!_parser.hasHeaderEnd(headerEnd) && !readHeaderChunk()) {
		return;
	}
	if(_parser.hasHeaderEnd(headerEnd)) {
		if(!_parser.parseHeaders(_response)) {
			if (_response.getStatusCode() >= 400) {
				finalizeResponse();
			}
			return;
		}
	/*if (receiveHeaderData(headerEnd)) {
		// change state?
		//parseHeaders();
		if (_clientState == WRITING_RESPONSE) {
			return; // exit immediately so the error response can be sent
		}
*/
		if (_parser.getRequest().getContentLength() > _server.getClientMaxBodySize()) {
			_response.setStatusCode(413);
			// 413 Payload too large error
			finalizeResponse();
			return;
		}

		if (!selectVirtualHost()) {
			return;
		}

		prepareRequestBody(headerEnd);
	}
}

/*bool Client::readChunkSize(size_t& lineEnd, size_t& chunkSize) {
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
		buildErrorResponse(400); // bad request
		return false;
	}
	return true;
}

void Client::finishChunkedBody(size_t chunkLineEnd) {
	// check if the final "\r\n\r\n" is completely in the buffer
	if (_readBuffer.length() >= chunkLineEnd + 4 &&
		_readBuffer.substr(chunkLineEnd, 4) == "\r\n\r\n") {
		_readBuffer.erase(0, chunkLineEnd + 4);
		_clientState = PROCESSING;
	}
}

bool Client::appendChunkData(size_t chunkLineEnd, size_t chunkSize) {
	// check if the entire chunk data + trailing "\r\n" has been received
	if (_readBuffer.length() < chunkLineEnd + 2 + chunkSize + 2) {
		return false; // wait for the rest of the chunk
	}

	// append the chunk data to the request
	_request.appendBody(_readBuffer.data() + chunkLineEnd + 2, chunkSize);

	// erase the processed chunk size, chunk data, and both \r\n from the buffer
	_readBuffer.erase(0, chunkLineEnd + 2 + chunkSize + 2);
	return true;
}*/

bool Client::readBodyChunk() {
	char buf[BUFFER_SIZE];
	ssize_t bytesRead = recv(_socketFd, buf, sizeof(buf), 0);
		
	if (bytesRead < 0) {
		// if there's simply no data right now, don't close the connection
		// just return and let poll() wait for the next event
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return false;
		}
		// for actual network errors (e.g., ECONNRESET), mark as done to trigger cleanup
		std::cerr << "Read error on fd " << _socketFd << " (errno: " << errno << ")\n";
		_clientState = DONE;
		return false;
	} else if (bytesRead == 0) {
		std::cout << "Client disconnected on fd " << _socketFd << "\n";
		_clientState = DONE;
		return false;
	}

	_parser.appendData(buf, bytesRead);
	return true;
}

/*void Client::parseChunkedBody() {
	while (true) {
		size_t pos;
		size_t chunkSize;
		if (!readChunkSize(pos, chunkSize)) {
			return;
		}

		// a chunk size of 0 indicates the end of the request body
		if (chunkSize == 0) {
			finishChunkedBody(pos);
			return; // wait for final "\r\n" if not fully received yet
		}

		if (!appendChunkData(pos, chunkSize)) {
			return;
		}
	}
}

void Client::processFixedLengthBody() {
	size_t contentLen = _request.getContentLength();

	if (_readBuffer.length() > _server.getClientMaxBodySize()) {
		buildErrorResponse(413); // pazload too large
		return;
	}

	if (_readBuffer.length() >= contentLen) {
		_request.appendBody(_readBuffer.data(), contentLen);
		_readBuffer.erase(0, contentLen);
		_clientState = PROCESSING;
	}
}*/

void Client::handleReadBody() {
	bool finishedParsing = false;
	if (_parser.getRequest().isChunked()) {
		finishedParsing = _parser.parseChunkedBody(_response);
	} else {
		finishedParsing= _parser.processFixedLengthBody(_server, _response);
	}

	if(finishedParsing) {
		_clientState = PROCESSING;
	} else if (_response.getStatusCode() >= 400) {
		finalizeResponse();
	}
}

/*bool Client::loadCustomErrorPage(int statusCode, std::string& body,
	std::string& mimeType) const {
	const std::map<int, std::string>& errorPages = _server.getErrorPages();
	std::map<int, std::string>::const_iterator it = errorPages.find(statusCode);
	if (it == errorPages.end()) {
		return false;
	}

	std::string filePath = it->second;
	// prepend root or server path if needed
	std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	body = stream.str();
	mimeType = _handler.getMimeType(filePath);
	return true;
}

std::string Client::createDefaultErrorPage(int statusCode) const {
	std::ostringstream stream;
	stream << "<!DOCTYPE html><html><head><title>" << statusCode << " "
		<< _response.getStatusMessage(statusCode)
		<< "</title></head><body><center><h1>" << statusCode << " "
		<< _response.getStatusMessage(statusCode)
		<< "</h1></center><hr><center>webserv</center></body></html>";
	return stream.str();
}

void Client::applyErrorResponseBody(const std::string& body,
	const std::string& mimeType) {
	_response.setHeader("Content-Type", mimeType);
	_response.setBody(body);
	finalizeResponse();
}

void Client::buildErrorResponse(int statusCode) {
	_response.setStatusCode(statusCode);

	std::string body;
	std::string mimeType = "text/html";
	if (!loadCustomErrorPage(statusCode, body, mimeType)) {
		body = createDefaultErrorPage(statusCode);
    }

	applyErrorResponseBody(body, mimeType);
}

void Client::finalizeResponse() {
	if (shouldKeepAlive()) {
		_response.setHeader("Connection", "keep-alive");
	} else {
		_response.setHeader("Connection", "close");
	}

	std::ostringstream oss;
	oss << _response.getBody().size();
	_response.setHeader("Content-Length", oss.str());

	setClientState(WRITING_RESPONSE);
}*/

void Client::setupErrorResponse(int statusCode) {
	_responseBuilder.buildErrorResponse(statusCode, _response, _server, _parser.getRequest());
	setClientState(WRITING_RESPONSE);
}

void Client::finalizeResponse() {
	_responseBuilder.buildResponse(_response, _parser.getRequest());
	setClientState(WRITING_RESPONSE);
}

void Client::resetForNextRequest() {
	_parser.reset();
	_parser.getRequest().setRemoteAddr(_clientIp);
	_response = HttpResponse();
	_writeBuffer.clear();
	_bytesSent = 0;
	_clientState = READING_HEADER;
}
/*
static std::string autoindexBaseUri(const std::string& requestUri) {
	std::string baseUri = requestUri;
	if (!baseUri.empty() && baseUri[baseUri.length() - 1] != '/') {
		baseUri += "/";
	}
	return baseUri;
}

static void appendAutoindexHeader(std::ostringstream& html,
	const std::string& requestUri) {
	// skeleton of HTML
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

std::string generateAutoindex(const std::string& fullPath, const std::string& requestUri) {
	DIR* dir = opendir(fullPath.c_str());
	if(dir == NULL) {
		return (""); //this will be a 403
	}

	std::ostringstream html;
	appendAutoindexHeader(html, requestUri);
	std::string baseUri = autoindexBaseUri(requestUri);
	appendAutoindexEntries(html, dir, baseUri);
	closedir(dir);
	appendAutoindexFooter(html);

	return html.str();
}

bool Client::findIndexFile(const LocationConfig& matchedLocation,
	std::string& fullPath, struct stat& pathStat) const {
	// search for an index file
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

int Client::serveAutoindex(const LocationConfig& matchedLocation,
	const std::string& fullPath, const std::string& uri) {
	if (!matchedLocation.getAutoindex()) {
		buildErrorResponse(403);
		return 0;
	}

	std::string htmlBody = generateAutoindex(fullPath, uri);
	// opendir failed (likely due to permissions)
	if (htmlBody.empty()) {
		buildErrorResponse(403);
		return 0;
	}

	// successfully generated the listing; set the response data
	_response.setStatusCode(200);
	_response.setBody(htmlBody);

	std::ostringstream lengthStream;
	lengthStream << htmlBody.length();
	_response.setHeader("Content-Type", "text/html");
	_response.setHeader("Content-Length", lengthStream.str());
	//todo build and send response
	return 1;
}

int Client::serveFile(const std::string& fullPath) {
	// std::ios::in mode flag telling the stream to open file for reading (input)
	// std::ios::binary mode flag that forces C++ to read the file byte-by-byte (not in text mode)
	std::ifstream file(fullPath.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		buildErrorResponse(403);
		return 0;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	std::string fileContent = stream.str();
	file.close();

	_response.setStatusCode(200);
	_response.setBody(fileContent);

	std::ostringstream lengthStream;
	lengthStream << fileContent.length();
	_response.setHeader("Content-Length", lengthStream.str());
	// mime-type logic
	_response.setHeader("Content-Type", getMimeType(fullPath));
	return 1;
}

bool Client::redirectDirectoryRequest(const std::string& uri) {
	// enforce trailing slash on the URI
	if (uri.empty() || uri[uri.length() - 1] == '/') {
		return false;
	}

	_response.setHeader("Location", uri + "/");
	buildErrorResponse(301); // moved permanently
	return true;
}

int Client::serveExistingPath(const LocationConfig& matchedLocation,
	std::string& fullPath, struct stat& pathStat, const std::string& uri) {
	bool isFileToServe = false;
	// check if it's a directory
	if (S_ISDIR(pathStat.st_mode)) {
		if (redirectDirectoryRequest(uri)) {
			return 0;
		}
		if (!findIndexFile(matchedLocation, fullPath, pathStat)) {
			return serveAutoindex(matchedLocation, fullPath, uri);
		}
		isFileToServe = true;
	} else if (S_ISREG(pathStat.st_mode)) {
		isFileToServe = true;
	}

	if (isFileToServe) {
		return serveFile(fullPath);
	}

	buildErrorResponse(403);
	return 0;
}

int Client::handleGet(const LocationConfig& matchedLocation, std::string fullPath, const std::string uri) {
	struct stat pathStat;
	// check if file exists
	if (stat(fullPath.c_str(), &pathStat) == 0) {
		return serveExistingPath(matchedLocation, fullPath, pathStat, uri);
	} else {
		//the path does not exist on disc
		buildErrorResponse(404);
		return 0;
	}
	return 1;
}

std::string Client::buildUploadPath(const LocationConfig& matchedLocation,
	const std::string& uri) const {
	size_t lastSlash = uri.find_last_of('/');
	std::string filename;
	if (lastSlash != std::string::npos) {
		filename = uri.substr(lastSlash);
	} else {
		filename = "/" + uri;
	}

	// create absolute path to save the file
	return matchedLocation.getUploadStore() + filename;
}

bool Client::writeUploadBody(std::ofstream& outFile,
	const std::string& savePath) {
	// use write() so that it writes binary data correctly
	const std::vector<char>& body = _parser.getRequest().getBody();
	if (body.empty()) {
		return true;
	}

	outFile.write(&body[0], body.size());
	if (outFile.fail()) {
		outFile.close();
		std::remove(savePath.c_str());
		buildErrorResponse(500); // internal server error
		return false;
	}
	return true;
}

void Client::setUploadResponse(const std::string& uri) {
	_response.setStatusCode(201);
	_response.setHeader("Content-Length", "0");
	_response.setHeader("Location", uri);
}


int Client::handlePost(const LocationConfig& matchedLocation, const std::string uri) {
	std::string savePath = buildUploadPath(matchedLocation, uri);

	// open an output file tream in binary mode to prevent corruption
	std::ofstream outFile(savePath.c_str(), std::ios::out | std::ios::binary);

	if (!outFile.is_open()) {
		buildErrorResponse(403); // forbidden
		return 0;
	}

	if (!writeUploadBody(outFile, savePath)) {
		return 0;
	}
	outFile.close();

	setUploadResponse(uri);
	return 1;
}

int Client::handleDelete(const std::string& fullPath) {
	// std::remove returns 0 on success, and non-zero on failure
	if (std::remove(fullPath.c_str()) == 0) {
		_response.setStatusCode(204); // 204 no content
		_response.setHeader("Content-Length", "0");
	} else {
		buildErrorResponse(403); // deletion failed (file doesn't exist or no permission)
		return 0;
	}
	return 1;
}

const LocationConfig* Client::findMatchingLocation(const std::string& uri) const {
	const LocationConfig* matchedLocation = NULL;
	size_t longestMatch = 0;

	const std::vector<LocationConfig>& locations = _server.getLocations();
	for (std::vector<LocationConfig>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		std::string locPath = it->getPath();
		if (uri.find(locPath) == 0 && locPath.length() > longestMatch) {
			longestMatch = locPath.length();
			matchedLocation = &(*it);
		}
	}
	return matchedLocation;
}

bool Client::isMethodAllowed(const LocationConfig& location,
	const std::string& method) const {
	const std::vector<std::string> allowedMethods = location.getAllowedMethods();
	for (std::vector<std::string>::const_iterator it = allowedMethods.begin();
		it != allowedMethods.end(); ++it) {
		if (*it == method) {
			return true;
		}
	}
	return false;
}

bool Client::handleRedirect(const LocationConfig& location) {
	const std::pair<int, std::string> redirect = location.getRedirect();
	if (redirect.first == 0 || redirect.second == "") {
		return false;
	}

	if (redirect.first >= 300 && redirect.first < 400) {
		_response.setStatusCode(redirect.first); // check if it starts with 3?
		_response.setHeader("Location", redirect.second);
		finalizeResponse();
		return true;
	}

	buildErrorResponse(500);
	return true;
}

bool Client::tryCgiHandler(const LocationConfig& location,
	const std::string& uri) {
	const std::map<std::string, std::string> cgiHandlers = location.getCgiHandlers();
	for (std::map<std::string, std::string>::const_iterator iter = cgiHandlers.begin();
		iter != cgiHandlers.end(); ++iter) {
		const std::string& extension = iter->first;
		if (uri.length() >= extension.length()
			&& uri.compare(uri.length() - extension.length(), extension.length(), extension) == 0) {
			// exeAssigns new contents to the vector, replacing its current contents, and modifying its size accordingly.
			char* const argv[] = {NULL};
			char* const envp[] = {NULL};
			execve(iter->second.c_str(), argv, envp); // TO DO fork() ...
			return true;
		}
	}
	return false;
}

bool Client::dispatchMethod(const LocationConfig& location,
	const std::string& method, const std::string& fullPath,
	const std::string& uri) {
	if (method == "GET") {
		return handleGet(location, fullPath, uri) != 0;
	}

	if (method == "POST") {
		if (location.getUploadStore() != "") {
			return handlePost(location, uri) != 0;
		}
		buildErrorResponse(403); // forbidden
		return false;
	}

	if (method == "DELETE") {
		return handleDelete(fullPath) != 0;
	}
	return true;
}

bool Client::validateLocation(const LocationConfig* location) {
	if (location != NULL) {
		return true;
	}

	_response.setStatusCode(404);
	buildErrorResponse(404); // !!! added 404 not found
	finalizeResponse();
	return false;
}

bool Client::validateMethod(const LocationConfig& location,
	const std::string& method) {
	if (isMethodAllowed(location, method)) {
		return true;
	}

	_response.setStatusCode(405);
	buildErrorResponse(405); // !!! added 405 method not allowed
	finalizeResponse();
	return false;
}*/

void Client::handleProcessing() {
	int statusCode;
	try {
		statusCode = _handler.processRequest(_parser.getRequest(), _response, _server);
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		setupErrorResponse(500);
		return;
	}

	if(statusCode == 0) {
		setClientState(CGI_PIPE_WAIT);
		return;
	}
	if(statusCode >= 400) {
		setupErrorResponse(statusCode);
		return;
	}

	finalizeResponse();
}

/*bool Client::shouldKeepAlive() const {
	if (_response.getStatusCode() >= 400 && _response.getStatusCode() != 404 && _response.getStatusCode() != 405) {
		return false;
	}

	std::map<std::string, std::string> headers = _parser.getRequest().getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.find("connection");
	std::string connVal = "";
	if (it != headers.end()) {
		connVal = it->second;
	}

	for (size_t i = 0; i < connVal.length(); ++i) {
		connVal[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(connVal[i])));
	}

	if (_parser.getRequest().getVersion() == "HTTP/1.1") {
		return (connVal != "close");
	} else if (_parser.getRequest().getVersion() == "HTTP/1.0") {
		return (connVal == "keep-alive");
	}
	return false;
}*/


void Client::handleWriteResponse() {
	if(_bytesSent == 0 && _writeBuffer.empty()) {
		std::vector<char> raw = _response.createResponse();
		_writeBuffer.assign(raw.begin(), raw.end());
	}
	size_t remainingBytes = _writeBuffer.size() - _bytesSent;

	ssize_t bytesWritten = send(_socketFd, &_writeBuffer[_bytesSent], remainingBytes, 0);
	if(bytesWritten <= 0) {
		setClientState(DONE); //handle error?
		return;
	}

	_bytesSent += bytesWritten;

	if(_bytesSent >= _writeBuffer.size()) {
		if(_response.getHeader("Connection") == "keep-alive") {
			resetForNextRequest();
		/*if (shouldKeepAlive()) {
			resetForNextRequest();*/ // set state back to reading header and clears request/response

			size_t dummy; //walk me through this
			if (_parser.hasHeaderEnd(dummy)) {
				handleReadHeader();
			}
			/*if (_readBuffer.find("\r\n\r\n") != std::string::npos) {
				handleReadHeader();
			}*/
		} else {
			setClientState(DONE);
		}
	}
}

void Client::handleCgiPipeWait() {

}

void Client::handleDone() {

}

void Client::handleRead() {
	if (_clientState == READING_HEADER) {
		handleReadHeader();
	} else if (_clientState == READING_BODY) {
		if (readBodyChunk()) {
			handleReadBody();
		}
	}
}

void Client::handleWrite() {
	if (_clientState == WRITING_RESPONSE) {
		handleWriteResponse();
	}
}

/*bool Client::parseRequestLineParts(const std::string& requestLine,
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

bool Client::applyRequestLine(const std::string& method,
	const std::string& uri, const std::string& version) {
	_request.setMethod(method);
	_request.setUri(uri);

	if (version == "HTTP/1.0" || version == "HTTP/1.1") {
		_request.setVersion(version);
		_response.setVersion(version);
	} else {
		_response.setStatusCode(505); // HTTP version not supported
		_request.setRequestState(HttpRequest::PARSE_ERROR);
		finalizeResponse();
		return false;
	}

	if (_request.getRequestState() == HttpRequest::PARSE_ERROR) {
		_response.setStatusCode(_request.getErrorCode());
		finalizeResponse();
		return false;
	}

	_request.setRequestState(HttpRequest::PARSE_HEADERS);
	return true;
}

bool Client::parseRequestLine(size_t& firstLineEnd) {
	// 1. Find the end of the first line
	firstLineEnd = _readBuffer.find("\r\n");
	if (firstLineEnd == std::string::npos) return false; // Safety check

	// Extract just the request line (excluding the \r\n)
	std::string requestLine = _readBuffer.substr(0, firstLineEnd);
	std::string method;
	std::string uri;
	std::string version;
	if (!parseRequestLineParts(requestLine, method, uri, version)) {
		_response.setStatusCode(400); // Bad Request
		finalizeResponse();
		return false;
	}

	return applyRequestLine(method, uri, version);
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

bool Client::storeParsedHeader(const std::string& key,
	const std::string& value) {
	_request.addHeader(key, value);
	if (_request.getRequestState() == HttpRequest::PARSE_ERROR) {
		_response.setStatusCode(_request.getErrorCode());
		finalizeResponse();
		return false;
	}
	return true;
}

bool Client::parseHeaderLine(const std::string& line) {
	// Find the colon in this specific line
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		return true;
	}

	std::string key = line.substr(0, colonPos);
	std::string value = line.substr(colonPos + 1);
	trimHeaderValue(value);

	return storeParsedHeader(key, value);
}

void Client::parseHeaders() {
	size_t firstLineEnd;
	if (!parseRequestLine(firstLineEnd)) {
		return;
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

		if (!parseHeaderLine(line)) {
			return;
		}

		// Move to the next line
		lineStart = lineEnd + 2;
	}
}*/


#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "RequestParser.hpp"
#include "../HttpResponse.hpp"
#include "RequestHandler.hpp"
#include "ResponseBuilder.hpp"
#include "../ServerConfig.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include <dirent.h>
#include <iostream>
#include <cerrno>

class Client {
	public:
		enum ConnectionState {
			READING_HEADER,
			READING_BODY,
			PROCESSING,
			WRITING_RESPONSE,
			CGI_PIPE_WAIT,
			DONE
		};

	private:
		static const int			BUFFER_SIZE = 4096;
		int                 		_socketFd;
		ConnectionState     		_clientState;
		RequestParser				_parser;
		RequestHandler				_handler;
		ResponseBuilder				_responseBuilder;
		HttpResponse        		_response;
		ServerConfig				_server;
		//std::string					_readBuffer;
		std::vector<char>			_writeBuffer;
		size_t						_bytesSent;
		std::vector<ServerConfig>	_virtualHosts;
		std::string					_clientIp;
		// ... buffers, timers, etc.

		void		handleReadHeader();
		//bool		findHeaderEnd(size_t& headerEnd) const;
		bool		readHeaderChunk();
		//bool		receiveHeaderData(size_t& headerEnd);
		bool		getHostHeaderValue(std::string& hostValue);
		std::string	extractHostName(const std::string& hostValue) const;
		void		applyVirtualHostSelection(const std::string& hostName);
		bool		selectVirtualHost();
		void		prepareRequestBody(size_t headerEnd);
		bool		readBodyChunk();
		//?void		processFixedLengthBody();
		void		handleReadBody();

		//bool		loadCustomErrorPage(int statusCode, std::string& body,
		//	std::string& mimeType) const;
		//std::string	createDefaultErrorPage(int statusCode) const;
		//void		applyErrorResponseBody(const std::string& body,
			//const std::string& mimeType);
		void		handleWriteResponse();
		void		handleCgiPipeWait();
		//bool		shouldKeepAlive() const;
		void		finalizeResponse();
		void		resetForNextRequest();

		/*int			handleGet(const LocationConfig& matchedLocation, std::string fullPath, const std::string uri);
		bool		findIndexFile(const LocationConfig& matchedLocation,
			std::string& fullPath, struct stat& pathStat) const;
		int		serveAutoindex(const LocationConfig& matchedLocation,
			const std::string& fullPath, const std::string& uri);
		int		serveFile(const std::string& fullPath);
		bool		redirectDirectoryRequest(const std::string& uri);
		int		serveExistingPath(const LocationConfig& matchedLocation,
			std::string& fullPath, struct stat& pathStat, const std::string& uri);
		std::string buildUploadPath(const LocationConfig& matchedLocation,
			const std::string& uri) const;
		bool		writeUploadBody(std::ofstream& outFile,
			const std::string& savePath);
		void		setUploadResponse(const std::string& uri);
		
		const LocationConfig* findMatchingLocation(const std::string& uri) const;
		bool		isMethodAllowed(const LocationConfig& location,
			const std::string& method) const;
		bool		handleRedirect(const LocationConfig& location);
		bool		tryCgiHandler(const LocationConfig& location,
			const std::string& uri);
		bool		dispatchMethod(const LocationConfig& location,
			const std::string& method, const std::string& fullPath,
			const std::string& uri);*/
		//bool		validateLocation(const LocationConfig* location);
		//bool		validateMethod(const LocationConfig& location,
			//const std::string& method);
		//int			handlePost(const LocationConfig& matchedLocation, const std::string uri);
		//int			handleDelete(const std::string& fullPath);
		
	public:
		Client();
		Client(int fd);
		Client(const Client& other);
		Client& operator=(const Client& other);
		~Client();
		
		void handleRead();
		void handleWrite();
		void handleProcessing();
		void handleDone();
		
		//void buildErrorResponse(int statusCode);
		void setupErrorResponse(int statusCode);

		ConnectionState						getClientState() const;
		int									getSocketFd() const;
		const ServerConfig&					getServer() const;
		const std::vector<ServerConfig>&	getVirtualHosts() const;
		const std::string&					getClientIp() const;

		void setClientState(ConnectionState state);
		void setServer(const ServerConfig& server);
		void setVirtualHosts(const std::vector<ServerConfig>& hosts);
		void setClientIp(const std::string& clientIp);
};

#endif

/*
A robust state machine for a 42 webserv project typically requires 
tracking six distinct phases for every client connection to handle 
non-blocking I/O, chunked data, and CGI execution safely.

READING_HEADERS: The server listens for POLLIN and reads raw bytes from 
the socket into a buffer until it detects the double CRLF (\r\n\r\n) 
delimiter marking the end of the HTTP headers.

READING_BODY: Once headers are parsed, the server continues reading data 
if a Content-Length header is present or if the request uses chunked 
transfer encoding (which must be un-chunked as required by the subject).

PROCESSING: The full request is received. The server evaluates the 
configuration routes, checks allowed methods (GET, POST, DELETE), verifies 
file access permissions, and prepares either a static file response or a 
CGI execution plan.

WAITING_CGI: If the request triggers a script (like PHP or Python), the 
server forks the CGI process, monitors its input/output pipes, and waits 
for execution to complete while ensuring the non-blocking event loop 
doesn't freeze.

WRITING_RESPONSE: The response headers and body are fully generated or 
loaded into a send buffer. The socket's event registration switches from 
POLLIN to POLLOUT to safely flush bytes back to the client.

CLOSING: The response is completely sent (or an error occurred), signaling 
that the server should close the file descriptor, free allocated memory, 
and remove the socket from the poll() array.
*/
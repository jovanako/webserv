#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "RequestParser.hpp"
#include "../HttpResponse.hpp"
#include "RequestHandler.hpp"
#include "ResponseBuilder.hpp"
#include "../ServerConfig.hpp"
#include "../cgi/CgiExecutor.hpp"
#include "../cgi/CgiRequest.hpp"
#include "../cgi/CgiResponse.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include <dirent.h>
#include <iostream>
#include <ctime>

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
		static const time_t			CLIENT_TIMEOUT_SEC = 30;
		
	private:
		static const int			BUFFER_SIZE = 4096;
		int                 		_socketFd;
		ConnectionState     		_clientState;
		RequestParser				_parser;
		RequestHandler				_handler;
		ResponseBuilder				_responseBuilder;
		HttpResponse        		_response;
		CgiExecutor					_cgiExecutor;
		ServerConfig				_server;
		std::vector<char>			_writeBuffer;
		size_t						_bytesSent;
		std::vector<ServerConfig>	_virtualHosts;
		std::string					_clientIp;
		time_t						_lastActivity;

		void		handleReadHeader();
		bool		readChunk();
		bool		getHostHeaderValue(std::string& hostValue);
		std::string	extractHostName(const std::string& hostValue) const;
		void		applyVirtualHostSelection(const std::string& hostName);
		bool		selectVirtualHost();
		void		prepareRequestBody(size_t headerEnd);
		void		handleReadBody();

		
		void		handleWriteResponse();
		void		finalizeResponse();
		void		resetForNextRequest();
		bool		startCgiExecution();
		
		public:
		Client();
		Client(int fd);
		Client(const Client& other);
		Client& operator=(const Client& other);
		~Client();
		
		void handleRead();
		void handleWrite();
		void handleProcessing();
		void handleCgiPipeWait();
		void handleCgiTimeout();
		void cleanupCgi();
		void handleDone();
		bool clientHasTimedOut() const;
		
		void setupErrorResponse(int statusCode);

		ConnectionState						getClientState() const;
		int									getSocketFd() const;
		int									getCgiPipeFd() const;
		bool								hasCgiTimedOut() const;
		const ServerConfig&					getServer() const;
		const std::vector<ServerConfig>&	getVirtualHosts() const;
		const std::string&					getClientIp() const;

		void setClientState(ConnectionState state);
		void setServer(const ServerConfig& server);
		void setVirtualHosts(const std::vector<ServerConfig>& hosts);
		void setClientIp(const std::string& clientIp);
};

#endif
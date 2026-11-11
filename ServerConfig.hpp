#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include "LocationConfig.hpp"

class ServerConfig {
	private:
		std::string                 		_host;
		int                         		_port;
		std::string							_root; // new
		std::vector<std::string>    		_index; // new
		std::vector<std::string>    		_allowedMethods; // new
		std::vector<std::string>    		_serverNames;
		std::map<int, std::string>  		_errorPages;
		size_t                      		_clientMaxBodySize;
		bool                        		_autoindex; // new
		std::map<std::string, std::string>	_cgiHandlers; // new
		std::string							_uploadStore; // new
		std::pair<int, std::string>         _redirect; // new
		std::vector<LocationConfig>			_locations;

	public:
		ServerConfig();
		ServerConfig(const ServerConfig& other);
		ServerConfig& operator=(const ServerConfig& other);
		~ServerConfig();
		
		const std::string&                  		getHost() const;
		int                           				getPort() const;
		const std::string&							getRoot() const;
		const std::vector<std::string>&				getIndex() const;
		const std::vector<std::string>&				getAllowedMethods() const;
		const std::vector<std::string>&     		getServerNames() const;
		const std::map<int, std::string>&   		getErrorPages() const;
		size_t                        				getClientMaxBodySize() const;
		bool										getAutoIndex() const;
		const std::map<std::string, std::string>&	getCgiHandlers() const;
		const std::string& 							getUploadStore() const;
		const std::pair<int, std::string>&  		getRedirect() const;
		const std::vector<LocationConfig>&  		getLocations() const;
		
		void setHost(const std::string& host);
		void setPort(int port);
		void setRoot(const std::string& root);
		void setClientMaxBodySize(size_t size);
		void setAutoindex(bool autoindex);
		void setUploadStore(const std::string& uploadStore);
		void setRedirect(int statusCode, const std::string& url);
		
		void addIndex(const std::string& index);
		void addAllowedMethod(const std::string& method);
		void addLocation(const LocationConfig& location);
		void addServerName(const std::string& name);
		void addErrorPage(int statusCode, const std::string& errorFilePath);
		void addCgiHandler(const std::string& extension, const std::string& path);
		std::string createPortString() const;

};

#endif
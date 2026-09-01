#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp" // Assumes you have this defined

class ServerConfig {
private:
    std::string                         _host;
    int                                 _port;
    std::vector<std::string>            _serverNames;
    std::map<int, std::string>          _errorPages;
    size_t                              _clientMaxBodySize;
    std::vector<LocationConfig>         _locations;

public:
    // Default constructor
    ServerConfig();
	ServerConfig(const ServerConfig& other);
	ServerConfig& operator=(const ServerConfig& other);
    ~ServerConfig();

    // Setters (used by your config file parser)
    void setHost(const std::string& host);
    void setPort(int port);
    void addServerName(const std::string& name);
    void addErrorPage(int statusCode, const std::string& errorFilePath);
    void setClientMaxBodySize(size_t size);
    void addLocation(const LocationConfig& location);

    // Getters (used by your request handler to make routing decisions)
    const std::string&                  getHost() const;
    int                                 getPort() const;
    const std::vector<std::string>&     getServerNames() const;
    const std::map<int, std::string>&   getErrorPages() const;
    size_t                              getClientMaxBodySize() const;
    const std::vector<LocationConfig>&  getLocations() const;
};

#endif
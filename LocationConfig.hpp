#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <utility> // For std::pair
#include <map>

class LocationConfig {
private:
    std::string                        _path;
    std::vector<std::string>           _allowedMethods;
    std::string                        _root;
    bool                               _autoindex;
    std::vector<std::string>           _index;
	std::map<std::string, std::string> _cgiHandlers;
    std::string                        _uploadStore;
    std::pair<int, std::string>        _redirect;

public:
    // Default constructor
    LocationConfig();
	LocationConfig(const LocationConfig& other);
	LocationConfig& operator=(const LocationConfig& other);
    ~LocationConfig();

    // Setters
    void setPath(const std::string& path);
    void setRoot(const std::string& root);
    void setAutoindex(bool autoindex);
    void setUploadStore(const std::string& uploadStore);
    void setRedirect(int statusCode, const std::string& url);
	
    void addAllowedMethod(const std::string& method);
    void addIndex(const std::string& index);
    void addCgiHandler(const std::string& extension, const std::string& path);

    // Getters
    const std::string&                  getPath() const;
    const std::vector<std::string>&     getAllowedMethods() const;
    const std::string&                  getRoot() const;
    bool                                getAutoindex() const;
    const std::vector<std::string>&     getIndex() const;
    const std::map<std::string, std::string>&   getCgiHandlers() const;
    const std::string&                  getUploadStore() const;
    const std::pair<int, std::string>&  getRedirect() const;
};

#endif
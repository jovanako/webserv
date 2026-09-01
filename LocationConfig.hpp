#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <utility> // For std::pair

class LocationConfig {
private:
    std::string                     _path;
    std::vector<std::string>        _allowedMethods;
    std::string                     _root;
    bool                            _autoindex;
    std::string                     _index;
    std::string                     _cgiExtension;
    std::string                     _cgiPath;
    std::string                     _uploadStore;
    std::pair<int, std::string>     _redirect;

public:
    // Default constructor
    LocationConfig();
	LocationConfig(const LocationConfig& other);
	LocationConfig& operator=(const LocationConfig& other);
    ~LocationConfig();

    // Setters
    void setPath(const std::string& path);
    void addAllowedMethod(const std::string& method);
    void setRoot(const std::string& root);
    void setAutoindex(bool autoindex);
    void setIndex(const std::string& index);
    void setCgi(const std::string& extension, const std::string& path);
    void setUploadStore(const std::string& uploadStore);
    void setRedirect(int statusCode, const std::string& url);

    // Getters
    const std::string&                  getPath() const;
    const std::vector<std::string>&     getAllowedMethods() const;
    const std::string&                  getRoot() const;
    bool                                getAutoindex() const;
    const std::string&                  getIndex() const;
    const std::string&                  getCgiExtension() const;
    const std::string&                  getCgiPath() const;
    const std::string&                  getUploadStore() const;
    const std::pair<int, std::string>&  getRedirect() const;
};

#endif
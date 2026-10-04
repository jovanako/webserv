#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <utility> // For std::pair
#include <map>

class LocationConfig {
	private:
		std::string                        _path;
		std::string                        _root;
		std::vector<std::string>           _index;
		std::vector<std::string>           _allowedMethods;
		bool                               _autoindex;
		std::map<std::string, std::string> _cgiHandlers;
		std::string                        _uploadStore;
		std::pair<int, std::string>        _redirect;

	public:
		LocationConfig();
		LocationConfig(const LocationConfig& other);
		LocationConfig& operator=(const LocationConfig& other);
		~LocationConfig();
		
		const std::string&                  		getPath() const;
		const std::string&                  		getRoot() const;
		const std::vector<std::string>&     		getIndex() const;
		const std::vector<std::string>&     		getAllowedMethods() const;
		bool                                		getAutoindex() const;
		const std::map<std::string, std::string>&   getCgiHandlers() const;
		const std::string& 							getUploadStore() const;
		const std::pair<int, std::string>&  		getRedirect() const;

		void setPath(const std::string& path);
		void setRoot(const std::string& root);
		void setAutoindex(bool autoindex);
		void setUploadStore(const std::string& uploadStore);
		void setRedirect(int statusCode, const std::string& url);
		
		void addIndex(const std::string& index);
		void addAllowedMethod(const std::string& method);
		void addCgiHandler(const std::string& extension, const std::string& path);

};

#endif
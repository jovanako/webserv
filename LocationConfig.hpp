#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <utility>
#include <map>
#include <stdexcept>

class LocationConfig {
	private:
		std::string                         _path;
		std::string                         _root;
		std::vector<std::string>            _index;
		std::vector<std::string>            _allowedMethods;
		bool                                _autoindex;
		bool                                _hasAutoindex;
		std::map<std::string, std::string>	_cgiHandlers;
		std::string                         _uploadStore;
		size_t                      		_clientMaxBodySize;
		bool                                _hasClientMaxBodySize;
		std::pair<int, std::string>         _redirect;
		std::map<int, std::string>  		_errorPages;

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
		bool                                		hasAutoindex() const;
		const std::map<std::string, std::string>&   getCgiHandlers() const;
		const std::string& 							getUploadStore() const;
		size_t                        				getClientMaxBodySize() const;
		bool                                		hasClientMaxBodySize() const;
		const std::pair<int, std::string>&  		getRedirect() const;
		const std::map<int, std::string>&   		getErrorPages() const;

		void setPath(const std::string& path);
		void setRoot(const std::string& root);
		void setIndex(const std::vector<std::string>& indexVector);
		void setAllowedMethods(const std::vector<std::string>& methodsVector);
		void setCgiHandlers(const std::map<std::string, std::string>& cgiHandlers);
		void setErrorPages(const std::map<int, std::string>& errorPages);
		void setAutoindex(bool autoindex);
		void setUploadStore(const std::string& uploadStore);
		void setClientMaxBodySize(size_t size);
		void setRedirect(int statusCode, const std::string& url);
		
		void addIndex(const std::string& index);
		void addAllowedMethod(const std::string& method);
		void addCgiHandler(const std::string& extension, const std::string& path);
		void addErrorPage(int statusCode, const std::string& errorFilePath);

		std::string resolveScriptName(const std::string& uri) const;
		std::string getCgiHandler(const std::string& scriptName) const;

};

#endif
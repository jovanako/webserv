#ifndef REQUESTHANDLER1_HPP
#define REQUESTHANDLER1_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <map>
#include <unistd.h>

class RequestHandler {
    private:
        const LocationConfig* findMatchingLocation(const ServerConfig& server, std::string& uri) const;
		bool		isMethodAllowed(const LocationConfig& location, const std::string& method) const;
		int 		handleRedirect(const LocationConfig& location, HttpResponse& response) const;
		bool		tryCgiHandler(const LocationConfig& location, const std::string& uri) const;
		
        int 		dispatchMethod(const LocationConfig& location, const std::string& method, const std::string& fullPath, const std::string& uri, const HttpRequest& request, HttpResponse& response) const;
        
        int			handleGet(const LocationConfig& matchedLocation, std::string fullPath, const std::string uri, HttpResponse& response) const;
	    int			handlePost(const LocationConfig& matchedLocation, const std::string uri, const HttpRequest& request, HttpResponse& response) const;
		int			handleDelete(const std::string& fullPath, HttpResponse& response) const;

        bool		findIndexFile(const LocationConfig& matchedLocation, std::string& fullPath, struct stat& pathStat) const;
		int		    serveAutoindex(const LocationConfig& matchedLocation, const std::string& fullPath, const std::string& uri, HttpResponse& response) const;
		int		    serveFile(const std::string& fullPath, HttpResponse& response) const;
		int	      redirectDirectoryRequest(const std::string& uri, HttpResponse& response) const;
        int		    serveExistingPath(const LocationConfig& matchedLocation, std::string& fullPath, struct stat& pathStat, const std::string& uri, HttpResponse& response) const;
		
        std::string buildUploadPath(const LocationConfig& matchedLocation, const std::string& uri) const;
		bool	    writeUploadBody(std::ofstream& outFile, const std::string& savePath, const HttpRequest& request) const;
		void	    setUploadResponse(const std::string& uri, HttpResponse& response) const;
		
    public:
        RequestHandler();
        ~RequestHandler();
        RequestHandler(const RequestHandler& other);
        RequestHandler& operator=(const RequestHandler& other);

        static std::string getMimeType(const std::string& path);
        static std::string getFileExtension(const std::string& path);
        static std::string lowercaseExtension(const std::string& extension);
        static std::string mimeTypeForExtension(const std::string& extension);

        int processRequest(const HttpRequest& request, HttpResponse& response, const ServerConfig& server) const;

};

#endif
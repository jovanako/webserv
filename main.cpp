#include "ConfigParser.hpp"
#include <iostream>

#include "ConfigParser.hpp"
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
        return 1;
    }

    try {
        ConfigParser parser(argv[1]);
        std::vector<ServerConfig> servers = parser.parse();

        std::cout << "\n=== WEBSERV CONFIGURATION DUMP ===" << std::endl;

        for (size_t i = 0; i < servers.size(); ++i) {
            const ServerConfig& srv = servers[i];
            
            std::cout << "\n[SERVER " << i + 1 << "]" << std::endl;
            std::cout << "  Host              : " << srv.getHost() << std::endl;
            std::cout << "  Port              : " << srv.getPort() << std::endl;
            std::cout << "  Max Body Size     : " << srv.getClientMaxBodySize() << " bytes" << std::endl;

            // Print Server Names
            std::cout << "  Server Names      : ";
            const std::vector<std::string>& names = srv.getServerNames();
            for (size_t j = 0; j < names.size(); ++j) {
                std::cout << names[j] << " ";
            }
            std::cout << std::endl;

            // Print Error Pages
            std::cout << "  Error Pages       : " << std::endl;
            const std::map<int, std::string>& errors = srv.getErrorPages();
            for (std::map<int, std::string>::const_iterator it = errors.begin(); it != errors.end(); ++it) {
                std::cout << "    - " << it->first << " -> " << it->second << std::endl;
            }

            // Print Locations
            const std::vector<LocationConfig>& locs = srv.getLocations();
            for (size_t j = 0; j < locs.size(); ++j) {
                const LocationConfig& loc = locs[j];
                
                std::cout << "\n  [Location " << j + 1 << "]" << std::endl;
                std::cout << "    Path            : " << loc.getPath() << std::endl;
                std::cout << "    Root            : " << loc.getRoot() << std::endl;
                std::cout << "    Autoindex       : " << (loc.getAutoindex() ? "on" : "off") << std::endl;
                std::cout << "    Upload Store    : " << loc.getUploadStore() << std::endl;
                
                // Print Indexes
                std::cout << "    Index Files     : ";
                const std::vector<std::string>& indexes = loc.getIndex();
                for (size_t k = 0; k < indexes.size(); ++k) {
                    std::cout << indexes[k] << " ";
                }
                std::cout << std::endl;

                // Print Allowed Methods
                std::cout << "    Allowed Methods : ";
                const std::vector<std::string>& methods = loc.getAllowedMethods();
                for (size_t k = 0; k < methods.size(); ++k) {
                    std::cout << methods[k] << " ";
                }
                std::cout << std::endl;

                // Print CGI Handlers
                std::cout << "    CGI Handlers    : " << std::endl;
                const std::map<std::string, std::string>& cgis = loc.getCgiHandlers();
                for (std::map<std::string, std::string>::const_iterator it = cgis.begin(); it != cgis.end(); ++it) {
                    std::cout << "      - " << it->first << " -> " << it->second << std::endl;
                }

                // Print Redirect
                const std::pair<int, std::string>& redir = loc.getRedirect();
                if (redir.first != 0) {
                    std::cout << "    Redirect        : " << redir.first << " -> " << redir.second << std::endl;
                } else {
                    std::cout << "    Redirect        : None" << std::endl;
                }
            }
            std::cout << "--------------------------------------" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
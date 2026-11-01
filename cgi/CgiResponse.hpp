#ifndef CGIRESPONSE_HPP
#define CGIRESPONSE_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cctype>
#include "../HttpResponse.hpp"

class CgiResponse {
	private:
		int									_statusCode;
		std::map<std::string, std::string>	_headers;
		std::vector<char>					_body;

		bool	findHeaderBoundary(const std::vector<char>& rawOutput, size_t& headerEnd, size_t& bodyStart) const;
		bool	parseHeaderBlock(const std::string& headerBlock);
		bool	parseHeaderLine(const std::string& line);
		bool	parseStatusHeader(const std::string& value);

	public:
		CgiResponse();
		CgiResponse(const CgiResponse& other);
		CgiResponse& operator=(const CgiResponse& other);
		~CgiResponse();

		bool	parse(const std::vector<char>& rawOutput);
		void	applyTo(HttpResponse& response) const;

		int											getStatusCode() const;
		const std::map<std::string, std::string>&	getHeaders() const;
		const std::vector<char>&					getBody() const;
};

#endif


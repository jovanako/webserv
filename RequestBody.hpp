#ifndef REQUESTBODY_HPP
#define REQUESTBODY_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>

class RequestBody {
	private:
		static const size_t	BUFFER_SIZE = 4096;
		static size_t		_fileCounter;

		mutable int			_fd;
		std::string			_filePath;
		size_t				_size;

		bool openTempFile();
		void	reset();

	public:
		RequestBody();
		RequestBody(const RequestBody& other);
		RequestBody& operator=(const RequestBody& other);
		~RequestBody();

		bool	append(const char* data, size_t size);
		bool	rewind() const;
		bool	writeTo(std::ofstream& outFile) const;

		int		getFd() const;
		size_t	getSize() const;
		bool	empty() const;
};

#endif


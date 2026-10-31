#include "RequestBody.hpp"

size_t RequestBody::_fileCounter = 0;

RequestBody::RequestBody()
	: _fd(-1),
	  _filePath(""),
	  _size(0) {}

RequestBody::RequestBody(const RequestBody& other)
	: _fd(-1),
	  _filePath(""),
	  _size(0) {
	*this = other;
}

RequestBody& RequestBody::operator=(const RequestBody& other) {
	if (this != &other) {
		reset();
		if (other._fd != -1) {
			_fd = dup(other._fd);
		}
		_filePath = other._filePath;
		_size = other._size;
	}
	return *this;
}

RequestBody::~RequestBody() {
	reset();
}

bool RequestBody::openTempFile() {
	if (_fd != -1) {
		return true;
	}

	std::ostringstream oss;
	oss << "/tmp/webserv_body_" << getpid() << "_" << ++_fileCounter;
	_filePath = oss.str();

	_fd = open(_filePath.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (_fd < 0) {
		_fd = -1;
		_filePath.clear();
		return false;
	}

	// Unlink immediately so the OS removes the file as soon as all FDs close
	std::remove(_filePath.c_str());
	_filePath.clear();
	return true;
}

bool RequestBody::append(const char* data, size_t size) {
	if (!data || size == 0) {
		return true;
	}

	if (_fd == -1 && !openTempFile()) {
		return false;
	}

	size_t totalWritten = 0;
	while (totalWritten < size) {
		ssize_t written = write(_fd, data + totalWritten, size - totalWritten);
		if (written <= 0) {
			return false;
		}
		totalWritten += static_cast<size_t>(written);
	}

	_size += size;
	return true;
}

bool RequestBody::rewind() const {
	if (_fd == -1) {
		return true;
	}
	return lseek(_fd, 0, SEEK_SET) != -1;
}

bool RequestBody::writeTo(std::ofstream& outFile) const {
	if (_fd == -1 || _size == 0) {
		return true;
	}

	if (!rewind()) {
		return false;
	}

	char buffer[BUFFER_SIZE];
	ssize_t bytesRead;
	while ((bytesRead = read(_fd, buffer, sizeof(buffer))) > 0) {
		outFile.write(buffer, bytesRead);
		if (outFile.fail()) {
			return false;
		}
	}

	if (bytesRead < 0) {
		return false;
	}

	return rewind();
}

void RequestBody::reset() {
	if (_fd != -1) {
		close(_fd);
		_fd = -1;
	}
	if (!_filePath.empty()) {
		std::remove(_filePath.c_str());
		_filePath.clear();
	}
	_size = 0;
}

int RequestBody::getFd() const {
	return _fd;
}

size_t RequestBody::getSize() const {
	return _size;
}

bool RequestBody::empty() const {
	return _size == 0;
}


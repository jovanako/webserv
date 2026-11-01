#include "CgiExecutor.hpp"

CgiExecutor::CgiExecutor()
	: _pid(-1),
	  _pipeOut(-1),
	  _state(CGI_IDLE),
	  _errorCode(0),
	  _startTime(0),
	  _outputBuffer() {}

CgiExecutor::CgiExecutor(const CgiExecutor& other)
	: _pid(-1),
	  _pipeOut(-1),
	  _state(CGI_IDLE),
	  _errorCode(0),
	  _startTime(0),
	  _outputBuffer() {
	*this = other;
}

CgiExecutor& CgiExecutor::operator=(const CgiExecutor& other) {
	if (this != &other) {
		cleanup();
		_pid = other._pid;
		_pipeOut = other._pipeOut;
		_state = other._state;
		_errorCode = other._errorCode;
		_startTime = other._startTime;
		_outputBuffer = other._outputBuffer;
	}
	return *this;
}

CgiExecutor::~CgiExecutor() {
	cleanup();
}

int CgiExecutor::getPipeFd() const {
	return _pipeOut;
}

pid_t CgiExecutor::getPid() const {
	return _pid;
}

CgiExecutor::State CgiExecutor::getState() const {
	return _state;
}

int CgiExecutor::getErrorCode() const {
	return _errorCode;
}

const std::vector<char>& CgiExecutor::getOutputBuffer() const {
	return _outputBuffer;
}

void CgiExecutor::closePipe() {
	if (_pipeOut != -1) {
		close(_pipeOut);
		_pipeOut = -1;
	}
}

void CgiExecutor::cleanup() {
	closePipe();
	if (_pid > 0) {
		int status = 0;
		pid_t result = waitpid(_pid, &status, WNOHANG);
		if (result == 0) {
			kill(_pid, SIGKILL);
			waitpid(_pid, &status, 0);
		}
		_pid = -1;
	}
	_state = CGI_IDLE;
	_errorCode = 0;
	_startTime = 0;
	_outputBuffer.clear();
}

bool CgiExecutor::validateScript(const CgiRequest& cgiReq) {
	struct stat scriptStat;
	const std::string& scriptPath = cgiReq.getScriptPath();

	if (stat(scriptPath.c_str(), &scriptStat) != 0) {
		_errorCode = 404;
		_state = CGI_ERROR;
		return false;
	}

	if (!S_ISREG(scriptStat.st_mode) || access(scriptPath.c_str(), R_OK) != 0) {
		_errorCode = 403;
		_state = CGI_ERROR;
		return false;
	}

	const std::string& executable = cgiReq.getExecutable();
	if (access(executable.c_str(), X_OK) != 0) {
		_errorCode = 500;
		_state = CGI_ERROR;
		return false;
	}

	return true;
}

void CgiExecutor::executeChild(const CgiRequest& cgiReq, int pipeReadFd, int pipeWriteFd) {
	close(pipeReadFd);

	if (dup2(pipeWriteFd, STDOUT_FILENO) == -1) {
		close(pipeWriteFd);
		_exit(1);
	}
	close(pipeWriteFd);

	int bodyFd = cgiReq.getBody().getFd();
	if (bodyFd != -1) {
		if (dup2(bodyFd, STDIN_FILENO) == -1) {
			close(bodyFd);
			_exit(1);
		}
		close(bodyFd);
	} else {
		int devNull = open("/dev/null", O_RDONLY);
		if (devNull != -1) {
			dup2(devNull, STDIN_FILENO);
			close(devNull);
		}
	}

	const std::string& workDir = cgiReq.getWorkingDirectory();
	if (!workDir.empty() && chdir(workDir.c_str()) != 0) {
		_exit(1);
	}

	std::vector<char*> argv = cgiReq.buildArgv();
	std::vector<char*> envp = cgiReq.buildEnvp();

	execve(cgiReq.getExecutable().c_str(), &argv[0], &envp[0]);
	_exit(1);
}

bool CgiExecutor::start(const CgiRequest& cgiReq) {
	cleanup();

	if (!validateScript(cgiReq)) {
		return false;
	}

	if (!cgiReq.getBody().rewind()) {
		_errorCode = 500;
		_state = CGI_ERROR;
		return false;
	}

	int pipeFd[2];
	if (pipe(pipeFd) == -1) {
		_errorCode = 500;
		_state = CGI_ERROR;
		return false;
	}

	int flags = fcntl(pipeFd[0], F_GETFL, 0);
	if (flags == -1 || fcntl(pipeFd[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		close(pipeFd[0]);
		close(pipeFd[1]);
		_errorCode = 500;
		_state = CGI_ERROR;
		return false;
	}

	_pid = fork();
	if (_pid < 0) {
		close(pipeFd[0]);
		close(pipeFd[1]);
		_pid = -1;
		_errorCode = 500;
		_state = CGI_ERROR;
		return false;
	}

	if (_pid == 0) {
		executeChild(cgiReq, pipeFd[0], pipeFd[1]);
	}

	close(pipeFd[1]);
	_pipeOut = pipeFd[0];
	_startTime = std::time(NULL);
	_state = CGI_RUNNING;
	return true;
}

bool CgiExecutor::reapChild() {
	if (_pid <= 0) {
		return true;
	}

	int status = 0;
	pid_t result = waitpid(_pid, &status, WNOHANG);
	if (result == 0) {
		result = waitpid(_pid, &status, 0);
	}

	_pid = -1;
	if (result < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		_errorCode = 502;
		_state = CGI_ERROR;
		return false;
	}

	_state = CGI_COMPLETE;
	return true;
}

bool CgiExecutor::readOutput() {
	if (_state != CGI_RUNNING || _pipeOut == -1) {
		return false;
	}

	char buffer[BUFFER_SIZE];
	ssize_t bytesRead = read(_pipeOut, buffer, sizeof(buffer));

	if (bytesRead > 0) {
		_outputBuffer.insert(_outputBuffer.end(), buffer, buffer + bytesRead);
		return true;
	}

	if (bytesRead == 0) {
		closePipe();
		return reapChild();
	}

	closePipe();
	if (_pid > 0) {
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
		_pid = -1;
	}
	_errorCode = 500;
	_state = CGI_ERROR;
	return false;
}

bool CgiExecutor::hasTimedOut(time_t timeoutSec) const {
	if (_state != CGI_RUNNING) {
		return false;
	}
	return (std::time(NULL) - _startTime) >= timeoutSec;
}

void CgiExecutor::handleTimeout() {
	closePipe();
	if (_pid > 0) {
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
		_pid = -1;
	}
	_errorCode = 504;
	_state = CGI_ERROR;
}


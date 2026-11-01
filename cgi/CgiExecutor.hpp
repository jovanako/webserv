#ifndef CGIEXECUTOR_HPP
#define CGIEXECUTOR_HPP

#include <vector>
#include <string>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdlib>
#include <iostream>
#include "CgiRequest.hpp"

class CgiExecutor {
	public:
		enum State {
			CGI_IDLE,
			CGI_RUNNING,
			CGI_COMPLETE,
			CGI_ERROR
		};

		static const time_t	DEFAULT_TIMEOUT_SEC = 10;

	private:
		static const size_t	BUFFER_SIZE = 4096;

		pid_t				_pid;
		int					_pipeOut;
		State				_state;
		int					_errorCode;
		time_t				_startTime;
		std::vector<char>	_outputBuffer;

		bool	validateScript(const CgiRequest& cgiReq);
		void	executeChild(const CgiRequest& cgiReq, int pipeReadFd, int pipeWriteFd);
		void	closePipe();
		bool	reapChild();

	public:
		CgiExecutor();
		CgiExecutor(const CgiExecutor& other);
		CgiExecutor& operator=(const CgiExecutor& other);
		~CgiExecutor();

		bool	start(const CgiRequest& cgiReq);
		bool	readOutput();
		bool	hasTimedOut(time_t timeoutSec = DEFAULT_TIMEOUT_SEC) const;
		void	handleTimeout();
		void	cleanup();

		int							getPipeFd() const;
		pid_t						getPid() const;
		State						getState() const;
		int							getErrorCode() const;
		const std::vector<char>&	getOutputBuffer() const;
};

#endif


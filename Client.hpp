#include "HttpRequest.hpp"
#include "HttpResponse.hpp" 

class Client {
public:
    enum ConnectionState {
        READING_HEADER,
        READING_BODY,
        PROCESSING,
        WRITING_RESPONSE,
        CGI_PIPE_WAIT,
        DONE
    };

private:
    int                 _socketFd;
    ConnectionState     _state;       // <--- The variable tracking where this client is
    HttpRequest         _request;
    HttpResponse        _response;
    // ... buffers, timers, etc.

public:
	Client();
    Client(int fd);
	Client(const Client& other);
	Client& operator=(const Client& other);
    ~Client();

    ConnectionState getClientState() const;
    void setClientState(ConnectionState state);

	void handleReadHeader();
	void handleReadBody();
	void handleProcessing();
	void handleWriteResponse();
	void handleCgiPipeWait();
	void handleDone();
    
    void handleEvent(); // The core logic per client
};

/*
A robust state machine for a 42 webserv project typically requires 
tracking six distinct phases for every client connection to handle 
non-blocking I/O, chunked data, and CGI execution safely.

READING_HEADERS: The server listens for POLLIN and reads raw bytes from 
the socket into a buffer until it detects the double CRLF (\r\n\r\n) 
delimiter marking the end of the HTTP headers.

READING_BODY: Once headers are parsed, the server continues reading data 
if a Content-Length header is present or if the request uses chunked 
transfer encoding (which must be un-chunked as required by the subject).

PROCESSING: The full request is received. The server evaluates the 
configuration routes, checks allowed methods (GET, POST, DELETE), verifies 
file access permissions, and prepares either a static file response or a 
CGI execution plan.

WAITING_CGI: If the request triggers a script (like PHP or Python), the 
server forks the CGI process, monitors its input/output pipes, and waits 
for execution to complete while ensuring the non-blocking event loop 
doesn't freeze.

WRITING_RESPONSE: The response headers and body are fully generated or 
loaded into a send buffer. The socket's event registration switches from 
POLLIN to POLLOUT to safely flush bytes back to the client.

CLOSING: The response is completely sent (or an error occurred), signaling 
that the server should close the file descriptor, free allocated memory, 
and remove the socket from the poll() array.
*/
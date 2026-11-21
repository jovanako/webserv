Ran list project files
Analyzed ServerManager.hpp:L1-71
Analyzed ServerManager.cpp:L1-525
Analyzed Client.hpp:L1-97
Analyzed Client.cpp:L1-399
Analyzed main.cpp:L1-39
Analyzed ConfigParser.hpp:L1-66
Analyzed ServerConfig.hpp:L1-64
Analyzed LocationConfig.hpp:L1-65
Analyzed RequestParser.hpp:L1-49
Analyzed RequestHandler.hpp:L1-54
Analyzed ResponseBuilder.hpp:L1-30
Analyzed HttpRequest.hpp:L1-79
Analyzed RequestBody.hpp:L1-40
Analyzed HttpResponse.hpp:L1-39
Analyzed CgiRequest.hpp:L1-52
Analyzed CgiExecutor.hpp:L1-63
Analyzed CgiResponse.hpp:L1-37

### 1. Architecture Overview

#### Classes & Responsibilities

* **Configuration Layer**
  * `ConfigParser` ([ConfigParser.hpp](file:///home/jovana/Desktop/42Berlin/webserv/ConfigParser.hpp)): Tokenizes and parses the `.conf` file into a `std::vector<ServerConfig>`.
  * `ServerConfig` ([ServerConfig.hpp](file:///home/jovana/Desktop/42Berlin/webserv/ServerConfig.hpp)) & `LocationConfig` ([LocationConfig.hpp](file:///home/jovana/Desktop/42Berlin/webserv/LocationConfig.hpp)): Store server-level settings (`host`, `port`, `server_name`, `error_page`, `client_max_body_size`, etc.) and route-level overrides (`root`, `index`, `allowed_methods`, `autoindex`, `upload_store`, `cgi`, `return`).
* **Event Loop & Socket Management**
  * `ServerManager` ([ServerManager.hpp](file:///home/jovana/Desktop/42Berlin/webserv/ServerManager.hpp), [ServerManager.cpp](file:///home/jovana/Desktop/42Berlin/webserv/ServerManager.cpp)): Initializes non-blocking listening sockets (deduplicating shared `host:port` virtual hosts), owns `_pollFds`, `_listenSockets`, `_clients`, and `_cgiPipes`, and runs the single-threaded `poll()` event loop in `run()`.
* **Per-Connection State Machine & HTTP Layer**
  * `Client` ([Client.hpp](file:///home/jovana/Desktop/42Berlin/webserv/Client/Client.hpp), [Client.cpp](file:///home/jovana/Desktop/42Berlin/webserv/Client/Client.cpp)): Represents one client connection and drives a 6-state machine:
    `READING_HEADER` → `READING_BODY` → `PROCESSING` → (`CGI_PIPE_WAIT` →) `WRITING_RESPONSE` → `DONE`.
  * `RequestParser` ([RequestParser.hpp](file:///home/jovana/Desktop/42Berlin/webserv/Client/RequestParser.hpp)): Buffers raw bytes from `recv()` and incrementally parses the request line, headers, fixed-length bodies, and `Transfer-Encoding: chunked` bodies into an `HttpRequest`.
  * `HttpRequest` ([HttpRequest.hpp](file:///home/jovana/Desktop/42Berlin/webserv/HttpRequest.hpp)) & `RequestBody` ([RequestBody.hpp](file:///home/jovana/Desktop/42Berlin/webserv/RequestBody.hpp)): Hold the parsed request. `RequestBody` streams body data to a temporary file on disk to keep memory usage bounded for large uploads and CGI inputs.
  * `RequestHandler` ([RequestHandler.hpp](file:///home/jovana/Desktop/42Berlin/webserv/Client/RequestHandler.hpp)): Finds the matching `LocationConfig`, checks allowed methods, handles redirects, serves static files/autoindex (`GET`), handles file uploads (`POST`), deletes files (`DELETE`), or signals CGI execution by returning `0`.
  * `ResponseBuilder` ([ResponseBuilder.hpp](file:///home/jovana/Desktop/42Berlin/webserv/Client/ResponseBuilder.hpp)) & `HttpResponse` ([HttpResponse.hpp](file:///home/jovana/Desktop/42Berlin/webserv/HttpResponse.hpp)): Attach final headers (`Content-Length`, `Connection: keep-alive/close`), load custom or default error pages, and serialize the response into a raw `std::vector<char>`.
* **CGI Subsystem**
  * `CgiRequest` ([CgiRequest.hpp](file:///home/jovana/Desktop/42Berlin/webserv/cgi/CgiRequest.hpp)): Builds `argv`, `envp` (`REQUEST_METHOD`, `SCRIPT_NAME`, `PATH_INFO`, `HTTP_*`, etc.), working directory, and references `RequestBody`.
  * `CgiExecutor` ([CgiExecutor.hpp](file:///home/jovana/Desktop/42Berlin/webserv/cgi/CgiExecutor.hpp)): `fork()`s the CGI process, wires `stdin` to `RequestBody`'s temp file fd and `stdout` to a non-blocking pipe, reads output incrementally, and enforces a 10s timeout.
  * `CgiResponse` ([CgiResponse.hpp](file:///home/jovana/Desktop/42Berlin/webserv/cgi/CgiResponse.hpp)): Parses the CGI script's headers/status and body and applies them to `HttpResponse`.

---

#### Request Lifecycle: `accept()` → Final HTTP Response

1. **Accept (`ServerManager::acceptClient`)**
   * When `poll()` signals `POLLIN` on a listening socket, `acceptConnection()` calls `accept()` to obtain `clientFd` and extracts the client IP.
   * `clientFd` is set to `O_NONBLOCK` (`fcntl`), wrapped in a `Client` (starting in `READING_HEADER` with its default `ServerConfig` and the virtual host list), and queued in `pendingFds` with `POLLIN` (merged into `_pollFds` at the end of the poll iteration to avoid invalidating the active loop).
2. **Read & Parse Headers (`READING_HEADER`)**
   * On `POLLIN`, `ServerManager::handleClientEvent()` calls `client.handleRead()` → `handleReadHeader()` → `readChunk()` (`recv()` into `RequestParser`'s buffer).
   * Once `\r\n\r\n` arrives, `_parser.parseHeaders()` validates the request line and headers, `selectVirtualHost()` selects the matching `ServerConfig` using the `Host` header, and `Content-Length` is checked against the effective `client_max_body_size` (returning `413` if exceeded).
3. **Read Body (`READING_BODY`, if present)**
   * `prepareRequestBody()` strips the headers from the buffer. If the request is chunked or has `Content-Length > 0`, the client enters `READING_BODY` and `handleReadBody()` feeds data into `parseChunkedBody()` or `processFixedLengthBody()` (writing into `RequestBody`'s temp file) until complete. Otherwise, it transitions directly to `PROCESSING`.
4. **Process Request (`PROCESSING`)**
   * `ServerManager::processClient()` calls `client.handleProcessing()`, which invokes `RequestHandler::processRequest()`.
   * **Standard path**: `RequestHandler` populates `_response` and returns a status code. Errors (`>= 400`) go through `setupErrorResponse()` (`ResponseBuilder::buildErrorResponse()`); success goes through `finalizeResponse()` (`ResponseBuilder::buildResponse()`). Both transition to `WRITING_RESPONSE`.
   * **CGI path**: `RequestHandler` returns `0`. `startCgiExecution()` builds a `CgiRequest`, calls `CgiExecutor::start()`, and sets the state to `CGI_PIPE_WAIT`. `ServerManager` registers the CGI pipe fd in `_pollFds` (`POLLIN`) and `_cgiPipes`. As the pipe becomes readable or closes (`POLLIN | POLLHUP`), `handleCgiPipeEvent()` calls `client.handleCgiPipeWait()` → `_cgiExecutor.readOutput()`. On completion, `CgiResponse` parses the output into `_response`, `finalizeResponse()` transitions to `WRITING_RESPONSE`, and `ServerManager` unregisters the pipe and switches the client's `pollfd` to `POLLOUT`. (Timeouts after 10s are caught by `checkCgiTimeouts()`, producing a `504`.)
5. **Write Response (`WRITING_RESPONSE`)**
   * `ServerManager::updateClientPollEvents()` switches the client's `pollfd.events` to `POLLOUT`.
   * On `POLLOUT`, `writeClientResponse()` calls `client.handleWrite()` → `handleWriteResponse()`. On the first write, `_response.createResponse()` serializes the full response into `_writeBuffer`. Non-blocking `send()` writes from `_writeBuffer[_bytesSent]`, advancing `_bytesSent` across as many `poll()` cycles as needed.
6. **Finish or Reuse (`DONE` / `keep-alive`)**
   * Once `_bytesSent >= _writeBuffer.size()`:
     * If `Connection: keep-alive`, `resetForNextRequest()` resets the parser, response, and buffers and returns to `READING_HEADER` (switching `pollfd.events` back to `POLLIN`).
     * Otherwise, the state becomes `DONE`, and `ServerManager::removeCompletedClient()` cleans up the client, closes `clientFd`, and removes it from `_clients` and `_pollFds`.
   * Idle connections (not waiting on CGI) are cleaned up after 30s by `checkClientTimeouts()`.

---

### 2. Choice of I/O Multiplexing: `poll()`

- **POSIX portability** (unlike epoll / kqueue): epoll is Linux-only and kqueue is macOS-only. poll() is standard POSIX, so the same code compiles and runs identically on Linux and macOS.
- **Dynamic array** (unlike select()): poll() uses a dynamic array of struct pollfds, where events are preserved, and kernel writes revents, and these are available through the struct pollfd. On the other hand, select() splits events across separate bitmasks that you have to copy/rebuild every iteration, requires tracking max_fd + 1, and has no POLLHUP flag.
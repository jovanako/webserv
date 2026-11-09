# CGI Implementation & Execution Flow

This document describes the architecture of the Common Gateway Interface (CGI/1.1) implementation in **webserv** and traces the step-by-step lifecycle of a CGI script execution from incoming HTTP request to final network response.

---

## 1. Architecture Overview

CGI execution in **webserv** is designed around the server's single-`poll()`, non-blocking event-driven architecture. Instead of blocking the server while an external script runs, CGI execution is split into three dedicated classes inside the `cgi/` directory and coordinated asynchronously by `Client` and `ServerManager`:

| Component | Files | Responsibility |
| :--- | :--- | :--- |
| **`LocationConfig`** | `LocationConfig.hpp`, `LocationConfig.cpp` | Stores `cgi_pass` extension-to-interpreter mappings (e.g., `.py` -> `/usr/bin/python3`) and resolves the script boundary (`SCRIPT_NAME`) within a request URI. |
| **`CgiRequest`** | `cgi/CgiRequest.hpp`, `cgi/CgiRequest.cpp` | Resolves filesystem paths, working directory, RFC 3875 environment variables, `execve()` argument/environment arrays (`argv`, `envp`), and holds the file-descriptor-backed `RequestBody`. |
| **`CgiExecutor`** | `cgi/CgiExecutor.hpp`, `cgi/CgiExecutor.cpp` | Validates script/interpreter permissions, creates non-blocking pipes, forks the child process, redirects `STDIN`/`STDOUT`, executes `execve()`, reads pipe output incrementally, reaps the child process, and enforces timeouts. |
| **`CgiResponse`** | `cgi/CgiResponse.hpp`, `cgi/CgiResponse.cpp` | Parses the raw bytes produced by the CGI script into CGI headers (`Status`, `Content-Type`, `Location`, etc.) and payload body, then applies them to `HttpResponse`. |
| **`Client` & `ServerManager`** | `Client/Client.cpp`, `ServerManager.cpp` | Registers the CGI output pipe file descriptor in the central `_pollFds` array (`CGI_PIPE_WAIT` state), dispatches `POLLIN`/`POLLHUP` events to `CgiExecutor`, and monitors CGI timeouts on every `poll()` cycle. |

---

## 2. Core Components

### 2.1 Configuration & Route Detection (`LocationConfig` & `RequestHandler`)

In `webserv.conf`, CGI is enabled inside a `location` block using the `cgi_pass` directive:

```nginx
location /cgi-bin {
    root ./www/cgi-bin;
    allow_methods GET POST;
    cgi_pass .py /usr/bin/python3;
    cgi_pass .php /usr/bin/php-cgi;
}
```

- **`LocationConfig::resolveScriptName(uri)`**: Walks path segments of the request URI from left to right and checks whether any segment ends with a configured CGI extension (such as `.py` or `.php`).
  - Example: For `/cgi-bin/hello.py/extra/path`, it returns `/cgi-bin/hello.py` as the `scriptName`, leaving `/extra/path` as `PATH_INFO`.
- **`LocationConfig::getCgiHandler(scriptName)`**: Extracts the extension from `scriptName` and returns the configured interpreter binary path (e.g., `/usr/bin/python3`).
- **`RequestHandler::processRequest()`**: After verifying that the location exists, the HTTP method is allowed (`405` otherwise), and no location redirect applies, it calls `tryCgiHandler()`. When a CGI handler matches, `processRequest()` returns status code `0` as a signal to `Client::handleProcessing()` to launch CGI execution instead of serving a static file.

---

### 2.2 Context & Environment Preparation (`CgiRequest`)

When `Client::startCgiExecution()` is called, it constructs a `CgiRequest(server, matchedLocation, request)`, which prepares everything the child process needs before `fork()`:

1. **Path & Working Directory Resolution (`initPaths`, `initWorkingDirectory`)**:
   - Strips the location prefix (`location.getPath()`) from `scriptName` and joins the remainder with `location.getRoot()` to produce `_scriptPath` (e.g., `./www/cgi-bin/hello.py`).
   - Splits `_scriptPath` at the last `/` into:
     - `_workingDirectory` (e.g., `./www/cgi-bin`) — used for `chdir()` in the child process so relative file access inside the script works as expected.
     - `_scriptFileName` (e.g., `hello.py`) — passed as `argv[1]` to the interpreter.
   - Extracts `pathInfo` (everything in the URI after `scriptName`).

2. **Environment Variables (`initContentEnvVars`, `initStandardEnvVars`, `initHttpHeaderEnvVars`)**:
   - **Content Variables**:
     - `CONTENT_LENGTH`: Set from the `Content-Length` header or `RequestBody::getSize()` (if > 0).
     - `CONTENT_TYPE`: Set from the `Content-Type` request header if present.
   - **Standard CGI/1.1 Meta-Variables**:
     - `GATEWAY_INTERFACE=CGI/1.1`
     - `REQUEST_METHOD`, `QUERY_STRING`, `REQUEST_URI`, `SERVER_PROTOCOL`
     - `SCRIPT_NAME` (URI path to script) and `SCRIPT_FILENAME` (local script filename)
     - `PATH_INFO` and `PATH_TRANSLATED` (mapped via `location.getRoot()` when `PATH_INFO` is non-empty)
     - `DOCUMENT_ROOT` (location's root directory)
     - `SERVER_NAME` (matched against the `Host` header or virtual host config), `SERVER_PORT`, `SERVER_SOFTWARE=webserv/1.0`
     - `REMOTE_ADDR` and `REMOTE_HOST` (client's IPv4 address)
     - `REDIRECT_STATUS=200` (required for `php-cgi` security checks)
   - **HTTP Request Headers (`HTTP_*`)**:
     - Every other HTTP request header is converted to uppercase, dashes (`-`) are replaced with underscores (`_`), and the key is prefixed with `HTTP_` (e.g., `User-Agent` -> `HTTP_USER_AGENT`, `Cookie` -> `HTTP_COOKIE`).
     - `Content-Length`, `Content-Type`, `Transfer-Encoding`, and `Proxy` are explicitly skipped in the `HTTP_*` loop to avoid duplicate or unsafe variables.

3. **`execve()` Arguments (`buildArgv`, `buildEnvp`)**:
   - `buildArgv()` constructs `[_executable, _scriptFileName, NULL]` (e.g., `["/usr/bin/python3", "hello.py", NULL]`).
   - `buildEnvp()` constructs a `NULL`-terminated `std::vector<char*>` pointing to the `"KEY=VALUE"` strings in `_envVars`.

---

### 2.3 Process Management & Non-Blocking I/O (`CgiExecutor`)

`CgiExecutor` manages the OS-level execution and state transitions (`CGI_IDLE`, `CGI_RUNNING`, `CGI_COMPLETE`, `CGI_ERROR`):

1. **Script & Interpreter Validation (`validateScript`)**:
   - Checks that `_scriptPath` exists via `stat()` -> fails with **`404 Not Found`** if missing.
   - Checks that `_scriptPath` is a regular file (`S_ISREG`) and readable (`access(..., R_OK)`) -> fails with **`403 Forbidden`** otherwise.
   - Checks that `_executable` is executable (`access(..., X_OK)`) -> fails with **`500 Internal Server Error`** otherwise.

2. **Input Body Rewind & Pipe Setup (`start`)**:
   - Rewinds the request body's temporary file descriptor (`cgiReq.getBody().rewind()` via `lseek(fd, 0, SEEK_SET)`). Because `RequestBody` stores incoming `POST` payloads (including de-chunked bodies) in an unlinked temporary file in `/tmp`, no separate stdin pipe is needed—the child process reads directly from this file descriptor.
   - Creates an OS pipe `pipeFd[2]` for capturing the script's standard output and sets the read end (`pipeFd[0]`) to non-blocking mode (`O_NONBLOCK`) via `fcntl()`.

3. **Forking & Child Execution (`executeChild`)**:
   - Calls `fork()`.
   - **In the child process (`_pid == 0`)**:
     - Closes the unused pipe read end (`pipeFd[0]`).
     - Redirects `STDOUT_FILENO` to `pipeFd[1]` via `dup2()`, then closes `pipeFd[1]`.
     - Redirects `STDIN_FILENO`:
       - If the request has a body (`bodyFd != -1`), duplicates `bodyFd` onto `STDIN_FILENO` via `dup2()`.
       - Otherwise, opens `/dev/null` read-only and duplicates it onto `STDIN_FILENO` so the script immediately sees EOF on standard input.
     - Changes the working directory to `_workingDirectory` via `chdir()`.
     - Invokes `execve(_executable, argv, envp)`. If `execve()` fails, terminates the child immediately with `_exit(1)`.
   - **In the parent process (`_pid > 0`)**:
     - Closes the pipe write end (`pipeFd[1]`).
     - Saves `_pipeOut = pipeFd[0]`, records `_startTime = std::time(NULL)`, and transitions to `CGI_RUNNING`.

4. **Incremental Output Reading & Child Reaping (`readOutput`, `reapChild`)**:
   - Each time `poll()` reports `POLLIN` or `POLLHUP` on `_pipeOut`, `CgiExecutor::readOutput()` performs a single `read()` of up to `4096` bytes (`BUFFER_SIZE`):
     - `bytesRead > 0`: Appends bytes to `_outputBuffer` and remains in `CGI_RUNNING`.
     - `bytesRead == 0` (EOF): Closes `_pipeOut` and calls `reapChild()`, which calls `waitpid()` to collect the child's exit status. If the child exited abnormally or with a non-zero exit code, `_state` becomes `CGI_ERROR` with status **`502 Bad Gateway`**; otherwise `_state` becomes `CGI_COMPLETE`.
     - `bytesRead < 0`: Closes `_pipeOut`, kills and reaps the child process, and sets **`500 Internal Server Error`**.

5. **Timeout Enforcement (`cgiHasTimedOut`, `handleTimeout`)**:
   - `DEFAULT_TIMEOUT_SEC` is set to **10 seconds**.
   - If a running CGI process exceeds 10 seconds, `handleTimeout()` closes `_pipeOut`, sends `SIGKILL` to `_pid`, reaps it with `waitpid()`, and sets `_errorCode = 504` (**`504 Gateway Timeout`**).

---

### 2.4 Output Parsing (`CgiResponse`)

Once `CgiExecutor` reaches `CGI_COMPLETE`, `Client::handleCgiPipeWait()` passes `_cgiExecutor.getOutputBuffer()` to `CgiResponse::parse()`:

1. **Header/Body Boundary Detection (`findHeaderBoundary`)**:
   - Scans the raw output buffer for either `\r\n\r\n` (CRLF) or `\n\n` (LF), since CGI scripts on Unix often output `\n` line endings.
   - If no header separator is found (or the output is empty), `parse()` returns `false` and the server responds with **`502 Bad Gateway`**.
2. **Header Block Parsing (`parseHeaderBlock`, `parseHeaderLine`)**:
   - Splits the header section line by line and validates `Key: Value` pairs.
   - If a `Status` CGI header is present (e.g., `Status: 201 Created` or `Status: 404 Not Found`), `parseStatusHeader()` extracts the 3-digit HTTP status code (`100`–`599`) and updates `_statusCode` (which defaults to `200`).
   - All other headers are stored in `_headers`.
3. **Applying to `HttpResponse` (`applyTo`)**:
   - Copies all parsed CGI headers into the `HttpResponse`.
   - If `_statusCode` is still `200` and a `Location` header was provided by the script, automatically updates the status code to **`302 Found`**.
   - If the script produced a body but omitted a `Content-Type` header, defaults `Content-Type` to `text/html`.
   - Copies the parsed body bytes into `HttpResponse`, where `ResponseBuilder::buildResponse()` subsequently computes `Content-Length` and `Connection` headers before socket transmission.

---

## 3. End-to-End Execution Flow of a CGI Script

```
Client Socket           ServerManager (poll)          Client / RequestHandler          CgiExecutor / Child Process
     |                           |                              |                                   |
     |--- HTTP Request --------->|                              |                                   |
     |                           |--- POLLIN (clientFd) ------->|                                   |
     |                           |                              |-- RequestParser parses headers    |
     |                           |                              |   & writes body to RequestBody    |
     |                           |                              |   (/tmp unlinked file)            |
     |                           |                              |                                   |
     |                           |                              |-- processRequest() matches        |
     |                           |                              |   cgi_pass -> returns 0           |
     |                           |                              |                                   |
     |                           |                              |-- startCgiExecution()             |
     |                           |                              |   builds CgiRequest               |
     |                           |                              |--- start(cgiReq) ---------------->|
     |                           |                              |                                   |-- validateScript()
     |                           |                              |                                   |-- rewind RequestBody fd
     |                           |                              |                                   |-- pipe() + O_NONBLOCK
     |                           |                              |                                   |-- fork()
     |                           |                              |                                   |     |-- dup2(bodyFd, STDIN)
     |                           |                              |                                   |     |-- dup2(pipeWrite, STDOUT)
     |                           |                              |                                   |     |-- chdir(scriptDir)
     |                           |                              |                                   |     |-- execve(interpreter)
     |                           |<-- registerCgiPipe(pipeOut) -|-- state = CGI_PIPE_WAIT           |
     |                           |                              |                                   |
     |                           |=== poll() loop continues === |                                   |-- script writes to stdout
     |                           |                              |                                   |
     |                           |--- POLLIN/POLLHUP (pipeOut)->|-- handleCgiPipeWait()             |
     |                           |                              |--- readOutput() ----------------->|-- read(pipeOut, 4096)
     |                           |                              |                                   |-- on EOF: waitpid(_pid)
     |                           |                              |<-- state = CGI_COMPLETE ----------|
     |                           |                              |                                   |
     |                           |                              |-- CgiResponse::parse(output)      |
     |                           |                              |-- cgiResp.applyTo(_response)      |
     |                           |                              |-- finalizeResponse()              |
     |                           |<-- unregisterCgiPipe --------|-- state = WRITING_RESPONSE        |
     |                           |<-- sync events to POLLOUT ---|                                   |
     |                           |                              |                                   |
     |                           |--- POLLOUT (clientFd) ------>|-- handleWriteResponse()           |
     |<-- HTTP Response ---------|                              |   sends serialized HttpResponse   |
```

### Step-by-Step Walkthrough

1. **Request Ingestion (`READING_HEADER` -> `READING_BODY` -> `PROCESSING`)**
   - `ServerManager` detects `POLLIN` on the client socket and calls `Client::handleRead()`.
   - `RequestParser` parses the request line and headers. If the request carries a body (`Content-Length` or `Transfer-Encoding: chunked`), `RequestParser` streams the decoded body bytes into `HttpRequest::_body` (`RequestBody`), which lazily opens an unlinked temporary file in `/tmp`.
   - Once the full request has arrived, the client state transitions to `PROCESSING`.

2. **Routing & CGI Detection (`PROCESSING`)**
   - `ServerManager::processClient()` calls `Client::handleProcessing()`, which invokes `RequestHandler::processRequest()`.
   - `RequestHandler` finds the longest matching `LocationConfig` (e.g., `/cgi-bin`), verifies that the HTTP method (`GET` or `POST`) is permitted, and checks `tryCgiHandler()`.
   - Seeing that the URI contains a script matching `cgi_pass` (e.g., `/cgi-bin/hello.py`), `processRequest()` returns `0`.

3. **Launching the CGI Process (`PROCESSING` -> `CGI_PIPE_WAIT`)**
   - `Client::handleProcessing()` sees the `0` return code and calls `Client::startCgiExecution()`.
   - A `CgiRequest` object is instantiated to compute `_executable`, `_scriptPath`, `_workingDirectory`, and all CGI environment variables.
   - `_cgiExecutor.start(cgiReq)` validates the script and interpreter, rewinds the `RequestBody` file descriptor to offset `0`, creates the non-blocking stdout pipe, and `fork()`s the child process.
   - The child redirects `STDIN_FILENO` (from `RequestBody::getFd()` or `/dev/null`) and `STDOUT_FILENO` (to the pipe write end), changes directory to `_workingDirectory`, and calls `execve()`.
   - The parent closes the pipe write end, sets the client's state to `Client::CGI_PIPE_WAIT`, and `ServerManager::processClient()` registers `_cgiExecutor.getPipeFd()` into `pendingFds` and the `_cgiPipes` map (`pipeFd -> clientFd`).

4. **Asynchronous Polling & Output Accumulation (`CGI_PIPE_WAIT`)**
   - While the child process executes, `ServerManager` continues handling other clients in the main `poll()` loop. Note that `Client::clientHasTimedOut()` ignores clients in `CGI_PIPE_WAIT` because CGI execution is governed by its own dedicated timer (`ServerManager::checkCgiTimeouts()`).
   - Whenever the CGI pipe signals `POLLIN` or `POLLHUP`, `ServerManager::handleCgiPipeEvent()` delegates to `Client::handleCgiPipeWait()`, which calls `_cgiExecutor.readOutput()`.
   - `readOutput()` reads up to 4096 bytes per `poll()` readiness notification into `_outputBuffer`.
   - When the child closes its stdout and exits, `read()` returns `0` (EOF). `CgiExecutor` closes `_pipeOut`, reaps the child via `waitpid()`, and transitions to `CGI_COMPLETE` (or `CGI_ERROR` if exit status != 0).

5. **Response Parsing & Transmission (`CGI_PIPE_WAIT` -> `WRITING_RESPONSE` -> `DONE` / `READING_HEADER`)**
   - Upon `CGI_COMPLETE`, `Client::handleCgiPipeWait()` parses `_outputBuffer` using `CgiResponse::parse()`, applies the status code, headers, and body to `_response` via `cgiResp.applyTo(_response)`, cleans up `_cgiExecutor`, and calls `finalizeResponse()`, switching the client state to `WRITING_RESPONSE`.
   - `ServerManager::handleCgiPipeEvent()` notices the client has left `CGI_PIPE_WAIT`, unregisters the CGI pipe from `_pollFds` and `_cgiPipes`, and updates the client socket's poll events to `POLLOUT`.
   - On the next `POLLOUT` event on the client socket, `Client::handleWriteResponse()` serializes `_response` and sends it to the client via `send()`.

---

## 4. Error Handling Summary

Every failure mode during CGI preparation, execution, or output parsing maps deterministically to an HTTP error response generated through `Client::setupErrorResponse()`:

| Failure Condition | Detected By | HTTP Status Code |
| :--- | :--- | :--- |
| Script file does not exist on disk (`stat()` fails) | `CgiExecutor::validateScript()` | **404 Not Found** |
| Script is not a regular file or lacks read permission (`R_OK`) | `CgiExecutor::validateScript()` | **403 Forbidden** |
| CGI interpreter binary is missing or lacks execute permission (`X_OK`) | `CgiExecutor::validateScript()` | **500 Internal Server Error** |
| `pipe()`, `fcntl()`, `fork()`, `read()`, or `RequestBody::rewind()` fails | `CgiExecutor::start()` / `readOutput()` | **500 Internal Server Error** |
| Child process exits with non-zero status or is terminated by a signal | `CgiExecutor::reapChild()` | **502 Bad Gateway** |
| Script output is empty, lacks `\r\n\r\n` / `\n\n`, or has malformed headers | `CgiResponse::parse()` | **502 Bad Gateway** |
| Child process runs longer than `DEFAULT_TIMEOUT_SEC` (10 seconds) | `ServerManager::checkCgiTimeouts()` | **504 Gateway Timeout** |

---

## 5. Included CGI Examples (`www/cgi-bin/`)

The repository includes two sample Python CGI scripts configured under `location /cgi-bin`:

1. **`www/cgi-bin/hello.py` (`GET` request with query string)**:
   - Reads the `QUERY_STRING` environment variable set by `CgiRequest` (e.g., `GET /cgi-bin/hello.py?name=42Berlin`).
   - Parses and HTML-escapes the `name` parameter.
   - Writes `Content-Type`, `Content-Length`, `\r\n`, and the rendered HTML document to `sys.stdout`.

2. **`www/cgi-bin/greetings.py` (`POST` request with form body)**:
   - Reads `CONTENT_LENGTH` from `os.environ` and reads the URL-encoded form payload directly from `sys.stdin` (which `CgiExecutor` redirected from `RequestBody`'s temporary file descriptor).
   - Parses and HTML-escapes the `name` field and writes the HTTP response headers and HTML body to `sys.stdout`.

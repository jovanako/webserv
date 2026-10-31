# --> HttpRequest

### Responsibility 
Represents and encapsulates a parsed client HTTP request. It acts as a data container and state tracker that stores the HTTP method (e.g. `GET`, `POST`, `DELETE`), target URI, query string, HTTP protocol version, client remote address, key-value request headers, and the request payload via `RequestBody _body`. It also tracks parsing progress through its `ParsingState` machine (`PARSE_REQUEST_LINE`, `PARSE_HEADERS`, `PARSE_BODY`, `PARSE_DONE`, `PARSE_ERROR`) and preserves any parsing error code encountered.

### Project Context
Operates in the **request parsing** and **ingestion** layer, specifically during non-blocking socket reads on client file descriptors.

### Interactions and Connections

- **RequestParser / Client:** As raw data is received via `recv()`, `RequestParser` parses headers and feeds body chunks into `appendBody()` while advancing `_parseState`, then calls `rewindBody()` once the full payload has arrived.

- **Router / RequestHandler:** Once parsing completes, the routing engine queries `getMethod()`, `getUri()`, `getHeaders()`, and `getBody()` to determine access rules, handle file uploads, or dispatch CGI.

- **CgiRequest:** Supplies request metadata (URI, query string, headers, method, version, remote IP) and its `RequestBody` to initialize a `CgiRequest`.

# --> RequestBody

### Responsibility
Encapsulates the HTTP request body as a file-descriptor-backed resource stored in an unlinked temporary file. It handles lazy creation of the temporary file (`/tmp/webserv_body_<pid>_<counter>`), incremental binary writes (`append()`), offset rewinding (`rewind()`), streaming to upload files (`writeTo()`), and RAII descriptor lifecycle management (`dup()` on copy, `close()` in `reset()` / destructor).

### Project Context
Operates across the **request ingestion**, **file upload**, and **CGI execution** layers, bridging non-blocking network reads with file-descriptor consumers.

### Interactions and Connections

- **HttpRequest & RequestParser:** Owned by `HttpRequest` (`_body`). `RequestParser` appends fixed-length or unchunked body bytes into it and rewinds it once body parsing finishes.

- **CgiRequest:** Copied into `CgiRequest::_body` (duplicating the underlying file descriptor via `dup()`), providing a ready-to-read file descriptor (`getBody().getFd()`) for `dup2(..., STDIN_FILENO)` in the CGI child process.

- **RequestHandler / Client (`handlePost`):** Streams uploaded payload bytes from the temporary file descriptor to the target upload file via `writeTo()`.

# --> CgiRequest

### Responsibility
Prepares and holds the execution context required to launch a CGI script for a given HTTP request, server, and location block. It resolves the CGI interpreter path (`_executable`), script filesystem path (`_scriptPath`), working directory (`_workingDirectory`), RFC 3875 environment variables (`_envVars`), and a `RequestBody` handle (`_body`) for standard input redirection.

### Project Context
Operates in the **CGI preparation and execution** layer when a request URI matches a configured CGI extension in `LocationConfig`.

### Interactions and Connections

- **LocationConfig & ServerConfig:** Queries `LocationConfig::resolveScriptName()` and `LocationConfig::getCgiHandler()` to determine the script boundary (`SCRIPT_NAME` vs. `PATH_INFO`) and interpreter binary, as well as server host/port settings.

- **HttpRequest & RequestBody:** Extracts headers, query string, client IP, and method to build `CONTENT_LENGTH`, `CONTENT_TYPE`, `GATEWAY_INTERFACE`, `PATH_INFO`, `HTTP_*`, and server variables, and copies `request.getBody()` so the CGI process has its own descriptor for `STDIN`.

# --> HttpResponse

### Responsibility

Constructs, formats, and serializes the HTTP response sent back to the client. It translates numeric HTTP status codes (e.g. `200`, `404`, `500`) into standard reason phrases via `getStatusMessage()`, stores response headers, holds the payload body in raw bytes, and serializes the entire message into a valid RFC-compliant byte buffer via `createResponse()`.

### Project Context

Operates in the **response dispatch** and **network output** layer, directly before writing data back to the client socket.

### Interactions and Connections

- **Request Handler/Static File Server:** The handler calls `setStatusCode()`, assigns response headers (like `Content-Type`, `Content-Length`), and loads file content or directory listings into `setBody()`.

- **CGI Handler:** Takes output from the CGI script pipe, extracts returned headers or generated content, and feeds them into the `HttpResponse` object.

- **Client/Socket Manager:** Calls `createResponse()` to retrieve the raw `std::vector<char>` buffer and sends it through non-blocking `write()`/`send()` calls monitored by the central event loop (`poll()` or equivalent).

# --> LocationConfig

### Responsibility

Stores configuration directives for a specific route prefix or URI path rule defined in the configuration file. It holds routing rules including the matching URL `_path`, document root directory (`_root`), allowed HTTP methods (`_allowedMethods`), default index files (`_index`), directory listing permissions (`_autoindex`), file upload destination paths (`_uploadStore`), HTTP redirections (`_redirect`), and CGI interpreter path mappings (`_cgiHandlers`).

### Project Context

Belongs to the **configuration parsing** and **route-resolution** domain.

### Interactions and Connections

- **Config Parser/ServerConfig:** Instantiated and populated during startup while parsing server blocks in the configuration file, typically held in a collection (e.g., `std::vector<LocationConfig>`) inside each `ServerConfig`.

- **Router/Request Handler:** When an `HttpRequest` arrives, the server performs a prefix match between `HttpRequest::getUri()` and `LocationConfig::getPath()`. The matching `LocationConfig` validates whether `HttpRequest::getMethod()` is permitted, determines where files live on disk (`getRoot()`), decides if directory listing should be generated (`getAutoindex()`), or routes requests with matching file extensions to `getCgiHandlers()`.

- **HttpResponse Generator:** Directly influences status codes and headers; for example, if `getRedirect()` is set, the server uses its target URL to set a `3xx` redirect on the `HttpResponse`.

# --> Client

### Responsibility

Manages the entire lifecycle, buffered network I/O, and finite state machine for an individual connected peer socket. It tracks the connection status (`READING_HEADER`, `READING_BODY`, `PROCESSING`, `WRITING_RESPONSE`, `CGI_PIPE_WAIT`, `DONE`), accumulates raw chunks into `_readBuffer` via non-blocking `recv()`, parses headers, routes the URI to matching location directives, performs HTTP status validation (e.g. 404, 405, 413, redirects), and stages data into `_writeBuffer` to be sent back.

### Project Context

Acts as the **central session coordinator and bridge between raw network socket events and application-level HTTP processing**.

### Interactions and Connections

- **ServerManager:** Created  upon a new incoming connection and stored inside `ServerManager::_clients`. `ServerManager` triggers `Client::handleEvent()` when the client socket's file descriptor signals readiness via `poll()`.

- **HttpRequest:** Held as an instance member (`_request`); populated by `Client::parseHeaders()` and `Client::handleReadBody()`, and queried during routing for its method, URI, and content length.

- **HttpResponse:** Held as an instance member (`_response`); modified during `Client::handleProcessing()` to set appropriate status codes, response headers, or redirections.

- **ServerConfig & LocationConfig:** Holds a copy/reference of the associated `ServerConfig` (`_server`). During `handleProcessing()`, it inspects configured locations to enforce allowed HTTP methods, body size restrictions, root directory targets, redirections, and CGI mappings.

# --> ServerConfig
### Responsibility

Acts as a data container storing configuration parameters for a single virtual host or server block. This includes host interface binding address (`_host`, default `0.0.0.0`), port(`_port`, default `8080`), server domain names (`_serverNames`), custom error page mappings (`_errorPages`), maximum allowed request payload size (`_clientMaxBodySize`), and route definitions stored in a list of `LocationConfig` objects.

### Project Context

Resides in the static configuration model, establishing the boundaries, limits, and behavior for each hosted server instance.

### Interactions and Connections

- **ConfigParser:** Populates `ServerConfig` instance line-by-line while parsing blocks from the `.conf` file.

- **ServerManager:** Receives a list of `ServerConfig` instances during initialization. It reads `getHost()` and `getPortString()` to bind and listen on designated network interfaces, and maps listening file descriptors back to their corresponding `ServerConfig` pointers.

- **Client:** Passed to newly accepted `Client` objects via `Client::setServer()`. The client references `getClientMaxBodySize()`, `getErrorPages()`, and `getLocations()` to enforce upload constraints, error pages, and routing rules.

- **LocationConfig:** Holds a collection (`std::vector<LocationConfig>`) of nested route configurations via `addLocation()` and `getLocations()`.

# --> ConfigParser

### Responsibility
Reads, lexes, validates, and parses the NGINX-style configuration file. It strips comments (`#`), pads syntax tokens (`{`, `}`, `;`), tokenizes the input file stream into strings, enforces grammar rules via `verifyToken()`, parses human-readable storage suffixes (`K`, `M`, `G`) into byte values via `parseSize()`, and builds fully initialized `ServerConfig` objects (including nested location blocks and error page directives).

### Project Context
Operates in the pre-runtime/initialization phase before socket creation or event-loop execution.

### Interactions and Connections
- **Main Entry Point (`main.cpp`):** Instantiates `ConfigParser` with the CLI file path argument, calls `parse()`, and catches parsing exceptions if syntax errors occur.

- **ServerConfig:** Instantiates `ServerConfig` objects inside `parseServerBlock()`, fills their directives (ports, names, body sizes, error pages), and appends them to its internal server list.

- **LocationConfig:** Instantiates and populates `LocationConfig` objects inside `parseLocationBlock()` (assigning roots, index files, autoindex flags, CGI routes, upload directories, return redirections) and registers them into the corresponding `ServerConfig`.

- **ServerManager:** The parsed `std::vector<ServerConfig>` returned by `parse()` is passed directly to the constructor of `ServerManager` to boot up the web server.

# --> ServerManager

### Responsibility
Orchestrates the core server runtime, multi-port socket lifecycle, and central I/O event multiplexing loop. It uses `getaddrinfo()`, `socket()`, `setsockopt(SO_REUSEADDR)`, `bind()`, and `listen()` to create non-blocking listening sockets for every configured server block, registers all listening and client file descriptors into a single `poll()` vector (`_pollFds`), handles connection acceptance via `accept()`, dispatches I/O events, and handles client cleanup/closure upon disconnects or errors.

### Project Context
Acts as the network engine and main multiplexing backbone meeting the project requirement for a non-blocking, single-`poll()` architecture.

### Interactions and Connections

- **ServerConfig:** Accepts `std::vector<ServerConfig>` on construction, reads bind information to spin up listening sockets, and maps each listening file descriptor to its matching server definition in `_listenSockets`.

- **Client:** When a listening socket is marked `POLLIN`, `acceptClient()` instantiates a `Client`, sets the socket to non-blocking via `fcntl()`, attaches the associated `ServerConfig`, and stores the instance in `_clients`.

- **Operating System Poll Engine:** Calls `poll()` on `_pollFds`, inspects returned `revents` (`POLLIN`, `POLLOUT`, `POLLERR`, `POLLHUP`), forwards control to `Client::handleEvent()`, toggles polling flags between read and write modes based on `Client::getClientState()`, and removes disconnected clients via `removeClient()`.
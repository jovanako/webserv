# Refactoring Report: Extracting `RequestHandler` in WebServ

This document details the structural, architectural, and API changes made during the extraction of the `RequestHandler` class from the monolithic `Client` class in the 42 `webserv` project.

---

## 1. Architectural Shift

### Before: The Monolithic Coordinator
Previously, the `Client` class handled multiple domains: non-blocking socket I/O, state machine tracking, HTTP request parsing, URI routing, file system access, auto-indexing, and CGI execution. This resulted in a heavily coupled class where business logic and networking concerns were deeply intertwined.

### After: Separation of Concerns & Stateless Routing
By introducing `RequestHandler`, we achieved a clean separation of concerns:
* **`Client`**: Reduced to a pure connection manager. It tracks connection states (`READING_HEADER`, `READING_BODY`, `PROCESSING`, `WRITING_RESPONSE`), manages buffers, and handles socket I/O via `poll()`.
* **`RequestHandler`**: Operates as a stateless utility module. It takes an `HttpRequest`, evaluates server configurations and location blocks, interacts with the file system, and computes the appropriate response or status code without managing socket states or network descriptors.

---

## 2. Function Migrations & Scope Adjustments

All routing, file-serving, and method-dispatching logic was successfully migrated from `Client` to `RequestHandler`.

* **Location Routing:** `findMatchingLocation`
* **Security & Validation:** `isMethodAllowed`, `handleRedirect`
* **Method Dispatchers:** `dispatchMethod`, `handleGet`, `handlePost`, `handleDelete`
* **File System Operations:** `serveExistingPath`, `serveFile`, `findIndexFile`, `redirectDirectoryRequest`
* **Directory Listing:** `serveAutoindex`, `generateAutoindex`, and autoindex HTML formatting helpers (`appendAutoindexHeader`, `appendAutoindexEntries`, `appendAutoindexFooter`, `autoindexBaseUri`)
* **File Upload Management:** `buildUploadPath`, `writeUploadBody`, `setUploadResponse`
* **CGI Execution:** `tryCgiHandler`
* **MIME Mapping Helpers:** `getMimeType`, `getFileExtension`, `lowercaseExtension`, `mimeTypeForExtension` (converted to static utility functions)

---

## 3. API Changes and Interface Design

To decouple `RequestHandler` from `Client`'s member variables, the function signatures and return patterns were redesigned:

1. **Elimination of Side Effects:**
   Methods that previously mutated client state directly (e.g., calling `buildErrorResponse()` or modifying `_clientState` internally) were refactored. 
2. **Explicit Parameter Passing:**
   Functions now accept required objects by reference (`const HttpRequest& request`, `HttpResponse& response`, `const ServerConfig& server`).
3. **HTTP Status Code Contract:**
   The primary entry point, `processRequest()`, returns an `int` representing the HTTP status result:
   * **`200`, `201`, `204`, `3xx`**: Success / Redirection (response body/headers are populated directly in `HttpResponse`).
   * **`400+`**: Error codes (handled cleanly by the client's error page generator).
   * **`0`**: Special signal indicating a CGI script has taken over execution via `execve`, prompting `Client` to switch into `CGI_PIPE_WAIT` state.

---

## 4. Key Benefits

* **Improved Maintainability:** `Client.cpp` was reduced by hundreds of lines, making the connection and event loop logic significantly easier to read and debug.
* **Testability:** `RequestHandler` is stateless. Routing rules, auto-indexing, and file validation can theoretically be tested independently by supplying mock `HttpRequest` and `ServerConfig` objects without needing active socket descriptors.
* **Adherence to 42 Standards:** The codebase now aligns more closely with object-oriented best practices and the Single Responsibility Principle, making code reviews and future feature expansions (like asynchronous CGI pipe handling) much smoother.
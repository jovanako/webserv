# webserv

*This project has been created as part of the 42 curriculum by `aabelkis` and `jkovacev`.*

## Description

`webserv` is a lightweight, non-blocking HTTP server written in C++98, inspired by NGINX.It implements the HTTP/1.0 protocol along with mandatory HTTP/1.1 extensions (such as chunked transfer encoding, `Host` header for virtual hosting, and the DELETE method).

**Features**

- Non-blocking I/O event loop
- Multiple virtual servers and listening ports
- `GET`, `POST`, and `DELETE` HTTP methods
- Static file serving, autoindex, file uploads, and redirections
- Custom error pages and configurable max body size
- CGI/1.1 execution (Python, PHP)

## Instructions

**Prerequisites**

- `c++` compiler supporting `-std=c++98`
- `make`
- `python3` / `php-cgi` (optional, for CGI scripts)

**Build:** `make`

**Run:** `./webserv webserv.conf`

## Usage & Testing Examples

After starting webserv, open a new tab in the browser or type these commands in a new terminal:

- `GET` static page: `curl -i http://127.0.0.1:8080/`
- `POST` upload: `curl -i -X POST -d "hello" http://127.0.0.1:8080/uploads/test.txt`
- `DELETE` file: `curl -i -X DELETE http://127.0.0.1:8080/uploads/test.txt`
- Redirect check: `curl -i http://127.0.0.1:8080/about.html`
- CGI script `curl -i http://127.0.0.1:8080/cgi-bin/<script>.py`


## Resources

- [RFC 9110 - HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)

- [RFC 9112 - HTTP/1.1](https://www.rfc-editor.org/info/rfc9112/)

- [RFC 3875 - CGI/1.1](https://www.rfc-editor.org/info/rfc3875/)

- [mdn HTTP Reference](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference)

- [C++ Reference](https://cplusplus.com/reference/)

- [NGINX HTTP Reference](https://nginx.org/en/docs/http/ngx_http_core_module.html)

- [Linux man pages](https://linux.die.net/man/)

**AI Usage**

AI tools were used as a learning and productivity aid throughout this project, specifically for:

- **Protocol research and clarification:** Explaining details of HTTP/1.0, HTTP/1.1 extension, and CGI/1.1 (RFC 3875) environment variables.

- **Architecture and debugging:** Helping through the design of the classes and modules, as well as identifying missing edge cases (in non-blocking I/O with `poll()`, request parsing, and CGI pipe/timeout handling) and helping diagnose bugs.

- **Testing and documentation:** Drafting test cases (`curl` commands and test scripts) and structuring the project documentation (`README.md`).

All code was reviewed, tested, and verified by the team to ensure full understanding and compliance with the 42 subject requirements.
Searched 42evalhub.com for webserv evaluation sheet 42evalhub.com/common/webserv
Searched "42evalhub.com/common/webserv" OR "webserv" evaluation sheet 42
Searched "webserv" "Check the code" "select" "poll" "epoll" "kqueue" "siege" evaluation sheet 42
Searched "webserv" "Mandatory Part" "Check how" "Multiple servers" "CGI" evaluation 42

Based on the 42 **Webserv** evaluation sheet, strict evaluators go section by section—starting with compilation and architecture, then testing HTTP/config edge cases, and finally trying to break your server with non-blocking I/O traps, CGI edge cases, and `siege`.

Here are the questions they typically ask, ordered from **basic** to **difficult**.

---

### 1. Basic Questions (Preliminaries, Code & Setup)
Evaluators usually start here to verify you actually wrote and understand the codebase:

* **Compilation & Standard:**
  * Does it compile cleanly with `c++ -Wall -Wextra -Werror -std=c++98`? Does `make` relink if nothing changed?
* **Architecture Overview:**
  * Walk me through your classes. How does a request flow from `accept()` to sending the final HTTP response?
  * Which I/O multiplexing function did you choose (`select`, `poll`, `epoll`, or `kqueue`), and why?
* **HTTP Basics:**
  * Which HTTP methods did you implement (`GET`, `POST`, `DELETE`), and what status code do you return for an unsupported or disallowed method (e.g., `405 Method Not Allowed` vs `501 Not Implemented`)?
  * How do you determine the `Content-Type` (MIME type) of a served file?
  * What is the difference between `root` and `alias` (if implemented), and how do you resolve a URI to a filesystem path?
* **Basic Config & Browser Checks:**
  * How do you configure multiple ports (e.g., `8080` and `8081`)?
  * What happens when a client requests a directory:
    * With an `index` file present?
    * Without an `index` file and `autoindex on` vs `autoindex off`?
  * Can you show a custom error page (e.g., `404` or `403`) configured in `.conf`?

---

### 2. Intermediate Questions (Config, Routing, Uploads & CGI)
Once the basics work, evaluators test whether your server follows the subject's rules on configuration, routing, and CGI:

* **Multiple Servers & Virtual Hosting:**
  * What happens if two `server` blocks listen on the **same port** with different `server_name` values? How do you route based on the `Host` header?
  * What happens if a request arrives with an unknown `Host` header? (It should fall back to the **first** server block listening on that `host:port`.)
  * What happens if two `server` blocks have the exact same `host:port` and `server_name`, or if a port is already in use? Does the server fail cleanly without crashing?
* **Request Limits & Parsing:**
  * How do you enforce `client_max_body_size`? What status code do you return when it is exceeded (`413 Payload Too Large`)?
  * What happens if a client sends a **malformed HTTP request** (missing `Host` in HTTP/1.1, invalid method, bad HTTP version, or garbage bytes via `nc`/`telnet`)?
  * How do you handle `POST` file uploads and `DELETE` requests? Do you prevent path traversal (e.g., `DELETE /../../etc/passwd`)?
* **CGI Fundamentals:**
  * Which CGI(s) do you support (Python, PHP, etc.), and how do you know when to trigger CGI (by file extension or location)?
  * Which environment variables do you pass to `execve` (`REQUEST_METHOD`, `QUERY_STRING`, `CONTENT_LENGTH`, `CONTENT_TYPE`, `PATH_INFO`, `SCRIPT_FILENAME`, `SERVER_PROTOCOL`, etc.)?
  * How do you pass a `POST` request body into the CGI script's `stdin`?

---

### 3. Difficult / Strict Evaluator Questions (The "Gotchas")
Strict evaluators specifically look for the **0-grade traps** on the evaluation sheet—especially around non-blocking I/O, chunked encoding, CGI failure modes, and stress testing:

* **The Strict Non-Blocking I/O Rules:**
  * *"Show me every `read`/`recv` and `write`/`send` in your codebase."*
    * Are **all** socket reads/writes and CGI pipe reads/writes going through your single `poll`/`epoll`/`select`/`kqueue` loop?
    * Do you perform **at most one** `read`/`recv` or `write`/`send` per ready file descriptor per event loop iteration?
    * **The `errno` trap:** Do you check `errno` (like `EAGAIN` / `EWOULDBLOCK`) after `read`/`recv` or `write`/`send` to retry or keep the connection alive? *(The 42 Webserv sheet strictly forbids checking `errno` after read/write to manage flow—you must rely on the multiplexer's readiness events and handle `<= 0` properly.)*
  * What happens if a client sends a request **one byte at a time** (slowloris / partial `recv` in `telnet` or `nc`) or disconnects mid-request? Does your server buffer properly without blocking other clients?
  * What happens if `send()` only sends part of a large response (partial write)? Do you wait for the next `POLLOUT`/`EPOLLOUT` event to send the remaining bytes?
* **Chunked Transfer-Encoding:**
  * How do you unchunk a `Transfer-Encoding: chunked` request body?
  * Does chunked `POST` work **when routed to a CGI**? (Your server must de-chunk the body before piping it to the CGI's `stdin`, or pass the correct `CONTENT_LENGTH`.)
* **CGI Edge Cases & Process Management:**
  * **Infinite loop / Hanging CGI:** *"What happens if I run a CGI script with `while True: pass`?"*
    * Does your entire server freeze, or can other clients still load pages while that CGI hangs?
    * Do you have a timeout that kills the hung child process (`kill` + `waitpid`) and returns `504 Gateway Timeout`?
  * **Zombie processes:** Do you reap all CGI child processes (e.g., `waitpid(-1, &status, WNOHANG)` or tracking PIDs), or do zombies accumulate in `ps aux`?
  * **CGI Working Directory:** If a CGI script opens a relative file (`open("data.txt")`), does your server `chdir` to the script's directory in the child process before `execve`?
  * **CGI Headers & EOF:**
    * What if the CGI outputs headers with `\r\n` vs `\n`, or omits `Content-Length`? Do you read until pipe `EOF`?
    * What if the CGI crashes (non-zero exit code) or outputs malformed headers? Do you return `500`/`502` instead of crashing the server?
* **Resource Leaks & Stress Testing (`siege`):**
  * **FD Leaks:** *"Let's check `lsof -p <pid>` before and after 1,000 requests and CGI calls."* Are all client sockets and CGI pipe FDs closed properly on errors, timeouts, and disconnects?
  * **Siege (`siege -b http://localhost:PORT/`):**
    * Is availability **>= 99.5%**?
    * Are there any hanging sockets, memory spikes (`valgrind` / `leaks`), or dropped connections under concurrency?
  * **Connections:** How do you handle `Connection: keep-alive` vs `Connection: close`, and do you time out idle keep-alive clients?
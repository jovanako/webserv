# --> Understanding PollFds Usage

The `_pollFds` vector (containing standard POSIX `struct pollfd` elements) serves as the central mechanism for monitoring asynchronous I/O events within the `ServerManager`.

Its primary functions are:

- **Central Socket Registry:** It stores all active file descriptors that the server needs to monitor, including the main listening server sockets and all active client connection sockets.

- **Driving the Non-Blocking Loop:** It is passed directly to the `poll()` system call in the `run()` loop, allowing the server to wait indefinitely for I/O events across all monitored sockets without blocking execution.

- **Event Detection:** After `poll()` executes, the `ServerManager` iterates through `_pollFds` and checks the `revents` field of each struct to determine if a socket is ready to be read (`POLLIN`), ready to be written to (`POLLOUT`), or has encountered a disconnection/error (`POLLHUP`, `POLLERR`, `POLLNVAL`).

- **State Synchronization:** The server actively updates the requested `events` field of each struct to match the client's current state, such as switching from `POLLIN` to `POLLOUT` when a client finishes processing and enters the `WRITING_RESPONSE` state.

- **Dynamic Scaling:** The vector expands dynamically when new client connections are accepted (temporarily stored in `pendingFds` before being appended) and shrinks when clients are disconnected via `removeClient()`.

# --> `ServerManager::acceptClient`

Passing `NULL` for the `sockaddr` and `socklen_t` arguments in the `accept()` function is standard POSIX practice if your server does not need to record or evaluate the client's IP address or port number.

## `accept()`

The `accept()` function is a core system call used in network programming to establish a connection with a client. When a server is actively listening for incoming traffic on a specific port, `accept()` is used to pick up a pending connection request and initialize communication.

Here is a breakdown of how it functions within your `ServerManager::acceptClient` method:

- **Creates a New Socket:** `accept()` does not use the server's main listening socket (`listenFd`) to exchange data with the client. Instead, it extracts the first connection request from the queue and creates a brand-new socket file descriptor (`clientFd`) dedicated exclusively to communicating with that specific client. The original listening socket remains open and continues to listen for new incoming connections.

- **The Arguments (`listenFd, NULL, NULL`):**

	* The first argument is the server's listening file descriptor.

	* The second and third arguments are typically used to pass a `sockaddr` structure and its length to capture the client's IP address and source port. By passing `NULL` for both, the program explicitly ignores the client's address information, as the server's current logic does not require it to process the request.

- **Error Handling and Non-Blocking:** If `accept()` fails, it returns a value less than 0. Because the project strictly requires a non-blocking server architecture, calling `accept()` on a non-blocking listening socket when no connections are pending will cause it to return an error (usually setting `errno` to `EAGAIN` or `EWOULDBLOCK`). The code safely accounts for this by checking `if (clientFd < 0)` and returning immediately without crashing.

## `EAGAIN` and `EWOULDBLOCK`

`EAGAIN` and `EWOULDBLOCK` are standard POSIX error codes that indicate an operation on a non-blocking file descriptor cannot be completed immediately.

## `acceptClient()`

This method handles new incoming connections, taking the server's listening socket (`listenFd`) and a reference to a temporary vector (`pendingFds`) used to store new connections safely during the current polling cycle.

- `int clientFd = accept(listenFd, NULL, NULL);`  
Extracts the first connection request from the listening socket's queue and creates a new, dedicated file descriptor (`clientFd`) for the client. Passing `NULL` ignores the client's source IP and port details, which are not currently needed.

- 
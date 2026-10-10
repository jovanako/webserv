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

- **EAGAIN (Try again):** Historically, this means "Resource temporarily unavailable." It tells the application that the requested action cannot be performed right now, but it might succeed  if attempted again later.

- **EWOULDBLOCK (Operation Would Block):** This specifically indicates that the requested operation (like reading, writing, or accepting a connection) would force the thread to pause (block) if the socket were operating in its default, blocking mode.

**How they apply to your server:**  
Because your project strictly requires all sockets to be non-blocking, system calls like `accept()` cannot wait around for a client to connect. If your `poll()` loop wakes up but by the time you call `accept()` the connection queue is empty, the operating system returns `-1` and sets `errno` to one of these values.

They are not actual "errors" or failures; they are simply the operating system's way of saying, "There is nothing here to do right now, go back to your polling loop."

*(NOTE: On most modern operating systems, including Linux and macOS,* `EAGAIN` *and* `EWOULDBLOCK` *are defined as the exact same integer value. However, it is standard practice to check fo rboth to ensure complete POSIX compliance across all platforms.)*

## `acceptClient()`

This method handles new incoming connections, taking the server's listening socket (`listenFd`) and a reference to a temporary vector (`pendingFds`) used to store new connections safely during the current polling cycle.

- `int clientFd = accept(listenFd, NULL, NULL);`  
Extracts the first connection request from the listening socket's queue and creates a new, dedicated file descriptor (`clientFd`) for the client. Passing `NULL` ignores the client's source IP and port details, which are not currently needed.

- `if (clientFd < 0) return;`  
Checks if `accept()` call failed. In non-blocking servers, this often means there were simply no pending connections (returning an `EAGAIN` or `EWOULDBLOCK` error), so the function safely exits without crashing.

- `fcntl(clientFd, F_SETFL, O_NONBLOCK);`  
Enforces the project's strict non-blocking requirement on the newly created client socket. This ensures future read/write operations on this specific client will not freeze the main server loop.

- `struct pollfd pfd;`  
`pfd.fd = clientFd;`  
`pfd.events = POLLIN;`  
`pfd.revents = 0;`  
Initializes the new `pollfd` to the temporary vector. This prevents the main `_pollfds` vector from resizing or shifting while the `run()` loop is actively iterating over it, which could cause skipped events or segmentation faults.

- `Client client(clientFd);`  
Instantiates a new `Client` object, passing the new file descriptor to its constructor to manage state, buffers, and the eventual HTTP request/response cycle.

- `std::map<int, ServerConfig*>::iterator it = _listenSockets.find(listenFd);...`  
Looks up the original listening socket in the `_listenSockets` map to find the specific `ServerConfig` block the client connected to. It then assigns those base settings (like default error pages or max body size) directly to the client.

- `client.setVirtualHosts(_servers);`  
Passes the entire list of parsed server configurations to the client. This provides the necessary data for the client to eventually resolve `server_name` routing (virtual hosts) if multiple server blocks share the same listening port.

- `_clients[clientFd] = client;`  
Stores the fully initialized client object into the central `_clients` map, using its file descriptor as the key. This guarantees fast retrieval of the client's state the next time `poll()` detects activity on this socket.

# --> Difference between `std::runtime_error` and `std::cerr`

`std::runtime_error` and `std::cerr` serve completely different roles in C++: one is an **exception type** used for flow control when an error happens, while the other is an **output stream** used for printing messages.

### Key Differences

**Category**

`std::runtime_error`: Exception class (inherits from `std::exception`)

`std::cerr`: Standard error output stream (`std::ostream`)

**Header**

`std::runtime_error`: `<stdexcept>`

`std::cerr`: `<iostream>`

**Primary Role**

`std::runtime_error`: Signal an unrecoverable or exceptional event

`std::cerr`: Display log messages, errors, or warnings

**Control Flow**

`std::runtime_error`: Interrupts execution immediately and unwinds the stack until caught by a `catch` block; terminates the program if unhandled

`std::cerr`: Does not change control flow; program execution continues to the next line immediately

**Buffering**

`std::runtime_error`: N/A (it is an object, not a stream)

`std::cerr`: Unbuffered (writes immediately to console/file descriptor 2)

## Detailed Breakdown

1. `std::runtime_error`

- **What it is:** A standard exception class representing errors detectable ony while the program is running (e.g., config parsing failure, invalid syntax, missing resources).

- **How it works:** When thrown with `throw std::runtime_error("details")`, the C++ runtime stops regular execution, tears down local variables (stack unwinding), and jumps to the nearest matching `catch (const std::exception& e)` block.

- **When to use it:** When an operation cannot continue or produce a valid result, such as parsing an invalid directive in `ConfigParser.cpp` during startup where halting configuration loading is mandatory.

2. `std::cerr`

- **What it is:** The predefined standard error stream, tied to file descriptor `2` (`stderr`).

- **How it works:** It behaves similarly to `std::cout`, but it is typically unbuffered so output appears on the terminal immediately without waiting for a newline or an explicit flush.

- **When to use it:** When you want to print a warning, debug message, or log a non-fatal failure without interrupting the server loop (for example, logging a failed `accept()` or file read while continuing to run other connections).

## How They Work Together

They are commonly used in tandem: you throw `std::runtime_error` at the failure site to bubble the problem up, and you catch it at a high level (e.g., in `main()`) to report the error via `std::cerr`:

```
int main(int argc, char** argv) {
    try {
        ConfigParser parser(argv[1]);
        std::vector<ServerConfig> configs = parser.parse();
        // start server...
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

# `F_SETFL`

`F_SETFL` is a command constant used with the POSIX `fcntl()` system call that stands for **File Set Status Flags**.

## Core Role

When passed to `fcntl()`, `F_SETFL` instructs the operating system kernel to update the file status flags of an open file descriptor (such as a socket pipe).

In your `webserv` implementation, it is used specifically to switch sockets into non-blocking mode:

`fcntl(clientFd, F_SETFL, O_NONBLOCK);`

## Breakdown of the Call

- `clientFd`: The file descriptor you want to configure.

- `F_SETFL`: The operation/command telling `fcntl()`: *"Overwrite the file status flags for this descriptor with the value provided in the third argument."*

- `O_NONBLOCK`: The target status flag, which disables blocking I/O behavior.

## Why it matters in `webserv`

By default, newly created sockets (from calls like `socket()` or `accept()`) operate in blocking mode, meaning system calls such as `read()`, `write()`, `send()`, or `recv()` will pause thread execution indefinitely until data arrives or buffer space clears.

Applying `fcntl(clientFd, F_SETFL, O_NONBLOCK)` prevents thread blocking so that the single `poll()` event loop can handle thousands of client connections concurrently without stalling.

# `struct pollfd`

The `struct pollfd` is a standard POSIX data structure defined in `<poll.h>` that provides the kernel with the exact file descriptors and event types you want to monitor when calling `poll()`.

## Structure Definition

```
struct pollfd {
    int   fd;       // File descriptor to monitor
    short events;   // Bitmask of events you are requesting to monitor
    short revents;  // Bitmask of events returned by the kernel
};
```

## Field Descriptions

- `fd`: The open file descriptor assigned to the struct (for example, the main listening socket or an accepted client socket).

- `events`: An input bitmask set by the application indicating what conditions should trigger a notification:
	* `POLLIN`: Notifies when data is ready to be read without blocking.
	* `POLLOUT`: Notifies when buffer space is available to write data without blocking.

- `revents`: An output bitmask filled by the kernel when `poll()` returns. It reports which events actually occurred, including errors or hang-ups (e.g., `POLLIN`, `POLLOUT`, `POLLHUP`, `POLLERR`, `POLLNVAL`).

## Step-by-Step Usage in `webserv`

**1. Initialization:** When a new socket is opened or accepted, create an instance of `struct pollfd`, set `fd`, set `events = POLLIN`, and initialize `revents = 0`:

```
struct pollfd pfd;
pfd.fd = clientFd;
pfd.events = POLLIN;
pfd.revents = 0;
```

**2. Registration:** Store these structs in a contiguous container, such as `std::vector<struct pollfd> _pollfds`.

**3. Execution:** Pass the address of the underlying array to `poll()`:

`poll(&_pollFds[0], _pollFds.size(), -1);`

**4. Inspection:** Iterate through the array after `poll()` unblocks to check `revents` with bitwise operations:

- `if (_pollFds[i].revents & POLLIN)`: Read incoming client requests or call `accept()` on listening sockets.

- `if (_pollFds[i].revents & POLLOUT)`: Send the prepared HTTP response.

- `if (_pollFds[i].revents & (POLLERR | POLLNVAL | POLLHUP))`: Close the descriptor and remove the struct from tracking.

**5. State Synchronization:** Modify the `events` field depending on client progress. For instance, once an entire HTTP request is read and processed, switch the descriptor's mode from `_pollFds[i].events = POLLIN` to `_pollFds[i].events = POLLOUT` so the server waits for write availability without spinning the CPU.

# `boundAddresses`

`std::set<std::pair<std::string, int> > boundAddresses;`

This line declares an empty collection named `boundAddresses` that is specifically designed to store unique pairs of strings and integers (representing the `host` and `port`).

`std::set` **does not allow duplicate elements**. When you attempt to insert a `host` and `port` combination that is already inside the set, the `std::set` simply ignores the new insertion. 

*(Note on the syntax: The space between the two closing angle brackets`> >` is required in standard C++98 to prevent the compiler from misinterpreting it as the `>>` bitwise shift operator.)*

### Why introduce `boundAddresses`

Because you **cannot and should not** bind multiple times to the exact same IP and port combination.

Here is why it works this way:

- **The Transport Layer (TCP/Sockets):** A port is like a single physical door to a building. You only need one socket to open that door and listen for incoming traffic on that port (e.g., port 8080). If you try to call `bind()` a second time on the exact same port and IP, the operating system will reject it with an "Address already in use" error.

- **The Application Layer (HTTP):** Once a client connects through that single door, they send an HTTP request. This request contains a `Host` header (e.g., `Host: webserv.com` or `Host: test.local`).

Because you only have one socket listening on port 8080, all traffic for *both* virtual hosts comes through that single socket. Your server reads the `Host` header to figure out which website the client actually wants.

# `count()`

In C++, `count()` function searches a container for a specific element and returns the number of times that element appears.

Because an `std::set` strictly enforces uniqueness, `count()` will only ever return one of two values when used on a set:

- `1`: The element exists in the set.

- `0`: The element does not exist in the set.

In the context of the virtual host check  
(`boundAddresses.count(std::make_pair(host, port)) > 0`), it acts as a simple boolean check. It tells the program: *"If this exact host and port combination appears 1 time in our tracked list, we know we've already set up a socket for it, so we can skip binding a new one*.
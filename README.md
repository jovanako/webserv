## Headers

### The golden rule for your parser: `\r\n\r\n`

How does your C++ code know where the shipping label ends and the contents of the box begin?

HTTP has a very strict rule: **The header and the body are always separated by a single, completely empty line.**

In programming terms, a new line in HTTP is represented by `\r\n` (carriage return + line feed). Therefore, the end of the headers is always marked by `\r\n\r\n`.

Here is a complete HTTP response, exactly as your server will send it back to a browser:

```
HTTP/1.1 200 OK\r\n
Server: MyWebserv/1.0\r\n
Content-Type: text/html\r\n
Content-Length: 46\r\n
\r\n
<html><body><h1>Hello World!</h1></body></html>
```

### How to use this in your code:

When you write your `recv()` loop to read data from the client:

1. Read the incoming characters and look for that `\r\n\r\n` sequence.
2. Everything **before** it gets passed to your Header Parsing function to figure out what the client wants.
3. Everything **after** it is the Body.
4. If the client is sending you data (like a file upload via a `POST` request), you look at the `Content-Length` header you just parsed. If it says `Content-Length: 5000`, you know you need to keep reading from the socket until your body contains exactly 5000 bytes.

### Where does Header live?

Header does not live in a pre-written file anywhere in your project folder.

Instead, **your C++ code builds this text from scratch** every single time a browser asks for a webpage.

In the architecture of your **webserv** project, this process usually lives inside a class called something like `HttpResponse` or `ClientHandler`.

Here is a breakdown of how and where your code will generate this string.

### 1. The Assembly Line (Your C++ Code)

When your server realies a client wants to see `index.html`, and your server successfully finds that file on your hard drive, your code will start concatenating a massive `std::string`.

It looks roughly like this in C++:

```
#include <string>
#include <sstream>

// Inside your HttpResponse class method:
std::string buildResponse(std::string fileContent) {
	std::stringstream response;

	// 1. Add the Status Line
	response << "HTTP/1.1 200 OK\r\n";

	// 2. Add the Headers
	response << "Server: MyWebserv/1.0\r\n";
	response << "Content-Type: text/html\r\n";

	// Calculate the exact size of the body
	response << "Content-Length: " << fileContent.length() << "\r\n";

	// 3. Add the required empty line
	response << "\r\n";

	// 4. Add the Body
	response << fileContent;

	return response.str();
}
```

### 2. Where do the pieces come from?

Your server has to figure out the values for these headers dynamically before it builds the string:

- `200 OK`: Your code determines this because it successfully found and opened the requested file. If the file was missing, your code's `if/else` logic would swap this out for `404 Not Found`.

- `text/html`: Your code will look at the file extension. You will likely write a small helper function that says: "If the file ends in `.html`, use `text/html`. If it ends in `.png`, use `image/png`." (These are called MIME types).

- `46`: Your code calculates this by checking the size of the file (using `std::string::length()` or the `stat()` system call).

- **The Body (HTML)**: This *does* live in a file! Your server will use `std::ifstream` to open a real file on your computer (like `/var/www/html/index.html`), read its contents into a variable, and stick it at the very end of the string.

### The Final Step: `send()`

Once your C++ code has glued all of this together into one giant `std::string`, where does it go?

It goes to the `<sys/socket.h>` library. You will use the `send()` function to push this entire text string through your network socket, over the Wi-Fi cable, and directly into the Chrome or Firefox browser waiting on the other side.

## What is an `.ipp` file?

In C++, an `.ipp` file stands for Inline Plus Plus (or Implementation Plus Plus). It is a file used to store implementation of **templates** or **inline functions**.

To understand why it exists, you have to look at how C++ normally splits files, and the problem that creates for templates.

### The Problem: Templates Break the Normal Rules

Normally, in C++, you split your code into two files:

1. `.hpp` **(Header)**: Containt the declarations (the "table of contents" for your class).

2. `.cpp` **(Source)**: Contains the actual logic (the implementation). These are compiled individually.

**But templates are different.** When you write a template class (e.g. `template <typename T> class MyArray`), the C++ compiler needs to see the *entire implementation* right when you use it. If you put the implementation in a `.cpp` file, the compiler will throw "undefined reference" linker errors because it can't find the code it needs to generate the specific version of your template (like `MyArray<int>`).

Because of this, you are forced to put the implementation of template classes directly inside the `.hpp` file.

### The Solution: `.ipp` Files

If you put all the implementation code inside your `.hpp` file, that file becomes massive, cluttered, and hard to read.

The `.ipp` file is a purely organizational trick to keep your headers clean.

**Here is how it works**:

1. You put your declarations in the `.hpp` file.

2. You put your template implementations in the `.ipp` file.

3. At the very bottom of your `.hpp` file, you `#include` the `.ipp` file.

### What it looks like in code:

1. `MyTemplate.hpp` **(Clean and easy to read)**

```
#ifndef MYTEMPLATE_HPP
#define MYTEMPLATE_HPP

template <typename T>
class MyTemplate {
	private:
		T _data;
	public:
		MyTemplate(T data);
		T getData() const;
};

// Include implementation at the end!
#include "MyTemplate.ipp"

#endif
```

2. `MyTemplate.ipp` **(The Messy Implementation)**

```
// Notice: No #include "MyTemplate.hpp" at the top.
// This file is meant to be injected INTO the header.

template <typename T>
MyTemplate<T>::MyTemplate(T data) : _data(data) {}

template <typename T>
T MyTemplate<T>::getData() const {
	return _data;
}
```

> `.tpp` is the same as `.ipp`.


## `std::vector` and `std::map`

`std::vector` and `std::map` are fundamental container types from the C++ Standard Template Library (STL) designed for distinct data management patterns:

- `std::vector` is a dynamic array stored in contiguous memory that expands or shrinks automatically.

- `std::map` is an associative container of sorted key-value pairs, implemented under the hood as a self-balancing bunary search tree (typically a Red-Black tree).

### Key Comparison

**Data Structure** 
- `std::vector<T>`: Dynamic array (contiguous memory)

- `std::map<Key, Value>`: Balanced binary search tree (node-based)

**Element Access**

- `std::vector<T>`: By integer index (`0` to `size-1`)

- `std::map<Key, Value>`: By unique `Key`

 **Ordering**

- `std::vector<T>`: Insertion order preserved

- `std::map<Key, Value>`: Sorted by `Key` (using `operator<`)

**Header**

- `std::vector<T>`: `#include <vector>`

- `std::map<Key, Value>`: `#include <map>`

### `std::vector` in Detail

Use `std::vector` when you need a sequence of items, fast sequential iteration, cache-friendly memory layout, or direct index-based access (*O*(1)).

> *O*(...) stands for **"Order of"** (often referred to as **Big-O Notation**). It describes the **order of magnitude** of an algorithm's growth rate - meaning how the running time or memory usage scales as the input sie (*n*) grows toward infinity.

```
#include <iostream>
#include <vector>

int main() {
	// Declaration and initialization
	std::vector<int> scores = {85, 92, 78};

	// Appending elements
	scores.push_back(95);
	scores.emplace_back(88);

	// Direct access
	int first = scores[0];		// No bounds checking
	int second = scores.at(1);	// Throws std::out_of_range if invalid

	// Iterating
	for (int score : scores) {
		std::cout << score << " ";
	}
	std::cout << "\nSize: " << scores.size() << "\n";
}
```

- **Memory behavior**: When full, vector allocates a larger chunk of memory (often 1.5x or 2x capacity), moves existing elements, and frees old storage. Use `scores.reserve(N)` if you know the size ahead of time to avoid reallocations.

### `std::map` in Detail

Use `std::map` when you need to associate values with unique identifiers (keys), retrieve items by key efficiently (*O*(log *n*)), and keep elements automatically sorted.

```
#include <iostream>
#include <map>
#include <string>

int main() {
	// Declaration: std::map<KeyType, ValueType>
	std::map<std::string, int> inventory;

	// Insertion
	inventory["apples"] = 10;			// Inserts or overwrites
	inventory.insert({"bananas", 5});	// Only inserts if key doesn't exist

	// Lookup: .find() returns an iterator (avoids inserting default values)
	auto it = inventory.find("apples");
	if (it != inventory.end()) {
		std::cout << "Apples in stock: " << it->second << "\n";
	}

	// Careful with operator[]:
	// Accessing a missing key automatically inserts it with a default value (0)!
	std::cout << "Oranges: " << inventory["oranges"] << "\n; // Inserts "oranges": 0

	// Iterating: always ordered alphabetically by key
	for (const auto& [item, count] : inventory) {
		std::cout << item << ": " << count << "\n";
	}
}
```

### What is `auto`?

In C++, `auto` is a keyword that tells the compiler to automatically deduce the type of a variable from its initialization expression at **compile time**.

It does not make C++ dynamically typed like Python or JavaScript. The variable is still strictly and statically typed - you just don't have to type out the long type name yourself.

```
auto it = inventory.find("apples");
```

Assuming `inventory` is defined as:

```
std::map<std::string, int> inventory;
```

Without `auto`, you would have to write the full, verbose iterator type:

```
std::map<std::string, int>::iterator it = inventory.find("apples");
```

With `auto`, the compiler sees that `.find()` returns a `std::map<std::string, int>::iterator`, so it substitutes that exact type for `it` during compilation with **zero runtime overhead**.

### What does `it` actually hold?

Because `it` is an iterator to a map element, it behaves like a pointer to a pair of values:

- `it->first`: the **key** (`"apples"`, of type `const std::string`)

- `it->second`: the **value** (the count, of type `int`)

# webserv processes

- **Request Receipt and Header Reading**: When a client sends a request, the server detects read-readiness via `poll()` and reads raw bytes from the socket into a buffer until it encouters the double CRLF (`\r\n\r\n`) delimiter marking the end of the HTTP headers.

- **Parsing and Body Reading**: The `HttpRequest` object decodes the request line and headers. If a body is attached (such as in a `POST` request), the server continues reading the payload based on the `Content-Length` header or un-chunks the data if chunked transfer encoding is used.

- **Processing and Route Evaluation**: Once the request parsing finishes (`PARSE_DONE`), the server evaluates the request against configuration routing rules, validates allowed methods (`GET`, `POST`, `DELETE`), checks file permissions, and determines whether to serve a static file or execute a CGI script.

- **CGI Execution (Conditional)**: If a dynamic script (like PHP or Python) is triggered, the server sets up anonymous pipes, forks a child process while passing environment variables, and monitors the CGI output asynchronously without freezing the main loop.

- **Response Sterilization and Writing**: The server packages the response headers and payload into a serialized byte stream. Once `poll()` signals write-readiness (`POLLOUT`), the server transmits the bytes back to the client using non-blocking `send()` or `write()` operations.

- **Cleanup and Termination**: After the complete response has been flushed to the client, the server either resets the state for persistent keep-alive connections or closes the file descriptor, frees memory, and removes the socket from the `poll()` array.

# `recv()`

The `recv()` function is a POSIX system call used to read incoming data from a connected network socket. It acts as the primary way your web server pulls the raw HTTP text sent by the client's browser into your program's memory.

### The Syntax

```
#include <sys/socket.h>

ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

### The Arguments

- `sockfd`: The file descriptor of the connected client socket you want to read from.

- `buf`: A pointer to the memory location (usually a `char` array or `std::vector` buffer) where the incoming bytes will be stored.

- `len`: The maximum number of bytes you want to read in this single call (which should match or be smaller than the size of your buffer to prevent overflow).

- `flags`: Special behavior modifiers. For standard HTTP reading, this is almost always set to `0`.

### Interpreting Return Value

The number returned by `recv()` is critical for your state machine logic, dictating exactly what your server should do next.

**Return Values**:

`> 0`

*Meaning*: **Success**: The number of bytes successfully read into your buffer.

*Server Action*: Append these bytes to your request parser. If the request isn't fully received yet, wait for the next `poll()` event.

`0`

*Meaning*: **Graceful Disconnect**: The client completely closed their end of the connection.

*Server Action*: Safely destroy the client object, close the file descriptor, and remove it from your `poll()` array.

`< 0`

*Meaning*: **Error or Block**: An error occured, or no data is currently available on a non-blocking socket.

*Server Action*: Handle the error or wait for the next `poll()` event.

> **The Non-Blocking Requirement in Webserv**\
Because your project strictly requires all sockets to be non-blocking, `recv()` behaves differently than standard blocking I/O. If you call `recv()` and there is no data waiting on the socket, it will not pause your program to wait for data. Instead, it instantly returns `-1`.\
 \
To prevent wasting CPU cycles constantly checking empty sockets, your server must never call `recv()` unless `poll()` (or `select`/`epoll`) has already confirmed that the socket has data to be read. Calling `recv()` on a descriptor without prior readiness notification from your event loop is a severe violation of the project rules.

# socket

A network socket is a software endpoint the establishes a **bidirectional communication channel** between two programs across a network. It acts as the literal **gateway** through which your server sends and receives all HTTP data.

**In Unix-based systems, a socket is simply treated as a file descriptor** - an integer that the kernel uses to track an open I/O stream. Just like you can use `read()` and `write()` on a text file, you use them (or `recv()` and `send()`) on a socket to talk to a web browser.

In your web server, you will manage two entirely different categories of sockets:

**1. The Listening Sockets (The Receptionists)**

- **Creation**: When your server boots, it uses the `socket()` system call to create these based on your configuration file.

- **Identity**: You use `bind()` to lock them to a specific IP address and port (e.g., `0.0.0.0:8080`), and `listen()` to tell the OS they are ready to receive traffic.

- **Purpose**: These sockets *never* read or write HTTP data. Their only job is to trigger a `POLLIN` event in your event loop when a new user tries to connect. When that happens, you call `accept()`.

**2. The Client Sockets (The Workers)**

- **Creation**: Every time you call `accept()` on a listening socket, the kernel generates a brand new, unique client socket (returning a new file descriptor).

- **Purpose**: This is the actual dedicated pipeline to that specific user's web browser.

- **Action**: You add this new file descriptor to your `poll()` array. When it triggers `POLLIN`, you call `recv()` to read the incoming HTTP request. When you are ready to reply and it triggers `POLLOUT`, you call `send()` to transmit the serialized HTTP response.

> Because of your project's strict rules, every single one of those sockets (both listening and client) must be set to non-blocking mode using  `fcntl()` to ensure the server never freezes while waiting for network traffic.

Parameter Breakdown: `socket(AF_INET, SOCK_STREAM, 0)`
When building an HTTP server, you pass three specific arguments to configure the socket for web traffic:

Domain (AF_INET): Specifies the address family. This explicitly instructs the kernel that this socket will operate over IPv4 networks, rather than IPv6 (AF_INET6) or local Unix pipes (AF_UNIX).

Type (SOCK_STREAM): Defines the communication semantics. This guarantees a reliable, sequenced, and two-way connection-oriented byte stream. In the networking world, this translates directly to requiring the TCP protocol (which HTTP relies on to ensure no data is lost in transit).

Protocol (0): Instructs the kernel to automatically select the default underlying protocol that matches your requested domain and type. For AF_INET and SOCK_STREAM, the default is TCP.



## ServerManager: The Network Orchestrator

This class handles the macro-level network environment and the core multiplexing loop.  Initialization: It loops through your ServerConfig vector to create, configure (O_NONBLOCK), bind, and listen on the master sockets.  State Tracking: It maintains the array of pollfd structures required by poll(), alongside a map linking active client file descriptors to their corresponding Client objects.The Loop: It runs the infinite while(true) loop calling poll().  Dispatching: When poll() wakes up, the ServerManager routes the event. If a master socket is readable, it calls accept(), configures the new socket as non-blocking, and instantiates a new Client. If an existing client socket triggers an event, it delegates the work to that specific Client object.  

## Client: The Connection Handler

Your Client class already has the perfect scaffolding to handle the micro-level logic for individual connections. The ServerManager simply tells the Client when it is allowed to act.  State Machine: When the ServerManager detects POLLIN or POLLOUT, it calls client.handleEvent().  Data Assembly: The Client performs the actual recv() call and appends data to its internal _readBuffer.  Protocol Logic: The Client handles the HTTP parsing and transitions from READING_HEADER to READING_BODY based on the bytes received.  Response Transmission: When the ServerManager detects a socket is ready to write, the Client performs the send() call to transmit the generated response.By structuring it this way, your ServerManager never needs to parse HTTP, and your Client never needs to know how poll() works.

# `data()`

The `.data()` method returns a direct pointer (`const char *`) to the internal memory array where the `std::string` stores its raw bytes.

In C++98, there is a strict and important difference between `.data()` and `.c_str()`:

### The Null-Terminator Guarantee

- `.c_str()` guarantees that the returned character array will end with a `\0` (null terminator). In C++98, if the string did not naturally have a null terminator at the end of its memory block, calling `.c_str()` could force the string to reallocate its memory just to append that `\0`.

- `.data()` makes no such guarantee. It simply hands you a pointer to the raw block of bytes exactly as they exist in memory, without adding anything. (Note: Since C++11, both methods were changed to behave identically and both guarantee a null terminator, but in C++98, they are distinct).

### Why use `.data()` for the HTTP Body?

When you are dealing with an HTTP body, you might be receiving a JPEG image, a PDF, or a compiled binary file.

**1. It is Semantically Correct:** You are not dealing with a readable text string; you are dealing with a raw chunk of binary data. `.data()` communicates to anyone reading your code that you are working with raw memory bytes.

**2. You Already Know the Size:** System calls like `recv()` and memory copy functions (like the vector's `insert()` method you are using in `appendBody()`) do not look for null terminators. They only care about the starting pointer and the exact number of bytes to copy (`contentLen`).

**3. Performance:** By using `.data()`, you guarantee ero overhead. You bypass any unnecessary C++98 background checks for null terminators and just instantly dump the raw network bytes into your request objest.

# How can I populate a map?

In C++98, there are two primary ways to populate a `std::map`.

**1. The Bracket Operator (`[]`)**

This is the most readable and common syntax. If the key does not exist, the map creates it. If the key already exists, it overwrites the value.

```
std::map<std::string, std::string> headers;

// Populating key-value pairs
headers["Host"] = "localhost:8080";
headers["Content-Length"] = "1024";
headers["Connection"] = "keep-alive";
```

**2. The `insert()` Method**

This method is strictly for adding new elements. If the key already exists, `insert()` does nothing and leaves the old value intact. Because a map requires a key-value pair, you must wrap your variables using `std::make_pair`.

```
#include <utility> // Required for std::make_pair

std::map<std::string, std::string> headers;

headers.insert(std::make_pair("Host", "localhost:8080"));
```

# How does `std::istringstream` work?

`std::istringstream` (Input String Stream) is a C++ class that treats a standard string exactly as if it were a keyboard input stream like `std::cin` or a file stream like `std::ifstream`.

Instead of forcing you to manually search for spaces and calculate substrings, it automatically tokenizes (splits) the string based on whitespace.

`#include <sstream>`

### How the Extraction Works

When you write this line in your parser:

```
std::string method, uri, version;

std::istringstream iss("GET    /index.html    HTTP/1.1");
iss >> method >> uri >> version;
```

**Here is exactly what the `>>` (extraction) operator does step-by-step:**

**1. Skips Leading Whitespace:** It looks at the start of the string. If there are any spaces or tabs, it ignores them.

**2. Reads the First Word:** It reads characters into the `method` variable until it hits the next space. `method` becomes `"GET"`.

**3. Pauses and Repeats:** It stops there. When it sees `>> uri`, it skips the intermediate spaces, reads the next block of text into `uri` (`"/index.html"`), and stops at the next space.

**4. Finishes:** It repeats this for `version`, grabbing `"HTTP/1.1"`.

### Why is it Safer Than `find()` and `substr()`

- **It handles irregular spaces automatically:** If a buggy client sends two spaces betwee the method and the URI, manual `find(" ")` will grab an empty string and shift your entire parsing logic off by one. The string stream just skips the extra spaces transparently.

- **It has built-in bounds checking:** If the string ends unexpectedly, `istringstream` simply stops reading. It will not crash with a segmentation fault like `substr()` does when given a bad index.

### The Safety Check Explained

When you chain extraction operators, the stream evaluates its own internal state. If the string only contained `"GET /index.html"`, the stream successfully fills `method` and `uri`, but hits the end of the string before it can fill `version`.

When a stream fails to fill a requested variable, it sets an internal "fail bit." Wrapping the extraction in an `if` statement checks that fail bit. If the stream couldn't find all three required pieces of the HTTP request line, it evaluates to `false`, allowing you to safely reject the malformed request.

# Location root

### Is it ok to have a location without a root?

Yes, this is perfectly fine, particularly for your /old-page example. Because that route executes a return redirect, the server instantly sends a 301 response with the new URL to the client and never needs to look for files on the disk. Note: In NGINX, if a normal route lacks a root, it automatically inherits the root defined at the parent server block level. You might want to implement that inheritance logic later for complete NGINX parity.

# Socket Struct `struct sockaddr_in`

```
struct in_addr {
    uint32_t s_addr; // 32-bit IPv4 address
};

struct sockaddr_in {
    sa_family_t    sin_family;  // Address family (always AF_INET for IPv4)

    in_port_t      sin_port;    // 16-bit Port number (in network byte order)

    struct in_addr sin_addr;    // 32-bit IPv4 address structure

    char           sin_zero[8]; // Padding to match the size of 'struct sockaddr'
};
```

# Socket struct `struct pollfd`

```
struct pollfd {
    int   fd;         // The file descriptor to monitor
    short events;     // The events you are requesting the kernel to watch
    short revents;    // The actual events that occurred (returned by the kernel)
};
```

Field Breakdown
fd: The raw integer file descriptor (such as a master listening socket or an active client connection) that you are registering for the event loop. If you set this to a negative number, poll() safely ignores this specific array entry.  events: A bitmask where you tell the kernel exactly what to look for. In webserv, you will dynamically swap this between POLLIN (when you want to read an incoming HTTP request) and POLLOUT (when you are ready to write the HTTP response).  revents: The kernel automatically overwrites this field right before the poll() function returns to your program. It contains the result, allowing you to check if POLLIN or POLLOUT actually triggered, or if unexpected error events like POLLHUP (client disconnected unexpectedly) occurred.


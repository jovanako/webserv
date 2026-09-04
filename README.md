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
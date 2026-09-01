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
# Difference between the two `setBody()` functions

The two `setBody` functions are overloaded methods that allow the `HttpResponse` class to seamlessly accept either raw byte arrays or standard text strings for the response payload.

```
void HttpResponse::setBody(const std::vector<char>& body) {
    _body = body;
}
```

- **`setBody(const std::vector<char>& body)`:**
	* **Input:** Accepts a `std::vector<char>`, which represents raw binary or byte data. This payload format is used for compiled, compressed, or media files that cannot be interpreted as readable characters (`.png`, `.jpg`, `.gif`, `.ico`, `.pdf`, or generic binary files served as `application/octet-stream`).
	* **Mechanism:** Uses the standard C++ assignment operator (`_body = body;`). Because the input type perfectly matches the class's internal `_body` member type, it performs a direct copy of the vector.
	* **Use Case:** Ideal for binary responses, such as serving images, PDFs, or arbitrary files where the data is already held in a byte vector.

```
void HttpResponse::setBody(const std::string& body) {
    _body.assign(body.begin(), body.end());
}
```

- **`setBody(const std::string& body)`:**
	* **Input:** Accepts an `std::string`, which represents text data. This payload format is used for human-readable content, structural markup, or scripts. Supported examples in the server include HTML documents (`.html`), CSS stylesheets (`.css`), JavaScript files (`.js`), JSON payloads (`.json`), and plain text (`.txt`).
	* **Mechanism:** Uses the iterator-based `.assign()` method (`_body.assign(body.begin(), body.end());`). Since sn `std::string` cannot be directly assigned to an `std::vector<char>` using the `=` operator, this method iterates from the beginning to the end of the string, copying each character into the internal vector.
	* **Use Case:** Acts as a convenience wrapper for text-based payloads. It allows the server to pass HTML strings (like dynamically generated error pages or auto-indexes) directly into the response without forcing the caller to manually convert the string to a vector first.

	# `createResponse()`

	The `createResponse()` function converts the internal state of an `HttpResponse` object into a single sequence of raw bytes (`std::vector<char>`) so it is ready to be transmitted over the network vie the `send()` socket function.

	It constructs the final HTTP response by assembling the following components together:

	- **Status Line:** It pieces together the HTTP version (defaulting to `HTTP/1.0` if none is set), the numeric statuc code, and the corresponding status message (e.g., `HTTP/1.1 200 OK\r\n`), then appends it to the byte vector.

	- **Headers:** It iterates through the map of stored headers, formats each as `Key: Value\r\n`, and inserts them into the vector.

	- **CRLF Separator:** It inserts a mandatory empty line consisting of a carriage return and line feed (`\r\n`) to formally separate the headers from the body.

	- **Body:** Finally, it appends the actual payload (the raw bytes stored in the `_body` vector) to the end of the response vector and returns the completed package.

	# `stringstream`

	In C++, a `stringstream` is a stream class that allows you to read from and write to strings as if they were standard input/output streams (like `cin` or `cout`). It is highly useful for parsing text, formatting data, and safely converting between strings and numerical types.

	The provided codebase utilizes three specific types of string streams from the `<sstream>` library:

	- `std::istringstream` **(Input String Stream):** Used to extract or parse data *out* of a string.

		* In the server, it is used to safely split a single string containing the HTTP request line into separate `method`, `uri`, and `version` variables by ignoring whitespace.

		* It is also used to convert string representations of numbers back into integer types, such as parsing the port number from a host header or validating IPv4 octets.
	
	- `std::ostringstream` **(Output String Stream):** Used to format and insert data *into* string.

		* Because C++98 lacks a built-in `to_string()` function, the server frequently uses `ostringstream` to convert integers into strings, such as converting the `_statusCode` (e.g., `404`) or payload lengths into string format for the HTTP headers.

		* It is also used to easily concatenate text and variables when building the HTML bodies for error pages or auto-index directory listings.
	
	- `std::stringstream` **(Input/Output String Stream):** Capable of both reading and writing.

		* In the HTTP parser, it is used to decode percent-encoded URIs (e.g., `%20` for a space) by feeding it a hexadecimal string and extracting it as an integer value using the `std::hex` manipulator.
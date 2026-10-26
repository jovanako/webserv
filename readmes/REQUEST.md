# --> ParsingState

The `ParsingState` enum in `HttpRequest` acts as a state machine tracker necessary for managing the lifecycle of an incoming HTTP request in a non-blocking environment. Since the server is strictly required to remain non-blocking at all times and use `poll()` (or equivalent) for I/O operations, a single network read might not capture an entire HTTP request at once. The server needs this enum to remember exactly where the parsing process left off so it can resume correctly when the next chunk of data arrives.

Here is why `ParsingState` is specifically required in your implementation:

- **Tracking Parsing Process:** The enum defines distinct operational phases (`PARSE_REQUEST_LINE`, `PARSE_HEADERS`, `PARSE_BODY`, `PARSE_DONE`, `PARSE_ERROR`) that allow the request to be constructed incrementally as data streams in. For example, in `Client::parseHeaders()`, the state is explicitly updated to `HttpRequest::PARSE_HEADERS` once the initial request line (Method, URI, Version) is successfully processed.

- **Immediate Error Flagging:** If validation fails at any stage, the state is immediately switched to `PARSE_ERROR` to halt further processing. For instance, if `setUri()` detects a path traversal attempt (like `/.../`) or an unsupported HTTP version is passed to `setVersion()`, the state is changed to `PARSE_ERROR` alongside an appropriate error code like 403 or 505.

- **Triggering Error Responses:** The `Client` class actively monitors this state during the parsing phase. If it detects that `_request.getRequestState() == HttpRequest::PARSE_ERROR`, it immediately stops extracting data, sets the response status code to the trapped error, and finalizes the response to send back to the client.

# --> `std::vector<char> _body`

The `_body` variable is defined as an `std::vector<char>` primarily to safely handle raw binary data.

- **Binary File Uploads:** The project requirements mandate that the server must support file uploads. Uploaded files (like images, executables, or archives) contain arbitrary binary data, including null bytes (`\0`). Using `std::vector<char>` guarantees that these raw bytes are stored exactly as received without the risk of null-termination truncation that can sometimes occur when handling C-strings.

- **Efficient Buffer Appending:** Because the server must operate in a non-blocking manner, an HTTP request body will often arrive in multiple fragmented chunks over time. `std::vector<char>` allows the `HttpRequest::appendBody()` method to easily and safely append these incoming raw network buffers using `_body.insert(_body.end(), data, data + size)`.

- **Contiguous Memory:** It provides a dynamic, contiguous block of memory. This makes it straightforward to eventually write the payload out to a file descriptor, pass it to a CGI script, or save it to a designated upload directory.

# --> Payload

In computing and networking, a **payload** is the actual, essential data being transmitted in a message, excluding the metadata or routing information needed to get it to its destination.

Think of a network request like a physical package:

- **The Headers (Metadata):** Act as the shipping label. They tell the server where the package is going, how big it is, and what kind of item is inside (e.g., `Content-Type: image/png, Content-Length: 1048576`).

- **The Payload (Body):** Is the actual contents of the box. This is the image file, the submitted login credentials, or the HTML document being transferred.

In the context of your HTTP server project, the payload is the request or response "body". For example, if a user uploads a file to your server, the raw binary data of that file is the payload, which your `HttpRequest` stores in its `_body` vector. If that payload exeeds the allowed size limits, your server rejects it with a 413 "Payload Too Large" error.

# --> Percent-Encoding

Percent-Encoding strictly represents exactly one 8-bit byte of data. Because of how hexadecimal math works, it takes exactly two hexadecimal digits to represent a full byte.

- **1 Hex Digit = 4 bits** (can hold values 0-15)
- **2 Hex Digits = 8 bits = 1 full byte** (can hold values 0-255)

If the standard allowed only one digit (like `%2`), it would only provide half a byte, making it impossible to know which of the 256 possible ASCII characters it was meant to represent. If it allowed three digits (like `%20A`), it would overflow the size of a standard character byte.

According to the official URI specification (RFC 3986), the format must always be exactly `%` followed by two valid hex digits. For example, a space is always `%20` (hex 20, decimal 32). If a client sends `%2`, it is incomplete. If a client sends `%20A`, the server interprets the `%20` as a space, and the `A` is just treated as the next normal letter in the URL.

# --> `iss >> std::hex >> value`

```
for (size_t i = 0; i < input.length(); ++i) {
	if (input[i] == '%') {
		std::string hexStr = input.substr(i + 1, 2);

		int value;
		std::stringstream iss(hexStr);
		iss >> std::hex >> value;
		
		decoded += static_cast<char>(value);
		i += 2;
	}
	...

}
```

- `iss >> std::hex >> value;`\
This is where the actual conversion happens. The `std::hex` part is a stream manipulator that instructs the stream to interpret the incoming characters as base-16 (hexadecimal) rather than standard base-10 (decimal) numbers. It reads the string `"20"`, calculates its hexadecimal value (which is 32 in decimal), and stores `32` into the `value` integer.

- `decoded += static_cast<char>(value);`\
Finally, the integer `32` is converted into a standard character. In the ASCII table, 32 corresponds to the space character (`' '`). The `static_cast<char>` ensures the compiler safely narrows the 4-byte integer into a 1-byte character without warnings. The space is then appended to the final `decoded` string.

# --> `static_cast<unsigned char>`

The cast to `unsigned char` in `isValidHeaderKey()` guarantees that the byte is evaluated as a positive integer (from 0 to 255) to ensure predictable and safe mathematical comparisons.

In C++, the standard `char` type can be either signed or unsigned depending on the compiler and underlying system architecture. If a compiler defaults to a signed `char`, it can only safely represent values from -128 to 127.

If a client sends an extended ASCII character or raw binary byte with a value of 128 or higher, a signed `char` will overflow and interpret that byte as a negative number (for example, reading it as `-106` instead of `150`).

The immediate next line in the code evaluates the bounds: `if (c <= 32 || c >= 127)`. By explicitly casting the character to an `unsigned char` first, you prevent any negative number wrap-around. A byte with a value of 150 will strictly be evaluated as the positive integer 150, which cleanly and safely triggers the `c >= 127` rejection condition.

# --> `isValidIpv4()`

- `std::istringstream ss(host);`\
Creates a string stream from the `host` string, allowing the code to easily parse it like an input stream.

- `std::string segment;`\
Declares a temporary string to hold each individual section (octet) of the IP address between the dots.

- `while (std::getline(ss, segment, '.')) {`\
Starts a loop that reads from the string stream up to the next `.` character, storing that chunk in `segment`.

- `if (segment.empty() || segment.length() > 3)`\
Checks if the chunk is missing (which happens if there are consecutive dots like `192..168`) or if it is longer than 3 characters (since the max value `255` is only 3 digits).

- `std::istringstream numStream(segment);`\
Creates a new string stream specifically for the single segment (e.g.`"192"`) to convert it into an integer.

- `numStream >> val`\
Extracts the numerical value from the string stream into `val`.

- `if (segment.length() > 1 && segment[0] == '0')`\
Rejects leading zeroes because they can be dangerously misinterpreted by some systems as octal (base-8) numbers rather than decimal.

# --> Domain

### Examples ov Valid Domains

- `localhost` (single label, common for local testing)
- `webserv.com` (standard domain)
- `my-custom-site.org` (hyphens are allowed in the middle of a label)
- `api.v1.backend.net` (multiple subdomains)
- `42network.org` (numbers are allowed)

### Examples of Invalid Domains

- `-webserv.com` (labels cannot start with a hyphen)
- `webserv.com-` (labels cannot end with a hyphen)
- `my web.com` (spaces are forbidden)
- `webserv_site.com` (underscores are forbidden in standard hostnames)
- `999.1.2.3` (all-numeric final label, which makes it an invalid IP rather than a valid domain)

# --> `std::istringstream` vs. `std::ostringstream`

Use `std::istringstream` when you need to read from or parse an existing string, and use `std::ostringstream` when you need to write to or construct a new string.

## `std::istringstream` (Input String Stream)

- **Purpose:** Extracts structured data (like integers, floats, or individual words) out of a raw string. It behaves like `std::cin`, but reads from a string variable in memory rather than user input.

- **Best For:** Splitting sentences by spaces, tokenizing data, or safely converting a string into a numeric type.

## `std::ostringstream` (Output String Stream)

- **Purpose:** Assembles various data types (strings, integers, characters) into a single formatted string. It behaves like `std::cout`, but writes to a string in memory rather than printing to the console.

- **Best For:** Generating complex text outputs (like HTML or JSON), dynamically building file paths, or converting numbers into strings (which is especially necessary in C++98 where `std::to_string` is unavailable).

# --> `>>` vs. `<<`

- **Use `>>` (Extraction/Input) to read data.**\
The arrows point *towards* your variables. Data flows out of the stream (like `std::cin` or `istringstream`) and into your code.\
*Example:* `iss >> myVariable;` (Data moves from `iss` into `myVariable`).

- **Use `<<` (Insertion/Output) to write data.**\
The arrows point *towards* the stream. Data flows out of your variables or raw text and into the stream (like `std::cout` or `ostringstream`).\
*Example:* `oss << "Error: " << errorCode;` (Data moves from the text and the variable into `oss`).

# Current `HttpRequest` Function Map

This section describes the current implementation and the responsibility of each `HttpRequest` function.

## Object lifecycle

### `HttpRequest()`

Creates an empty request. It starts in `PARSE_REQUEST_LINE`, with zero content length, no error code, and chunked transfer disabled.

### Copy constructor and assignment operator

Copy all request state, including the method, URI, query string, version, headers, body, content length, error code, and chunked flag.

### Destructor

Releases the request object. The class uses standard containers, so they clean up their own storage.

## State and value setters

### `setRequestState(ParsingState)`

Changes the parser state. The states describe whether the request line, headers, or body are being parsed, or whether parsing is complete or failed.

### `setErrorCode(int)`

Stores the HTTP error code associated with a parsing failure.

### `setMethod(const std::string&)`

Validates and stores the request method. Every character must be uppercase, and only `GET`, `POST`, and `DELETE` are supported. Invalid characters produce `400`; unsupported methods produce `501` and set `PARSE_ERROR`.

### `setUri(const std::string&)`

Coordinates URI validation and storage:

1. `validateUriStructure()` checks the leading slash, maximum length, and traversal patterns.
2. `hasValidUriCharacters()` checks printable characters and percent-encoding.
3. `storeUriParts()` separates and decodes the path and query string.

### `setVersion(const std::string&)`

Accepts only `HTTP/1.0` and `HTTP/1.1`. Other versions set error `505` and move the request to `PARSE_ERROR`.

### `setContentLength(size_t)`

Stores the parsed `Content-Length` value.

## Request accessors

The getter functions expose parsed request data without changing it:

- `getRequestState()` returns the parser state.
- `getErrorCode()` returns the current parsing error.
- `isChunked()` reports whether `Transfer-Encoding: chunked` was received.
- `getMethod()` returns the method.
- `getUri()` returns the decoded path.
- `getQueryString()` returns the raw query string.
- `getVersion()` returns the HTTP version.
- `getHeaders()` returns the normalized header map.
- `getBody()` returns the raw request body.
- `getContentLength()` returns the parsed body length.

## URI helpers

### `validateUriStructure(const std::string&)`

Returns `0` for a structurally valid URI. It returns `400` when the URI is empty or does not begin with `/`, `414` when it exceeds `MAX_URI_LENGTH`, and `403` when it contains blocked parent-directory traversal patterns.

### `hasValidUriCharacters(const std::string&)`

Rejects control characters, DEL, and invalid percent sequences. A percent sign must be followed by two hexadecimal digits.

### `storeUriParts(const std::string&)`

Splits the URI at `?`, decodes the path using percent-decoding, and stores the decoded path in `_uri` and the raw query string in `_queryString`.

## Header handling

### `addHeader(const std::string&, const std::string&)`

Validates, normalizes, and stores one header:

1. Validates the header-name characters.
2. Converts the name to lowercase.
3. Rejects duplicate `Host`, `Content-Length`, and `Transfer-Encoding` headers.
4. Validates the `Host` value.
5. Validates transfer encoding or content length when applicable.
6. Stores the normalized key and original value.

### `handleError(HttpRequest&, int)`

Stores an error code and changes the parser state to `PARSE_ERROR`.

### `isValidHeaderKey(const std::string&)`

Checks that a header name is non-empty, printable ASCII, and contains no HTTP separator characters.

### `toLowerCopy(const std::string&)`

Returns a lowercase copy of a header name or value.

### `isDuplicateRestrictedHeader(const std::string&, headers)`

Checks whether `Host`, `Content-Length`, or `Transfer-Encoding` already exists in the header map.

### `validateTransferEncoding(const std::string&)`

Rejects `Transfer-Encoding` when `Content-Length` already exists, accepts only `chunked`, and sets `_isChunked` when valid. It returns `400`, `501`, or `0`.

### `validateContentLength(const std::string&)`

Rejects conflicts with `Transfer-Encoding`, requires a decimal value, parses the length, and stores it. It returns `400` or `0`.

## Host validation helpers

### `isValidHost(const std::string&)`

Validates a complete HTTP `Host` header value by separating its optional port and checking the hostname.

### `splitHostAndPort(const std::string&, std::string&)`

Splits a hostname from an optional `:port`. A missing port is allowed; a present port must pass `isValidPort()`.

### `isValidPort(const std::string&)`

Requires one to five decimal digits and accepts only ports from `1` through `65535`.

### `isValidIpv4(const std::string&)`

Splits an IPv4 address into four dot-separated segments and validates every segment with `isValidIpv4Segment()`.

### `isValidIpv4Segment(const std::string&)`

Validates one IPv4 segment: one to three digits, numeric range `0-255`, and no leading zeroes except for the value `0`.

### `isValidDomain(const std::string&)`

Splits a hostname into labels, checks the total length, and validates each label with `isValidDomainLabel()`.

### `isValidDomainLabel(const std::string&, bool)`

Validates label length, allowed characters, and hyphen placement. When the label is the top-level label, it must contain at least one letter.

## Body and path handling

### `appendBody(const char*, size_t)`

Appends raw bytes to `_body`. It accepts binary data and does nothing for a null pointer or zero length.

### `makeFullPath(const LocationConfig&)`

Combines the matched location's root with the request URI after removing the location prefix. It also normalizes the slash between the root and remaining URI path.
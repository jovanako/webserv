# ParsingState

The `ParsingState` enum in `HttpRequest` acts as a state machine tracker necessary for managing the lifecycle of an incoming HTTP request in a non-blocking environment. Since the server is strictly required to remain non-blocking at all times and use `poll()` (or equivalent) for I/O operations, a single network read might not capture an entire HTTP request at once. The server needs this enum to remember exactly where the parsing process left off so it can resume correctly when the next chunk of data arrives.

Here is why `ParsingState` is specifically required in your implementation:

- **Tracking Parsing Process:** The enum defines distinct operational phases (`PARSE_REQUEST_LINE`, `PARSE_HEADERS`, `PARSE_BODY`, `PARSE_DONE`, `PARSE_ERROR`) that allow the request to be constructed incrementally as data streams in. For example, in `Client::parseHeaders()`, the state is explicitly updated to `HttpRequest::PARSE_HEADERS` once the initial request line (Method, URI, Version) is successfully processed.

- **Immediate Error Flagging:** If validation fails at any stage, the state is immediately switched to `PARSE_ERROR` to halt further processing. For instance, if `setUri()` detects a path traversal attempt (like `/.../`) or an unsupported HTTP version is passed to `setVersion()`, the state is changed to `PARSE_ERROR` alongside an appropriate error code like 403 or 505.

- **Triggering Error Responses:** The `Client` class actively monitors this state during the parsing phase. If it detects that `_request.getRequestState() == HttpRequest::PARSE_ERROR`, it immediately stops extracting data, sets the response status code to the trapped error, and finalizes the response to send back to the client.

# `std::vector<char> _body`

The `_body` variable is defined as an `std::vector<char>` primarily to safely handle raw binary data.

- **Binary File Uploads:** The project requirements mandate that the server must support file uploads. Uploaded files (like images, executables, or archives) contain arbitrary binary data, including null bytes (`\0`). Using `std::vector<char>` guarantees that these raw bytes are stored exactly as received without the risk of null-termination truncation that can sometimes occur when handling C-strings.

- **Efficient Buffer Appending:** Because the server must operate in a non-blocking manner, an HTTP request body will often arrive in multiple fragmented chunks over time. `std::vector<char>` allows the `HttpRequest::appendBody()` method to easily and safely append these incoming raw network buffers using `_body.insert(_body.end(), data, data + size)`.

- **Contiguous Memory:** It provides a dynamic, contiguous block of memory. This makes it straightforward to eventually write the payload out to a file descriptor, pass it to a CGI script, or save it to a designated upload directory.

# Payload

In computing and networking, a **payload** is the actual, essential data being transmitted in a message, excluding the metadata or routing information needed to get it to its destination.

Think of a network request like a physical package:

- **The Headers (Metadata):** Act as the shipping label. They tell the server where the package is going, how big it is, and what kind of item is inside (e.g., `Content-Type: image/png, Content-Length: 1048576`).

- **The Payload (Body):** Is the actual contents of the box. This is the image file, the submitted login credentials, or the HTML document being transferred.

In the context of your HTTP server project, the payload is the request or response "body". For example, if a user uploads a file to your server, the raw binary data of that file is the payload, which your `HttpRequest` stores in its `_body` vector. If that payload exeeds the allowed size limits, your server rejects it with a 413 "Payload Too Large" error.
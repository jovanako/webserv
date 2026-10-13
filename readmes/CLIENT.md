# What a Chunked Body Looks Like

Chunked transfer encoding is defined by the HTTP/1.1 specification (RFC 7230). It allows a server or client to send data in a series of "chunks" without needing to know the total `Content-Length` upfront.

A chunked body looks like this over the network:

```
7\r\n			<-- Chunk size in Hexadecimal (7 bytes) followed by CRLF
Mozilla\r\n		<-- The actual data (7 bytes) followed by CRLF
9\r\n			<-- Chunk size in Hexadecimal (9 bytes)
Developer\r\n	<-- The actual data (9 bytes)
0\r\n			<-- A chunk size of 0 indicates the end of the body
\r\n			<-- A final empty line (CRLF) to terminate the request
```

## parseChunkedBody

**1. The Continuous parsing loop**  
`while (true)` loops continuously to process as many complete chunks as are currently stored in `_readBuffer`.

**2. Finding the chunk size boundary**  
`find("\r\n")` searches for the first `\r\n` (CRLF), which marks the end of the hexadecimal size line. If it's missing, the size hasn't fully arrived yet, so the function yields back to the main `poll()` loop to wait for more data.

**3. Extracting the size and ignoring extensions**  
`hexSize = _readBuffer.substr(0, pos);` extracts everything up to the `\r\n`. The HTTP spec allows optional "chunk extensions" separated by a semicolon (e.g., `7;name=value\r\n`). 

```
size_t semiPos = hexSize.find(';');
if (semiPos != std::string::npos) {
    hexSize = hexSize.substr(0, semiPos);
}
```

This safely strips them out so you only parse the number.

**4. Converting Hexadecimal to Integer**  
Feeds the hex string into a stringstream and extracts it as an integer (`chunkSize`). If the string contains invalid characters, it triggers a 400 Bad Request.

**5. Handling the terminating chunk**  
`if (chunkSize == 0)` -> A chunk size of `0` means the body is finished. However, HTTP requires a final trailing `\r\n` after the `0\r\n`. It checks if all 4 bytes (`\r\n\r\n`) are present. If they are, it cleans the buffer and moves the client to the `PROCESSING` state so the server can generate a response.

**6. Ensuring the full chunk data has arrived**  
`if (_readBuffer.length() < pos + 2 + chunkSize + 2) return;` -> It calculates the exact bytes needed: the length of the size string (`pos`) + its CRLF (`2`) + the actual data (`chunkSize`) + the trailing CRLF (`2`). If the buffer is shorter than this, the chunk was split across multiple TCP packets. The function returns to wait for the rest.

**7. Appending data and advancing the buffer**

`_request.appendBody();`  
`_readBuffer.erase();`  
Copies exactly `chunkSize` bytes of raw data into your request body object. It then erases the entire processed chunk from `_readBuffer`. The `while (true)` loop then restarts to check if a second chunk is sitting in the remaining buffer.

> **Why it has to be done this way:** Because your server uses a non-blocking `poll()` architecture, you cannot use a simple blocking `read()` to wait for the rest of a chunk. If a client sends "7\r\nMoz" and pauses, a blocking server would freeze completely while waiting for "illa\r\n".

> Instead, this code acts as a **state machine**. It aggressively checks if it has enough data to move forward. If it doesn't, it immediately hits a `return;`. This hands control back to `ServerManager::run()` so the server can handle *other* clients' requests while waiting for this specific client's network packets to finish arriving.
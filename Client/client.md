# WebServ Client Refactoring: RequestParser Extraction

This update introduces a structural refactor to the `Client` class, breaking it down to adhere to the Single Responsibility Principle without altering the underlying application logic. The HTTP parsing mechanisms have been decoupled from the network I/O and state machine, drastically reducing the complexity of the core connection handler.

## Overview of Changes

* **New Component Created:** Introduced `RequestParser` (`RequestParser.hpp` and `RequestParser.cpp`) to handle all raw byte interpretation, HTTP header validation, and chunked body processing.
* **Data Ownership Reassigned:** `_readBuffer` and the `_request` object were moved out of `Client` and are now exclusively owned and managed by the `RequestParser`. 
* **Client Simplification:** Removed over 15 string-manipulation and parsing utility methods from the `Client` class, replacing them with a single `_parser` member variable.

## Key Improvements

* **Strict Encapsulation:** The `Client` class is now focused purely on connection management, state transitions, and routing. Any logic checking content lengths, HTTP versions, or header values now safely routes through `_parser.getRequest()` and `_parser.getReadBuffer()`.
* **Streamlined Network I/O:** Socket reading loops (`readHeaderChunk` and `readBodyChunk`) now pull data via `recv()` and immediately pass it to `_parser.appendData()`, keeping memory manipulation boundaries crystal clear.
* **Cleaner State Coordination:** Methods like `handleReadHeader()` and `handleReadBody()` no longer manually mutate HTTP request structures. They now delegate to the parser and rely on returned boolean flags to seamlessly dictate when the client's state machine should advance to `PROCESSING`.
* **HTTP Pipelining Integrity:** Corrected a critical bug in the request reset cycle. When a keep-alive connection finishes sending a response, `_parser.reset()` now successfully clears the `_request` object while leaving `_readBuffer` intact, ensuring pipelined requests arriving in the same TCP packet are not accidentally destroyed.

## Future Architecture Roadmap

This initial extraction paves the way for isolating the remaining distinct responsibilities from the `Client` class:
1. **RequestHandler:** To take over URI routing, method validation, CGI execution, and file system operations (`handleGet`, `handlePost`, `serveAutoindex`).
2. **ResponseBuilder:** To manage `HttpResponse` payload construction, MIME-type mapping, and dynamic error page generation.
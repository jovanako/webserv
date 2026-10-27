# ResponseBuilder Architecture & Design

## Overview
The `ResponseBuilder` class was introduced to decouple HTTP response generation and formatting from the connection state machine (`Client`) and the core request routing logic (`RequestHandler`). It acts as a dedicated utility for constructing both standard and error responses, managing headers, validating keep-alive rules, and calculating payload lengths.

---

## Class Responsibilities
- **Error Page Resolution**: Loads custom error pages defined in server configurations or falls back to generating clean default HTML error pages.
- **Header Finalization**: Automatically determines connection persistence (`Keep-Alive` vs. `Close`) and calculates `Content-Length`.
- **Stateless Design**: Operates purely on references (`HttpResponse`, `HttpRequest`, `ServerConfig`), ensuring it remains lightweight and thread/context safe without managing connection states directly.

---

## Core Public Interface (`ResponseBuilder.hpp`)

```cpp
class ResponseBuilder {
    public:
        ResponseBuilder();
        ~ResponseBuilder();
        ResponseBuilder(const ResponseBuilder& other);
        ResponseBuilder& operator=(const ResponseBuilder& other);

        // Builds custom or default error responses, applying bodies and finalizing headers
        void buildErrorResponse(int statusCode, HttpResponse& response, const ServerConfig& server, const HttpRequest& request);
        
        // Finalizes general responses (calculates Content-Length, sets Connection headers)
        void buildResponse(HttpResponse& response, const HttpRequest& request);
};
```

---

## Architectural Separation of Concerns

1. **`Client`**: 
   - Manages socket file descriptors, non-blocking I/O buffers, and connection states (`READING_HEADER`, `READING_BODY`, `WRITING_RESPONSE`, etc.).
   - Acts as a coordinator by triggering `_responseBuilder.buildErrorResponse(...)` or `_responseBuilder.buildResponse(...)` and updating its state machine to `WRITING_RESPONSE`.

2. **`RequestHandler`**: 
   - Inspects location rules, checks allowed methods, manages file stat checks, and generates directory listings (autoindex) or upload paths.
   - Returns a status code (`200`, `204`, `403`, `404`, `405`, etc.) or execution flags back to the processing flow.

3. **`ResponseBuilder`**: 
   - Takes raw response structures and fills them with proper headers, status codes, and body content before serialization.
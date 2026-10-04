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
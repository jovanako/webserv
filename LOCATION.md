# `_redirect`

The `_redirect` variable is an `std::pair<int, std::string>` because an HTTP redirection inherently requires two distinct but tightly coupled pieces of information to function correctly:

- **The Status Code (`int` / `.first`):** The numerical HTTP status code indicating the type of redirect (e.g., `301` for Moved Permanently, `302` for Found).

- **The Target URL (`std::string` / `.second`):** The new destination path or URL where the client should be sent.

# `std::map` vs `std::pair`

An `std::pair` is a simple data structure that holds exactly two linked values, while an `std::map` is a container that stores an entire collection of pairs, allowing you to look up values using unique keys.

- `std::pair`:

	* Stores exactly two heterogeneous items as a single unit, accessed via the `.first` and `.second` members.

	* It has a fixed size of two elements.

	* Example in the code: The `_redirect` variable uses an `std::pair<int, std::string>` because a specific redirect configuration always consists of exactly one status code and one destination URL.

- `std::map`:

	* Stores a dynamically sized collection of key-value pairs where every key must be unique.

	* Automatically sorts the elements by their keys and provides fast lookups (e.g., using `myMap["key"]` or `myMap.find("key")`).

	* Example in the code: The `_headers` variable uses an `std::map<std::string, std::string>` to store multiple HTTP headers. Because a single HTTP request can have dozens of headers, the map acts as a searchable dictionary, allowing the server to quickly check if a specific header like `"host"` exists. Similarly, `_errorPages` uses an `std::map<int, std::string>` to map multiple distinct integer status codes to their respective HTML file paths.
# --> `throw`

You do not need to add a `return` statement after throwing an exception.

In C++, the `throw` keyword immediately halts the normal execution flow of the function. Control is instantly transferred out of the function as the program begins stack unwinding to look for an appropriate `catch` block to handle the `std::runtime_error`.

Because execution completely stops at the `throw` statement, any code placed directly after it - including a `return` - is completely unreachable. Most modern compilers will flag a subsequent `return` statement with an "unreachable code" warning.

# --> `insert`

The `std::string::insert` function shifts existing characters to the right to make room for the new content.

For example: `line.insert(i, " ");` places a space exactly at index `i`. This forces the `{` (which was currently at index `i`) and all subsequent characters to shift one position to the right.

Because the string has grown by two characters and the special character has been shifted, the code then executes `i += 2` to skip past the newly inserted spaces and avoid analyzing the same character again on the next loop iteration.

# --> `parseSize()`

In the case we get an input of "10X" for `sizeStr`:

1. **The `if` blocks:** The `lastChar` is `X`. Because `X` does not match 'K', 'M', or 'G', all the `if` blocks are skipped.

2. **State before extraction:** The `multiplier` remains `1`, and `numPart` remains the full string `"10X"`.

3. **The Stream:** `std::istringstream iss("10X");` is created.

When we reach the evaluation `if (!(iss >> value) || !iss.eof())`, here is how it breaks down:

- `iss >> value`: The stream starts reading `"10X"`. It reads the `1`, then the `0`, and then encounters the `'X'`. Since `'X'` is not a number, the stream stops reading. However, because it successfully found a valid number (`10`) before stopping, the extraction operation evaluates to `true`.

- `!(iss >> value)`: Since the extraction was `true`, the negation makes this part `false`.

- `iss.eof()`: The stream stopped at the `'X'` and never reached the actual end of the string. Because there is still an unread `'X'` sitting in the stream, `iss.eof()` evaluates to `false`.

- `!iss.eof()`: Since `iss.eof()` is `false`, the negation makes this part `true`.

Finally, the `||` (OR) operator evaluates the two halves:  
`false || true` evaluates to `true`.

Because the combined statement is `true`, the code will enter the `if` block and successfully throw the `std::runtime_error`, protecting your server from the invalid `"10X"` input.

# --> `interface:port`

To satisfy the requirement to handle `interface:port` pairs, the parser must account for three common scenarios in a `listen` directive:

1. **Port only:** `listen 8080;` (Should default the host to `0.0.0.0`)

2. **Interface and Port:** `listen 127.0.0.1:8080;`

3. **Interface only (Optional but good practice):** `listen 127.0.0.1;` (Should default the port to `80`)

# --> Port range 1 - 65535

The port range of 1 to 65535 exists because the underlying  TCP and UDP networking protocols allocate exactly 16 bits for the port number field within their packet headers.

- **The Math:** A 16-bit unsigned integer can hold 2^16 (or 65,536) distinct values, ranging from 0 to 65535.

- **Port 0:** While 0 is technically part of the 16-bit range, it is specially reserved in network programming to act as a wildcard (telling the operating system to automatically assign any available ephemeral port). Therefore, the actual usable ports for explicitly listening or connecting range from 1 to 65535, which your `ConfigParser` and `HttpRequest` classes strictly validate.

# --> `size_t` overflow

In C++98, you can find the maximum value of a `size_t` by using `(size_t)-1` (or by including `<limits>`)

# --> `value > maxSize / multiplier

This condition is a standard mathematical trick used in C++ to prevent integer overflow *before* it actually happens.

Here is exactly how and why it works:

- **The Goal:** We want to know if multiplying our parsed number by the unit multiplier (e.g., `value * multiplier`) will result in a number larger than what a `size_t` variable can hold (`maxSize`).

- **The Trap:** If you simply write `if (value * multiplier > maxSize)`, the C++ compiler performs the multiplication *first*. If the result is larger than `maxSize`, the integer silently overflows and wraps around to a very small incorrect number. By the time the `>` comparison evaluates, it is comparing that tiny wrapped-around number against `maxSize`, meaning the overflow check completely fails.

- **The Algebraic Fix:** If we take the mathematical inequality `value * multiplier > maxSize` and divide both sides by `multiplier`, we get `value > maxSize / multiplier`.

By dividing instead of multiplying, the numbers get smaller. Calculating `maxSize / multiplier` is completely safe and impossible to overflow. This allows the program to safly look ahead and determine if the multiplication *would* be too big, without ever actually executing the dangerous multiplication step.

# --> `(size_t)-1`

`size_t` is an **unsigned** integer. It cannot hold negative numbers; it only undesrstands zero and positive numbers.

When you try to force a negative number like `-1` into an unsigned variable, the computer handles it by "wrapping around".

What happens:

1. You start with `0`.

2. You subtract `1` (which is what casting `-1` to an unsigned type effectively does).

3. Because the variable can't go below zero, it rolls backward past zero and lands on the absolute highest number that variable's memory can physically hold.

# Current `ConfigParser` Function Map

This section describes the current implementation and the responsibility of each parser function.

## Object lifecycle

### Constructors, assignment, and destructor

The constructors initialize the configuration path and token index. The copy constructor and assignment operator copy the path, token list, current token position, and parsed server list. The destructor releases the parser object; its standard containers clean up their own storage.

## Tokenization

### `tokenize()`

Opens the configured file and processes it line by line. Each line is passed through comment removal, punctuation isolation, and token collection before its tokens are appended to `_tokens`.

### `removeComment(std::string&)`

Finds the first `#` and removes it and everything after it. Configuration comments therefore never reach the parser.

### `isolatePunctuation(std::string&)`

Adds spaces around `{`, `}`, and `;` so these syntax characters become independent tokens even when attached to another word.

### `appendLineTokens(const std::string&)`

Uses an input string stream to split a processed line on whitespace and append each word to `_tokens`.

### `getNextToken()`

Returns the next token and advances `_currentTokenIndex`. It throws when the parser reaches the end of the token list unexpectedly.

## Server parsing

### `parse()`

Starts tokenization, resets the token index, and requires the top-level input to contain one or more `server` blocks. It returns the completed server configurations and rejects an empty configuration.

### `parseServerBlock()`

Creates one `ServerConfig`, verifies its opening `{`, repeatedly dispatches directives, verifies the closing `}`, and stores the completed server.

### `parseServerDirective(ServerConfig&, directive)`

Dispatches a server directive to `listen`, `server_name`, `client_max_body_size`, `error_page`, or `location` parsing. Unknown directives produce a configuration error.

### `parseListenDirective(ServerConfig&)`

Reads a listen token, obtains its host and port with `parseListenTarget()`, validates the port with `parseListenPort()`, stores the values in the server, and consumes the terminating semicolon.

### `parseListenTarget(token, host, port)`

Interprets the listen forms `port`, `host:port`, and host-only input. It defaults the host to `0.0.0.0` and an IP-only target's port to `80`.

### `parseListenPort(const std::string&)`

Requires a non-empty decimal port and checks that it is within `1` through `65535`.

### `parseServerNames(ServerConfig&)`

Requires at least one server name, collects names until `;`, adds them to the server, and consumes the terminator.

### `parseClientMaxBodySize(ServerConfig&)`

Reads a size value, parses it with `parseSize()`, stores the result in the server, and verifies `;`.

## Error pages

### `parseErrorPage(ServerConfig&)`

Collects the directive arguments, identifies the final argument as the error-page path, and registers all preceding status codes.

### `getTokensUntilSemicolon()`

Reads and returns tokens until it encounters `;`. It is used by directives with a variable number of arguments.

### `getErrorPagePath(args)`

Requires at least one status code and one path, then returns the final argument as the error-page path.

### `addErrorPageCodes(ServerConfig&, args, errorPath)`

Validates that every argument before the path contains only digits, converts each code, and maps it to the selected error page.

## Location parsing

### `parseLocationBlock(ServerConfig&)`

Reads a location path, verifies the opening and closing braces, dispatches location directives, adds a default `index.html` when no index was provided, and stores the completed location in the server.

### `parseLocationRoot(LocationConfig&)`

Reads and stores the location root path, then verifies `;`.

### `parseLocationIndex(LocationConfig&, indexPresent)`

Requires at least one index file, collects all index names until `;`, stores them, and marks that an explicit index was provided.

### `parseLocationMethods(LocationConfig&)`

Requires at least one method, accepts only `GET`, `POST`, and `DELETE`, stores the allowed methods, and verifies `;`.

### `parseLocationAutoindex(LocationConfig&)`

Accepts only `on` or `off`, updates the location's autoindex flag, and verifies `;`.

### `parseLocationUploadStore(LocationConfig&)`

Reads and stores the upload directory, then verifies `;`.

### `parseLocationCgi(LocationConfig&)`

Requires a CGI extension and executable path, stores the mapping, and verifies `;`.

### `parseLocationReturn(LocationConfig&)`

Validates the redirect status code, reads the redirect URL, stores the redirect configuration, and verifies `;`.

## Shared parsing helpers

### `verifyToken(expected)`

Checks that the next token matches the expected syntax token, such as `{`, `}`, or `;`, then advances the token index. It throws a configuration error on mismatch or end of input.

### `parseSize(sizeStr)`

Coordinates size parsing: it extracts a unit, parses the numeric portion, checks multiplication overflow, and returns the final byte count.

### `parseSizeUnit(sizeStr, numPart)`

Recognizes optional `K`, `M`, and `G` suffixes, writes the numeric part to `numPart`, and returns the corresponding multiplier.

### `parseSizeNumber(numPart, originalSize)`

Parses the numeric portion with an input string stream and rejects trailing or invalid characters.

### `multiplySizeValue(value, multiplier, originalSize)`

Checks `value > maxSize / multiplier` before multiplying, preventing `size_t` overflow. It throws a configuration error when the result would be too large.
# `throw`

You do not need to add a `return` statement after throwing an exception.

In C++, the `throw` keyword immediately halts the normal execution flow of the function. Control is instantly transferred out of the function as the program begins stack unwinding to look for an appropriate `catch` block to handle the `std::runtime_error`.

Because execution completely stops at the `throw` statement, any code placed directly after it - including a `return` - is completely unreachable. Most modern compilers will flag a subsequent `return` statement with an "unreachable code" warning.

# `insert`

The `std::string::insert` function shifts existing characters to the right to make room for the new content.

For example: `line.insert(i, " ");` places a space exactly at index `i`. This forces the `{` (which was currently at index `i`) and all subsequent characters to shift one position to the right.

Because the string has grown by two characters and the special character has been shifted, the code then executes `i += 2` to skip past the newly inserted spaces and avoid analyzing the same character again on the next loop iteration.

# `parseSize()`

In the case we get an input of "10X" for `sizeStr`:

1. **The `if` blocks:** The `lastChar` is `X`. Because `X` does not match 'K', 'M', or 'G', all the `if` blocks are skipped.

2. **State before extraction:** The `multiplier` remains `1`, and `numPart` remains the full string `"10X"`.

3. **The Stream:** `std::istringstream iss("10X");` is created.

When we reach the evaluation `if (!(iss >> value) || !iss.eof())`, here is how it breaks down:

- `iss >> value`: The stream starts reading `"10X"`. It reads the `1`, then the `0`, and then encounters the `'X'`. Since `'X'` is not a number, the stream stops reading. However, because it successfully found a valid number (`10`) before stopping, the extraction operation evaluates to `true`.

- `!(iss >> value)`: Since the extraction was `true`, the negation makes this part `false`.

- `iss.eof()`: The stream stopped at the `'X'` and never reached the actual end of the string. Because there is still an unread `'X'` sitting in the stream, `iss.eof()` evaluates to `false`.

- `!iss.eof()`: Since `iss.eof()` is `false`, the negation makes this part `true`.

Finally, the `||` (OR) operator evaluates the two halves:\
`false || true` evaluates to `true`.

Because the combined statement is `true`, the code will enter the `if` block and successfully throw the `std::runtime_error`, protecting your server from the invalid `"10X"` input.

# `interface:port`

To satisfy the requirement to handle `interface:port` pairs, the parser must account for three common scenarios in a `listen` directive:

1. **Port only:** `listen 8080;` (Should default the host to `0.0.0.0`)

2. **Interface and Port:** `listen 127.0.0.1:8080;`

3. **Interface only (Optional but good practice):** `listen 127.0.0.1;` (Should default the port to `80`)

# `size_t` overflow

In C++98, you can find the maximum value of a `size_t` by using `(size_t)-1` (or by including `<limits>`)

# `value > maxSize / multiplier
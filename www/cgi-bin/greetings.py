#!/usr/bin/env python3
import html
import os
import sys
import urllib.parse

content_length_str = os.environ.get("CONTENT_LENGTH", "")
try:
    content_length = int(content_length_str) if content_length_str else 0
except ValueError:
    content_length = 0

raw_body = sys.stdin.read(content_length) if content_length > 0 else sys.stdin.read()
params = urllib.parse.parse_qs(raw_body, keep_blank_values=True)
name = html.escape(params.get("name", [""])[0])

body = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Greetings CGI</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="container">
        <h1>Greetings, {name}</h1>
        <p><a href="/cgi.html">&larr; Back to CGI Demo</a> | <a href="/index.html">Home</a></p>
    </div>
</body>
</html>
"""

sys.stdout.write("Content-Type: text/html; charset=utf-8\r\n")
sys.stdout.write(f"Content-Length: {len(body.encode('utf-8'))}\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)


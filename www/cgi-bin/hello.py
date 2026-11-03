#!/usr/bin/env python3
import html
import os
import sys
import urllib.parse

query_string = os.environ.get("QUERY_STRING", "")
params = urllib.parse.parse_qs(query_string, keep_blank_values=True)
name = html.escape(params.get("name", [""])[0])

body = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Hello CGI</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="container">
        <h1>Hello, {name}</h1>
        <p><a href="/index.html">&larr; Back to Home</a></p>
    </div>
</body>
</html>
"""

sys.stdout.write("Content-Type: text/html; charset=utf-8\r\n")
sys.stdout.write(f"Content-Length: {len(body.encode('utf-8'))}\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)

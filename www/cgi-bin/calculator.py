#!/usr/bin/python3
import os
from urllib.parse import parse_qs

def main():
    query_string = os.environ.get("QUERY_STRING", "")
    params = parse_qs(query_string)

    try:
        a = int(params.get("a", [0])[0])
        b = int(params.get("b", [0])[0])
        result = a + b
    except ValueError:
        result = "Error: Invalid numbers"

    print("Content-Type: text/html\r\n\r\n")
    print("<html>")
    print("<head><title>Sum Calculator</title></head>")
    print("<body>")
    print(f"<h1>Sum Calculator</h1>")
    print(f"<p>{a} + {b} = {result}</p>")
    print("</body>")
    print("</html>")

if __name__ == "__main__":
    main()

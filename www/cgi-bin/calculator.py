#!/usr/bin/python3
import os
import sys
from urllib.parse import parse_qs

def main():
    # 1. Logic for processing parameters
    query_string = os.environ.get("QUERY_STRING", "")
    params = parse_qs(query_string)

    status = "200 OK"
    try:
        a = int(params.get("a", [0])[0])
        b = int(params.get("b", [0])[0])
        body = str(a + b)
    except ValueError:
        body = "Error: Invalid numbers"
        status = "400 Bad Request"

    # 2. Print Headers using \r\n (HTTP standard)
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: text/plain\r\n")
    # len() on a string is safe here since result is numeric or ASCII
    sys.stdout.write(f"Content-Length: {len(body)}\r\n")

    # 3. THE MANDATORY BLANK LINE
    sys.stdout.write("\r\n")

    # 4. The Body (Result only)
    sys.stdout.write(body)
    sys.stdout.flush()

if __name__ == "__main__":
    main()
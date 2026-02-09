#!/usr/bin/python3
import os
import sys
from urllib.parse import parse_qs

def main():
    query_string = os.environ.get("QUERY_STRING", "")
    params = parse_qs(query_string)

    status = "200 OK"
    try:
        a = int(params.get("a", 0))
        b = int(params.get("b", 0))
        body = str(a + b)
    except ValueError:
        body = "Error: Invalid numbers"
        status = "400 Bad Request"

    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: text/plain\r\n")
    sys.stdout.write(f"Content-Length: {len(body)}\r\n\r\n")

    sys.stdout.write(body)
    sys.stdout.flush()

if __name__ == "__main__":
    main()

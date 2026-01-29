import os
import sys

print("Status: 200 OK")
print("Content-Type: text/plain\r\n\r\n", end='')
print("Hello from CGI!\n")

if "QUERY_STRING" in os.environ:
    print(f"Query: {os.environ['QUERY_STRING']}")

if "CONTENT_LENGTH" in os.environ:
    try:
        length = int(os.environ["CONTENT_LENGTH"])
        if length > 0:
            body = sys.stdin.read(length)
            print(f"Body: {body}")
    except ValueError:
        pass
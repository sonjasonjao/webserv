import sys

body = "Hello from CGI!\n"
sys.stdout.write("Status: 200 OK\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write(f"Content-Length: {len(body)}\r\n\r\n")
sys.stdout.write(body)
sys.stdout.flush()

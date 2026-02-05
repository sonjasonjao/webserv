import sys

# Define the body
body = "Hello from CGI!\n"

# 1. Status and Headers (Use \r\n for HTTP standard)
sys.stdout.write("Status: 200 OK\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write(f"Content-Length: {len(body)}\r\n")

# 2. THE BLANK LINE (End of headers)
sys.stdout.write("\r\n")

# 3. The Body
sys.stdout.write(body)
sys.stdout.flush()
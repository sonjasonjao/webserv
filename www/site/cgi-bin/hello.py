#!/usr/bin/python3
import datetime
import os

# The HTML Content
print(f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Python CGI Test</title>
    <style>
        body {{ font-family: sans-serif; background-color: #f4f4f9; padding: 2rem; }}
        .card {{ background: white; padding: 1.5rem; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }}
        h1 {{ color: #007bff; }}
        code {{ background: #eee; padding: 2px 5px; border-radius: 4px; }}
    </style>
</head>
<body>
    <div class="card">
        <h1>🐍 Hello from Python CGI!</h1>
        <p>This page was generated dynamically by a Python script running inside your Nginx Docker container.</p>
        
        <ul>
            <li><strong>Server Time:</strong> {datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}</li>
            <li><strong>Requested URL:</strong> {os.environ.get('REQUEST_URI', 'N/A')}</li>
            <li><strong>User Agent:</strong> {os.environ.get('HTTP_USER_AGENT', 'Unknown')}</li>
        </ul>

        <p><a href="/">← Back to Home</a></p>
    </div>
</body>
</html>
""")
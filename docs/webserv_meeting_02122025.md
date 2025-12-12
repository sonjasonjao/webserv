# Webserv meeting 2.12.2025
- Combine separate parts and see what our server is able to produce at the moment
	- Initially ok to use responses that are placeholders
	- Start with 200 OK, 404 doesn't exist and 403 bad request
	- Also ok to send an untrue response for now
- Setup a server with a default content page, and an easy hardcoded root file structure
	- Always read file, or keep in memory regardless of size?
- listen -> request -> response -> send -> clean up fds

- Have to research more about linking files to hosts, proxies
	- Where to get static web pages from (what is the absolute/relative path?)
	- Where to upload files (same folder for all, or different for different server instances?)
	- Where to load error pages from (same for all, modifiable in the config?)

- CTRL-D to close the program?

# What are we lacking currently?
- CGI
	- Create first something.cgi script, prototype forking, closing resources, running script with inputs and getting output back into the main process
- Correct responses for incoming requests
	- Link request and response interfaces
	- Analyse request data, decide resource/error page
	- Create minimal error page getter function for the different pages (placeholder)
- Chunked resource handling
	- First make single requests work, then worry about this
- File uploads
	- How does this even work?

- Sonja away next week Thursday and Friday
---
#hive

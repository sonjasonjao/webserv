# Goals
## Test driven development approach (TDD)
- Test cases
	- Come up with a test that applies all possible combinations of inputs to a function
		- Empty, NULL, valid, invalid, overflow, underflow, empty string etc. etc.
## Alternative
Fast base behavior, establish basic function, make bomb safe after

- Look into GitHub testing
# Timeline
## November
- Some more research
- Getting the hang of things
- Start writing tickets in the GitHub project
- Starting work on other features than the base case
## December
- Johnny away from 23.-28.12.
- Get really rolling
- Get HTTP parsing and validation working to a degree
- Create basic HTTP pages for errors -> load and return pages
- Send simple, different HTTP responses to clients
- CGI doing something
- Start writing test software, use siege, htop
## January
- First half: test, refine and fix, hopefully all features locked
- Second half: refine, test more, get ready for evaluation
- Evaluate -> pass!
# Weekly meeting
- Review of progress
- Discussion
	- Changed goals
	- Concerns
	- Need help
	- etc.
- Decide meeting for next week during meeting
# Division of labor
## Configuration file parsing, server setup
- Check subject and nginx files for ideas
- Create data structure with key - value pairs for most important fields
	- Where to save settings map/struct?
	- host, port, \_maxRequestSize etc.
	- Folders, redirections
## Main loop
- Listen to socket, poll/epoll/etc. all active sockets
- Accepting new connections
	- If full ignore/drop
- Monitor state of active connections
	- Read or write
		- Partial or in one go
- Prune inactive connections
- Terminate completed interactions
- Activate CGI or other functionality, decide behavior
	- Queue for responses?
- MVP soon (ASAP pls)
## Saving files
- Where to upload?
	- Define folder in config file?
		- Invalidate ../../../../ and other not so great folders
## Validation
- Incorrect fields, missing, bad header, bad timestamp, empty, no eof, http version...
## Exception handling
## CGI activation & return value
- Calculator for example
- More cool stuff later
## Forming the response
- Just load the correct page and send?
- Selection of correct error pages
- Redirection?
## Logger
- What is happening
	- Is it info, debug, error
- Where is it happening
- When is it happening
# Agenda for next week
- Have active tickets in the GitHub project
- Have a starting version of the main loop (not all functions, but initial structure mapped out to a degree)
- Configuration file parsing first version
- Johnny, figure out what to do!
**Next meeting date:** Tuesday 25th at 13:30

# Netcat testing

## Most simple example
```
echo -n "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8081
```

There are example files in `tests/requests` that can be read with `cat` and
then piped to `nc` like in the example above.

It is also possible to establish a Netcat connection to the server and then
send a request made up of partial requests. To access **CRLF** in the terminal,
press `CTRL + V ENTER ENTER` as a sequence, or use the `-C` flag, which makes
`ENTER` input **CRLF** without the need for an escape sequence.

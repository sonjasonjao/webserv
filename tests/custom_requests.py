import urllib.request, http.client

http.client.HTTPConnection._http_vsn = 10
http.client.HTTPConnection._http_vsn_str = 'HTTP/1.0'

url = 'http://localhost'
port = 8081
target = url + ":" + str(port)

request = urllib.request.Request(target)
request.remove_header('Connection')             # Why doesn't this work?
request.add_header('Connection', 'keep-alive')
request.add_header('Herp', 'Derp')              # This works

response = urllib.request.urlopen(request)
print(response.read().decode('utf-8'))

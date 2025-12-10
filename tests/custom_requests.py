import urllib.request

url = "http://localhost"
port = 8081
target = url + ":" + str(port)
request = urllib.request.Request(target)
request.remove_header('Connection')
request.add_header('Connection', 'keep-alive')
print(request)
response = urllib.request.urlopen(request)
print(response.read())

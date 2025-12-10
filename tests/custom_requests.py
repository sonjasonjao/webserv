import urllib.request

url = "http://localhost"
port = 8081
target = url + ":" + port
request = urllib.request.Request(target)
print(request)
response = urllib.request.urlopen(request)
print(response.read())

from requests import Request, Session

s = Session()

url = 'http://localhost:8081'
data = {}
headers = {}

req = Request('POST', url, data=data, headers=headers)
prepped = req.prepare()

prepped.body = 'Custom body'
prepped.headers['Content-Length'] = len(prepped.body)
prepped.headers['Connection'] = 'keep-alive'

resp = s.send(prepped)

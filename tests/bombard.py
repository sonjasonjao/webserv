import requests, time

while True:
    time.sleep(0.01)
    try:
        #g = requests.get("http://localhost:8081", timeout=0.1)
        p = requests.get("http://localhost:8081", timeout=0.001)
    except requests.exceptions.Timeout:
        pass
    except requests.exceptions.ConnectionError:
        pass

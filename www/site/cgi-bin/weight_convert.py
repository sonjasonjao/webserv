#!/usr/bin/env python3
import os
import sys
import urllib.parse

# Gravity factors relative to Earth
GRAVITY_FACTORS = {
    "mercury": 0.38, "venus": 0.91, "earth": 1.0,
    "mars": 0.38, "jupiter": 2.34, "saturn": 1.06,
    "uranus": 0.92, "neptune": 1.14, "pluto": 0.06, "moon": 0.16
}

def get_input_data():
    """Manual replacement for cgi.FieldStorage()"""
    method = os.environ.get("REQUEST_METHOD", "GET")
    
    if method == "POST":
        content_length = int(os.environ.get("CONTENT_LENGTH", 0))
        query_string = sys.stdin.read(content_length)
    else:
        query_string = os.environ.get("QUERY_STRING", "")
    
    # Parses 'weight=100&planet=mars' into {'weight': ['100'], 'planet': ['mars']}
    return urllib.parse.parse_qs(query_string)

def main():
    try:
        data = get_input_data()
        
        # 1. Handle Inputs
        weight_val = data.get("weight", [None])[0]
        planet_val = data.get("planet", [None])[0]

        if not weight_val or not planet_val:
            # If inputs are missing, we send a partial response that 
            # your C++ 'if(res.body.empty()...)' check will catch
            return 

        planet_name = planet_val.lower()
        earth_weight = float(weight_val)
        final_weight = earth_weight * GRAVITY_FACTORS.get(planet_name, 1.0)

        # 2. Prepare the Body
        script_dir = os.path.dirname(os.path.abspath(__file__))
        with open(os.path.join(script_dir, "result.html"), "r") as f:
            template = f.read()

        body = template.replace("{{FINAL_WEIGHT}}", f"{final_weight:.3f}")
        body = body.replace("{{PLANET_NAME}}", planet_name.upper())
        body = body.replace("{{EARTH_WEIGHT}}", f"{earth_weight:.3f}")

        # 3. Output EXACTLY what parseCgiOutput expects
        # Note: We provide Status and Content-Type so your C++ struct fills correctly
        sys.stdout.write("Status: 200 OK\r\n")
        sys.stdout.write("Content-Type: text/html\r\n")
        sys.stdout.write(f"Content-Length: {len(body)}\r\n")
        sys.stdout.write("\r\n") # The critical separator
        sys.stdout.write(body)
        sys.stdout.flush()

    except Exception as e:
        # This will trigger your ERROR_LOG("Malformed CGI output...")
        pass 

if __name__ == "__main__":
    main()
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <exception>
#include <unistd.h>

std::map<std::string, double> gravity_factors = {
    {"mercury", 0.38},
    {"venus",   0.91},
    {"earth",   1.0},
    {"mars",    0.38},
    {"jupiter", 2.34},
    {"saturn",  1.06},
    {"uranus",  0.92},
    {"neptune", 1.14},
    {"pluto",   0.06},
    {"moon",    0.16}
};

struct CGIException : public std::exception {
    private :
        std::string message;
    public:
        explicit CGIException (const std::string& msg) : message(msg) {}
        virtual const char* what() const noexcept override {
            return (message.c_str());
        }     
};

// cerating a key-value map for input data
std::map<std::string, std::string> parseQuery(std::string& query) {
    std::map<std::string, std::string> data;
    std::string pair;
    std::stringstream ss(query);
    
    while(std::getline(ss, pair, '&')) {
        size_t pos = pair.find('=');
        if(pos != std::string::npos) {
            std::string key = pair.substr(0, pos);
            std::string value = pair.substr(pos + 1);
            data[key] = value;
        }
    }
    return (data);
}

// reading data from the result template
std::string readFile(const std::string& file_path){
    std::ifstream file (file_path);
    if(!file) {
        return ("");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return (buffer.str());
}

// insert correct value to place holders 
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

int main() {
        const char* method = getenv("REQUEST_METHOD");
        std::string request_method = (method) ? method : "GET";
        std::string input_data;

        if(request_method == "POST") {
            // reading data from the STDIN pass by the handler
            const char *str_len = getenv("CONTENT_LENGTH");
            size_t len = (str_len) ? std::atoi(str_len) : 0;

            if(len > 0) {
                std::vector<char>buffer(len);
                std::cin.read(buffer.data(), len);
                input_data.assign(buffer.begin(), buffer.end());
            }
        } else {
            // reading data using QUERY_STRING for GET
            const char *query = getenv("QUERY_STRING");
            if(query) {
                input_data = query;
            }
        }

        auto formData = parseQuery(input_data);

        try {
            // check if key "weight" is exsits if read value
            if(!formData.contains("weight")) {
                throw std::invalid_argument("Incorrect argument!");
            }
            double earth_weight = std::stod(formData["weight"]);

            // check if key "planet" is exsits if read value
            if(!formData.contains("planet")) {
                throw std::invalid_argument("Incorrect argument!");
            }
            std::string planet = formData["planet"];

            // check if respective "planet" is exsits if read value
            if(!gravity_factors.contains(planet)) {
                throw std::invalid_argument("Incorrect argument!");
            }
            double factor = gravity_factors[planet];
        
            double result = earth_weight * factor;

            result = std::round(result * 1000.0) / 1000.0;  
        
            //sleep(1000);

            std::string html = readFile("result.html");

            if(html.empty()){
                std::cout << "Error : 502\n";
            }
            
            std::stringstream ss_result;
            ss_result << "Status: 200 OK\r\n";
            ss_result << "Content-Type: text/plain\r\n";
            ss_result << "Content-Length: {len(body)}\r\n"; 
            ss_result << std::fixed << std::setprecision(3) << result;
            replaceAll(html, "{{FINAL_WEIGHT}}", ss_result.str());

            replaceAll(html, "{{PLANET_NAME}}", planet);

            std::stringstream ss_earth;
            ss_earth << std::fixed << std::setprecision(3) << result;
            replaceAll(html, "{{EARTH_WEIGHT}}", ss_earth.str());
            
            // Body
            std::cout << html;

        } catch (const std::exception& e) {
            // handle error
            std::cout << "Error: 500\n";
        }

    return (EXIT_SUCCESS);
}
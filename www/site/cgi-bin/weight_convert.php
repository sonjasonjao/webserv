#!/usr/bin/php
<?php
// Gravity factors relative to Earth
$gravity_factors = [
    "mercury" => 0.38,
    "venus"   => 0.91,
    "earth"   => 1.0,
    "mars"    => 0.38,
    "jupiter" => 2.34,
    "saturn"  => 1.06,
    "uranus"  => 0.92,
    "neptune" => 1.14,
    "pluto"   => 0.06,
    "moon"    => 0.16
];

try {
    // PHP automatically parses both GET and POST into $_REQUEST
    $weight_val = $_REQUEST['weight'] ?? null;
    $planet_val = $_REQUEST['planet'] ?? null;

    if (!$weight_val || !$planet_val) {
        throw new Exception("Missing parameters");
    }

    $planet_name = strtolower($planet_val);
    if (!isset($gravity_factors[$planet_name])) {
        throw new Exception("Invalid planet");
    }

    // Calculations
    $earth_weight = (float)$weight_val;
    $factor = $gravity_factors[$planet_name];
    $final_weight = round($earth_weight * $factor, 3);

    // Load Template
    $template_path = __DIR__ . "/result.html";
    if (!file_exists($template_path)) {
        throw new Exception("Template not found");
    }
    $html = file_get_contents($template_path);

    // Replace Placeholders
    $html = str_replace("{{FINAL_WEIGHT}}", number_format($final_weight, 3, '.', ''), $html);
    $html = str_replace("{{PLANET_NAME}}", strtoupper($planet_name), $html);
    $html = str_replace("{{EARTH_WEIGHT}}", number_format($earth_weight, 3, '.', ''), $html);

    // Output for your C++ Parser
    // PHP's 'header()' function won't work correctly in a raw CGI script 
    // being parsed by a custom C++ server, so we print manually:
    echo "Status: 200 OK\r\n";
    echo "Content-Type: text/html\r\n";
    echo "Content-Length: " . strlen($html) . "\r\n";
    echo "\r\n"; // Header/Body separator
    echo $html;

} catch (Exception $e) {
    echo "Status: 500 Internal Server Error\r\n";
    echo "Content-Type: text/plain\r\n";
    echo "\r\n";
    echo "Error: " . $e->getMessage();
}
?>
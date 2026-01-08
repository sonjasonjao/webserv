#include "../include/Server.hpp"
#include "Pages.hpp"

/**
 * Main loop: instantiating Parser to parse config file, then creating Server object
 * with config data. This initial version only creates one server socket (for config
 * at index 0 for now), but that will be changed to make listener to each config.
 */
int	main(int argc, char **argv)
{
	if (argc > 3) {
		std::cout << "Usage: ./webserv [configuration file] [output log file]\n";
		return 0;
	}

	std::string	confFile = "./config_files/test.json";

	if (argc > 1)
		confFile = argv[1];
	else
		std::cout << "No config file provided, using default: " << confFile << "\n\n";

	if (argc == 3) {
		try {
			Log::setOutputFile(argv[2]);
		} catch (std::exception const &e) {
			std::cerr << "Error setting output file: " << e.what() << "\n";
			std::cerr << "Logging to std::cout instead\n\n";
		}
	}

	try
	{
		Parser	parser(confFile);
		std::cout << "\n--- Active servers ---\n\n";
		for (size_t i = 0; i < parser.getNumberOfServerConfigs(); ++i) {
			Config config = parser.getServerConfig(i);
			std::cout << "Host		: " << config.host << "\n";
			std::cout << "Host Name	: " << config.host_name << "\n";
			std::cout << "Listen		: ";
			for (auto const &p : config.ports)
				std::cout << p << " ";
			std::cout << "\n\n";
		}
		std::cout << "----------------------\n\n";
		Server	server(parser);
		Pages::loadDefaults();
		server.run();
	}
	catch(const std::exception& e) {}

	return 0;
}

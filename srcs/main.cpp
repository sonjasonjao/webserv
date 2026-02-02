#include "Server.hpp"
#include "Log.hpp"
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

		return EXIT_SUCCESS;
	}

	std::string	confFile = "./config_files/test.json"; // default configuration file

	// Handling one or two user inputs
	if (argc == 1)
		std::cout << "No config file provided, using default: " << confFile << "\n";
	if (argc < 3)
		std::cout << "No log file provided, using standard output\n\n";
	if (argc > 1)
		confFile = argv[1];
	if (argc == 3) {
		try {
			Log::setOutputFile(argv[2]);
		} catch (std::exception const &e) {
			std::cerr << "Failed to set output file, logging to standard output instead\n";
		}
	}

	try {
		Parser	parser(confFile);

		size_t	configCount = parser.getNumberOfServerConfigs();

		std::cout << "\n--- Active servers ---\n\n";
		std::cout << "Number of active servers: " << configCount << "\n";
		for (size_t i = 0; i < configCount; ++i) {
			Config config = parser.getServerConfig(i);
			std::cout << "Host		: " << config.host << "\n";
			std::cout << "Server Name	: " << config.serverName << "\n";
			std::cout << "Listen		: ";
			std::cout << config.port << " ";
			std::cout << "\n\n";
		}
		std::cout << "----------------------\n\n";

		Server	server(parser);
		Pages::loadDefaults();
		server.run();

	} catch (Parser::ParserException const &e) {
		std::cerr << "Exiting\n";

		return EXIT_FAILURE;
	} catch (std::exception const &e) {
		std::cerr << "Exiting\n";

		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "Exiting\n";

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

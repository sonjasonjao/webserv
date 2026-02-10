#include "Server.hpp"
#include "Log.hpp"
#include "Pages.hpp"
#include <iostream>

static void	debugPrintActiveServers(Parser const &parser, size_t configCount);

/**
 * Entry point for starting the webserver.
 *
 * With no command line arguments a default config file is used, otherwise the first optional command
 * line argument is the config file, and the second optional argument is a logfile for server logging.
 *
 * The parser parses the config file, and sets up data structures that are then used to start the
 * server program, which creates listener sockets for the configured ports, and begins the server loop.
 */
int	main(int argc, char *argv[])
{
	if (argc > 3) {
		std::cout << "Usage: ./webserv [configuration file] [output log file]\n";
		return EXIT_FAILURE;
	}

	std::string	confFile = "./config_files/default.json";

	// Handling the optional user inputs
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
		Server	server(parser);

		debugPrintActiveServers(parser, configCount);

		Pages::loadDefaults(); // Load fallback status pages to cache
		server.run();
	} catch (std::exception const &e) {
		std::cerr << "Exception caught at main: " << e.what() << "\n";
		std::cerr << "Exiting\n";
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "Exiting\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void	debugPrintActiveServers(Parser const &parser, size_t configCount)
{
	#if DEBUG_LOGGING
	std::cout << "\n--- Active servers ---\n\n";
	std::cout << "Number of active servers: " << configCount << "\n";
	for (size_t i = 0; i < configCount; ++i) {
		Config	config = parser.getServerConfig(i);

		std::cout << "Host		: " << config.host << "\n";
		std::cout << "Server Name	: " << config.serverName << "\n";
		std::cout << "Listen		: ";
		std::cout << config.port << " ";
		std::cout << "\n\n";
	}
	std::cout << "----------------------\n\n";
	#endif
}

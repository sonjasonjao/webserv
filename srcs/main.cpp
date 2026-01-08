#include "../include/Server.hpp"
#include "../include/Log.hpp"
#include "Pages.hpp"

/**
 * Main loop: instantiating Parser to parse config file, then creating Server object
 * with config data. This initial version only creates one server socket (for config
 * at index 0 for now), but that will be changed to make listener to each config.
 */
int	main(int argc, char **argv)
{
	// case : more than 03 arguments
	if (argc > 3) {
		std::cout << "Usage: ./webserv [configuration file] [output log file]\n";
		return (EXIT_FAILURE);
	}

	std::string	confFile 	= "./config_files/test.json"; // default configuration file

	// no arguments provided, use default configuration
	if (argc == 1) {
		std::cout << "No config file provided, using default : " << confFile <<  "\n";
		std::cout << "WARNING : No Log file provided!" << "\n\n";
	}

	// exactly 02 arguments: second argument becomes the configuration file
	if (argc == 2) {
		std::cout << "WARNING : No Log file provided!" << "\n\n";
		confFile = argv[1];
	}
	
	try {
			if( argc == 3) {
				Log::setOutputFile(argv[2]);
			}
			
			Parser	parser(confFile);
			
			size_t configCount = parser.getNumberOfServerConfigs();

			std::cout << "\n--- Active servers ---\n\n";
			std::cout << "No of active servers: " << configCount << "\n";
			
			for (size_t i = 0; i < configCount; ++i) {
				Config config = parser.getServerConfig(i);
				std::cout << "Host		: " << config.host << "\n";
				std::cout << "Host Name	: " << config.host_name << "\n";
				std::cout << "Listen		: ";
				std::cout << config.port << " ";
				std::cout << "\n\n";
			}
			
			std::cout << "----------------------\n\n";

			Server	server(parser);
			Pages::loadDefaults();
			server.run();

	} catch (const Parser::ParserException& e) {

		std::cerr << "Error : " << e.what() << "\n";
		INFO_LOG(e.what());
		
		return (EXIT_FAILURE);
	
	} catch (const std::exception& e) {
		
		std::cerr << "Error setting output file: " << e.what() << "\n";
		std::cerr << "Logging to std::cerr instead\n\n";
		
		return (EXIT_FAILURE);		
	
	} 
	return (EXIT_SUCCESS);
}

#include "../include/Server.hpp"

/**
 * Main loop: instantiating Parser to parse config file, then creating Server object
 * with config data. This initial version only creates one server socket (for config
 * at index 0 for now), but that will be changed to make listener to each config.
 */
int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "Usage: ./webserv [configuration file]\n";
		return 0;
	}

	try
	{
		Parser	parser(argv[1]);
		config_t config = parser.getServerConfig(0);
		std::cout << "Host	: " << config.host << "\n";
		std::cout << "Host Name	: " << config.host_name << "\n";
		std::cout << "Listen	: " << config.ports.at(0) << "\n";
		Server	server(parser);
		server.run();
	}
	catch(const std::exception& e) {
		ERROR_LOG(e.what());
	}

	return 0;
}

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

		Server	server(parser);
		server.run();
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << '\n';
	}

	return 0;
}

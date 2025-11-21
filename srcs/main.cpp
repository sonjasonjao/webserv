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

		std::vector<config_t>	configs = server.getConfigs();
		std::vector<config_t>::iterator it = configs.begin();
		int	listener = server.getListenerSocket(it);
		if (listener < 0)
		{
			ERROR_LOG("Server error: socket fail");
			throw std::runtime_error("Server error: socket fail");
		}

		server.run(listener);
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}

	return 0;
}

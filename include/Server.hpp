#pragma once
#include "Parser.hpp"
#include "Log.hpp"
#include <vector>
#include <unordered_map>
#include <exception>

class Server
{
	private:
		std::vector<config_t>					_configs;
		std::vector<pollfd>						_pfds;
		std::unordered_map<int, std::string>	_serverFds;

	public:
		Server(Parser& parser);
		~Server();

		std::vector<config_t> const&	getConfigs() const;

		int					getListenerSocket(std::vector<config_t>::iterator it);
		void				run(int listener);
		void				handleNewClient(int listener, int& fdCount);
		void				handleClientData(int listener, int& fdCount, int& i);
		void				handleConnections(int listener, int& fdCount);
		void				closePfds(void);
};

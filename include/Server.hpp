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
		std::unordered_map<int, config_t>		_serverFds;

	public:
		Server() = delete;
		Server(Parser const& parser);
		Server(Server& const obj);
		Server& const	operator=(Server& const other) = delete;
		~Server();

		std::vector<config_t> const&	getConfigs() const;

		void				getServerSockets(void);
		int					getListenerSocket(config_t conf);
		void				run(void);
		void				handleNewClient(int listener);
		void				handleClientData(int& i);
		void				handleConnections(void);
		void				closePfds(void);
};

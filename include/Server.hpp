#pragma once
#include "Parser.hpp"
#include "Log.hpp"
#include "Request.hpp"
#include <vector>
#include <unordered_map>
#include <exception>
#include <poll.h>
#include <csignal>

#define MAX_PENDING 20 //to be decided
#define RECV_BUF_SIZE 4096 //to be decided

class Server
{
	private:
		std::vector<config_t>				_configs;
		std::vector<pollfd>					_pfds;
		std::unordered_map<int, config_t>	_serverFds;

	public:
		Server() = delete;
		Server(Parser& parser);	//will be Parser const& later when parser is ready
		Server(Server const& obj);
		Server const&	operator=(Server const& other) = delete;
		~Server();

		std::vector<config_t> const&	getConfigs() const;

		void	createServerSockets(void);
		int		getServerSocket(config_t conf);
		void	run(void);
		void	handleNewClient(int listener);
		void	handleClientData(size_t& i);
		void	handleConnections(void);
		void	closePfds(void);
};

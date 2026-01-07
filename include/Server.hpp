#pragma once
#include "Parser.hpp"
#include "Log.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <vector>
#include <deque>
#include <unordered_map>
#include <exception>
#include <poll.h>
#include <csignal>

class Request;

#define MAX_PENDING 20 // all of these to be decided
#define RECV_BUF_SIZE 4096
#define POLL_TIMEOUT 100

struct ServerGroup
{
	int					fd;
	std::vector<Config>	configs;
	Config const		*defaultConf;
};

class Server
{
	using ReqIter = std::vector<Request>::iterator;

	private:
		std::vector<Config>								_configs;
		std::vector<pollfd>								_pfds;
		std::vector<ServerGroup>						_serverGroups;
		std::vector<Request>							_clients;
		std::unordered_map<int, std::deque<Response>>	_responses;

	public:
		Server() = delete;
		Server(Parser &parser);
		Server(Server const &obj) = delete;
		Server const	&operator=(Server const &other) = delete;
		~Server();

		std::vector<Config> const	&getConfigs() const;

		void			createServerSockets(void);
		int				createSingleServerSocket(Config conf);
		void			run(void);
		void			handleNewClient(int listener);
		void			handleClientData(size_t &i);
		void			removeClientFromPollFds(size_t &i);
		void			sendResponse(size_t &i);
		void			checkTimeouts(void);
		void			handleConnections(void);
		void			groupConfigs(void);
		bool			isGroupMember(Config &conf);
		bool			isServerFd(int fd);
		Config const	&matchConfig(Request const &req);
		ReqIter			getRequestByFd(int fd);
};

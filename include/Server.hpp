#pragma once

#include "Parser.hpp"
#include "Response.hpp"
#include <vector>
#include <list>
#include <deque>
#include <unordered_map>
#include <filesystem>
#include <unistd.h>
#include <poll.h>

#define MAX_PENDING		20 // all of these to be decided
#define RECV_BUF_SIZE	4096
#define POLL_TIMEOUT	100
#define MAX_CLIENTS		4096

struct ServerGroup
{
	int					fd;
	std::vector<Config>	configs;
	Config const		*defaultConf;
};

class Server
{
	using ReqIter = std::list<Request>::iterator;

	private:
		std::vector<Config>								_configs;
		std::vector<pollfd>								_pfds;
		std::vector<ServerGroup>						_serverGroups;
		std::list<Request>								_clients;
		std::unordered_map<int, std::deque<Response>>	_responses;
		std::map<int, Request*>							_cgiFdMap;

	public:
		Server() = delete;
		Server(Parser &parser);
		Server(Server const &obj) = delete;
		~Server();

		Server const	&operator=(Server const &other) = delete;

		std::vector<Config> const	&getConfigs() const;

		void			createServerSockets();
		int				createSingleServerSocket(Config conf);
		void			run();
		void			handleNewClient(int listener);
		void			handleClientData(size_t &i);
		void			removeClientFromPollFds(size_t &i);
		void			sendResponse(size_t &i);
		void			checkTimeouts();
		void			handleConnections();
		void			groupConfigs();
		bool			isGroupMember(Config &conf);
		bool			isServerFd(int fd);
		Config const	&matchConfig(Request const &req);
		ReqIter			getRequestByFd(int fd);
		
		// CGI handler related methods
		bool 			isCgiFd(int fd);
		void 			handleCgiOutput(size_t &i);
		void			cleanupCgi(Request* req);
};

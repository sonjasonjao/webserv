#pragma once

class Server
{
	private:
		int	_listener;
		int	_id;
		int	_port;

	public:
		Server(int sockfd);
		~Server() = default;
};

#pragma once

class Server
{
	private:
		int	_listener;

	public:
		Server(int sockfd);
		~Server();
};

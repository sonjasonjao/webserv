#pragma once

class Server
{
	private:
		int				_id;
		int				_port;
		struct pollfd*	_pfds;

	public:
		Server(); //int id, int port, what else?
		~Server();

		void	run(int listener);
		void	handleNewClient(int listener, int& fdCount);
		void	handleClientData(int listener, int& fdCount, int& i);
		void	processConnections(int listener, int& fdCount);
};

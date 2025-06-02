#pragma once

#include <time.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <queue>

namespace NETWORK_INTERFACE {

	constexpr int NET_MAX_PACKET_SIZE = 16344;
	constexpr int NET_DEFAULT_PACKET_SIZE = 1460;
	constexpr int NET_MAX_BUFFER_SIZE = 256;
	constexpr int NET_MAX_DATA_QUERY_SIZE = 256;

	extern bool NET_API_INITED;
	extern const char* ERROR; // Буфер ошибок 



	typedef void* NET_SOCKET; // Может измениться в зависимости от системы
	typedef uint8_t byte;

	typedef struct ipaddres_s {
		uint32_t host;
		uint16_t port;
	} ipaddress_t;

	enum NET_SERVER_STATUS {
		NET_SERVER_STATUS_DEAD = 0,
		NET_SERVER_STATUS_ALIVE,
		NET_SERVER_STATUS_KILLED,
		NET_SERVER_STATUS_RESTARTING,
	};

	enum NET_CLIENT_STATUS {
		NET_CLIENT_DISCONNECTED = 0,
		NET_CLIENT_DISCONNECTING,
		NET_CLIENT_CONNECTING,
		NET_CLIENT_CONNECTED,
		NET_CLIENT_ERROR
	};

	enum NET_PACKET_TYPE {

		NET_PACKET_UNDEFINED = 0,

		NET_PACKET_REQUEST_CONNECTION,
		NET_PACKET_REQUEST_DISCONNECT,
		NET_PACKET_REQUEST_KILLED,
		NET_PACKET_REQUEST_DATA,
		NET_PACKET_REQUEST_STATUS,

		NET_PACKET_RESPONSE_CONNECTION,
		NET_PACKET_RESPONSE_DISCONNECT,
		NET_PACKET_RESPONSE_FULL_SERVER,
		NET_PACKET_RESPONSE_KILLED,
		NET_PACKET_RESPONSE_STATUS,
		NET_PACKET_RESPONSE_DATA,

	};

	bool InitAPI();
	bool QuitAPI();

	std::string ipToString(ipaddress_t ip);
	ipaddress_t stringToIP(std::string ip, int port);

	struct NET_DATA {
		size_t size;
		byte* data;
	};

	struct NET_PACKET_HEADER {

		NET_PACKET_TYPE type = NET_PACKET_UNDEFINED;

		int packetnum = 0;
		size_t size = 0;
		
		char buffer[NET_MAX_BUFFER_SIZE] = {};
	};

	struct NET_PACKET {
		NET_PACKET_HEADER header;
		byte data[NET_MAX_PACKET_SIZE];
	};

	// Информация о состоянии передачи информации
	struct NET_TRANSFER_CONTEXT {

		clock_t last_activity = 0;

		bool is_busy = false;

		size_t total_size    = 0;
		size_t received_size = 0;

		std::queue<NET_DATA> query{};
		std::vector<NET_PACKET*> fragments{};
	};

	std::vector<NET_PACKET*> fragmentData(NET_DATA data, int* status, size_t fragsize);
	int defragmentData(byte** data, size_t* size, std::vector<NET_PACKET*>& fragments);
	int raw_recvData(      byte** data, size_t* size, ipaddress_t* address, NET_SOCKET socket);
	int raw_sendData(const byte*  data, size_t  size, ipaddress_t  address, NET_SOCKET socket);


	class NET_SERVER {
	protected:

		struct NET_SERVER_CLIENT {
			ipaddress_t ip;
			int priority = 1;
			NET_CLIENT_STATUS status = NET_CLIENT_DISCONNECTED;

			// Два независимых контекста
			NET_TRANSFER_CONTEXT incoming;
			NET_TRANSFER_CONTEXT outgoing;

			// Тайминги
			clock_t last_heartbeat = 0;
			uint32_t current_tick  = 0;
		};


		ipaddress_t ip;

		NET_SOCKET serverSocket = nullptr;

		NET_SERVER_STATUS status = NET_SERVER_STATUS_DEAD;

		int max_clients = 0;
		std::vector<NET_SERVER_CLIENT> clients{};

		clock_t lastTick = 0;

		int sendConnect(ipaddress_t ip);
		int sendDisconnect(int clientnum);
		bool raw_addClient(const NET_SERVER_CLIENT& nclient, int* clientnum);

	public:

		virtual ~NET_SERVER();

		NET_SERVER(const NET_SERVER& other) = delete;
		NET_SERVER(uint16_t port, int maxclients);
		
		// Start and Stop
		bool wakeUp();
		bool kill();

		// Client operating
		bool kick(int clientnum);

		//int setClientPriority(int clientnum, int priority);

		int sendData(int clientnum, const byte*  data, size_t  size);
		int recvData(int clientnum,       byte** data, size_t* size);

		int getClientNum(const ipaddress_t& ip);
		ipaddress_t getClientIP(int clientnum);


		// Server operating
		int tick();

		ipaddress_t getServerIP();
		int getMaxClients();
		int getClientsCount();
	};
	



	class NET_CLIENT {
	private:
		
		std::string name{};
		NET_SOCKET socket = nullptr;

		ipaddress_t serverIP{};

		clock_t ping = 0;
		clock_t lastTimeRecieved = 0;

		clock_t lastTick = 0;

	public:
		
		NET_CLIENT_STATUS status = NET_CLIENT_DISCONNECTED;

		NET_TRANSFER_CONTEXT incoming;
		NET_TRANSFER_CONTEXT outgoing;

		int connectTo(ipaddress_t address);
		int disconnect();

		ipaddress_t getServerAddress();

		int recvData(      void** data, size_t* size, ipaddress_t* address = nullptr);      // Если address = nullptr, то все пакеты входящие не от сервера игнорируются 
		int sendData(const void*  data, size_t  size, ipaddress_t* address = nullptr);  // Если address = nullptr, то данные отправляются на serverIP

		NET_CLIENT();
		NET_CLIENT(const NET_CLIENT&) = delete;
		virtual ~NET_CLIENT();
		
		int tick();
	};
};
#include "network.h"
#include <SDL_net.h>

namespace NETWORK_INTERFACE {

	/// 
	/// Global Functions
	/// 

	bool NET_API_INITED = false;
	const char* ERROR = SDLNet_GetError();

	clock_t time_to_left = 10000;

	bool InitAPI() {

		if (NET_API_INITED)
			return true;

		return NET_API_INITED = !SDLNet_Init();
	}

	bool QuitAPI() {
		if (!NET_API_INITED)
			return false;
		SDLNet_Quit();
		return true;
	}

	std::string ipToString(ipaddress_t ip) {
		Uint32 host = SDL_SwapBE32(ip.host);
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u:%i",
			(host >> 24) & 0xFF,
			(host >> 16) & 0xFF,
			(host >> 8) & 0xFF,
			host & 0xFF,
			(int)SDL_SwapBE16(ip.port)
		);
		return std::string(buffer);
	}


	ipaddress_t stringToIP(std::string ip, int port) {
		ipaddress_t res;
		SDLNet_ResolveHost((IPaddress*)&res, ip.c_str(), port);
		return res;
	}

	//  0 - ok
	// -1 - allocation error
	// -2 - bad parameters

	std::vector<NET_PACKET*> fragmentData(NET_DATA data, int* status, size_t fragsize) {

		std::vector<NET_PACKET*> packets{};
		*status = 0;

		if (!data.data || !data.size || !fragsize) {
			*status = -2; // Некорректные параметры
			return packets;
		}

		if (fragsize > NET_MAX_PACKET_SIZE) {
			*status = -3;
			return packets;
		}

		const int n = static_cast<int>(
			SDL_ceil(static_cast<double>(data.size) / fragsize)
	    );

		packets.reserve(n);

		for (int i = 0; i < n; i++) {
			NET_PACKET* p = new NET_PACKET;
			
			if (!p) {
				*status = -1;
				for (const auto& x : packets) {
					delete x;
				}
				packets.resize(0);
				return packets;
			}

			const size_t offset = i * fragsize;
			const size_t bytes_to_copy = static_cast<size_t>(
				std::min(fragsize, data.size - offset)
		    );

			p->header = NET_PACKET_HEADER{ 
				NET_PACKET_REQUEST_DATA, 
				i, 
				bytes_to_copy,
				{} 
			};

			

			memcpy(p->data, &data.data[offset], bytes_to_copy);
			packets.push_back(p);
		}

		delete[] data.data;
		data.data = nullptr;
		data.size = 0;

		return packets;
	}

	//  0 - ok
	// -1 - parameters error
	// -2 - fragments error
	// -3 - size error

	int defragmentData(byte** data, size_t size, std::vector<NET_PACKET*>& fragments) {
		
		if (!size || !data)
			return -1;

		size_t total_size = 0;
		size_t cur_size   = 0;

		for (const auto& x : fragments) {
			if (!x->data || !x->header.size)
				return -2;

			total_size += x->header.size;
		}

		if (size != total_size) {
			return -3;
		}
		
		*data = new byte[size];

		for (int i = 0; i < fragments.size(); i++) {
			NET_PACKET* packet = fragments[i];
			memcpy((*data) + cur_size, packet->data, packet->header.size);

			delete packet;

			cur_size += packet->header.size;
		}

		fragments.resize(0);

		return 0;
	}

	//  0 - Ошибка отправки
	//  1 - Успешная отправка
	// -1 - Ошибка параметров
	// -2 - Ошибка выделения памяти под пакет

	int raw_sendData(const byte* data, size_t size, ipaddress_t address, NET_SOCKET socket) {

		if (size > NET_DEFAULT_PACKET_SIZE)
			return -2;

		UDPpacket* packet = SDLNet_AllocPacket(NET_DEFAULT_PACKET_SIZE);

		if (!packet)
			return -1;

		packet->channel = -1;
		packet->len = size;

		memcpy(packet->data, data, size);
		packet->address.host = address.host;
		packet->address.port = address.port;
		
		int status = SDLNet_UDP_Send((UDPsocket)socket, -1, packet);

		SDLNet_FreePacket(packet);

		return status;
	}


	//  0 - Нет пакетов
	//  1 - Есть пакеты
	// -1 - Ошибка параметров
	// -2 - Ошибка выделения памяти под пакет
	// -3 - Ошибка выделения памяти для буфера

	int raw_recvData(byte** data, size_t* size, ipaddress_t* address, NET_SOCKET socket) {

		if (!data || !size || !address) {
			return -1;
		}

		*data = nullptr;
		*size = 0;

		UDPpacket* packet = SDLNet_AllocPacket(NET_MAX_PACKET_SIZE);
		if (!packet) {
			return -2;
		}

		int status = SDLNet_UDP_Recv((UDPsocket)socket, packet);

		if (status == 1) {

			address->host = packet->address.host;
			address->port = packet->address.port;

			*data = new Uint8[packet->len];

			if (!*data) {
				*size = 0;
				SDLNet_FreePacket(packet);
				return -3;
			}

			memcpy(*data, packet->data, packet->len);
			*size = packet->len;

		}
		else {
			*size = 0;
		}

		SDLNet_FreePacket(packet);
		return status;
	}


	/// Server's Functions



	NET_SERVER::~NET_SERVER() {
		kill();
	}

	NET_SERVER::NET_SERVER(uint16_t port, int maxclients) {
		ip.port = port;
		this->max_clients = maxclients;
		status = NET_SERVER_STATUS_DEAD;
	}

	bool NET_SERVER::wakeUp() {

		if (!NET_API_INITED) {
			return false;
		}

		serverSocket = (NET_SOCKET)SDLNet_UDP_Open(ip.port);


		if (!serverSocket)
			return false;


		status = NET_SERVER_STATUS_ALIVE;

		ip = getServerIP();

		return true;

	}

	bool NET_SERVER::kill() {

		if (status == NET_SERVER_STATUS_KILLED)
			return false;


		NET_PACKET_HEADER header{ NET_PACKET_REQUEST_KILLED, 0, 0, {} };
		memset(&header.buffer, 0, sizeof(header.buffer));

		UDPpacket* packet = SDLNet_AllocPacket(sizeof(NET_PACKET_HEADER));

		packet->channel = -1;
		packet->len = sizeof(NET_PACKET_HEADER);
		packet->maxlen = sizeof(NET_PACKET_HEADER);

		memcpy(packet->data, &header, sizeof(NET_PACKET_HEADER));

		for (auto& client : clients) {
			packet->address = *(IPaddress*)&client.ip;
			SDLNet_UDP_Send((UDPsocket)serverSocket, -1, packet);
		}

		SDLNet_FreePacket(packet);
		SDLNet_UDP_Close((UDPsocket)serverSocket);

		status = NET_SERVER_STATUS_KILLED;

		return true;

	}

	bool NET_SERVER::raw_addClient(const NET_SERVER_CLIENT& nclient, int* clientnum) {

		if (clientnum)
			*clientnum = -1;

		if (clients.size() >= max_clients) {
			return false;
		}

		for (int i = 0; i < clients.size(); i++) {
			if (!memcmp(&clients[i].ip, &nclient.ip, sizeof(ipaddress_t))) {
				if (clientnum)
					*clientnum = i;
				return false;
			}
		}

		if (clientnum)
			*clientnum = clients.size();

		clients.push_back(nclient);

		return true;
	}

	bool NET_SERVER::kick(int clientnum) {

		if (clientnum < 0 || clientnum >= clients.size())
			return false;

		sendDisconnect(clientnum);
		clients.erase(clients.begin() + clientnum);

		return true;
	}

	//  0 - Успешная отправка
	// -1 - Ошибка clientnum 
	// -2 - data или size
	// -3 - Ошибка клиента
	// -4 - Ошибка сервера
	// -5 - Слишком большая очередь

	int NET_SERVER::sendData(int clientnum, const byte* data, size_t size) {

		if (status != NET_SERVER_STATUS_ALIVE) {
			return -4;
		}

		if (clientnum < 0 || clientnum >= clients.size()) {
			return -1;
		}
		
		if (!data || !size) {
			return -2;
		}

		NET_SERVER_CLIENT* client = &clients[clientnum];

		if (client->status != NET_CLIENT_CONNECTED) {
			return -3;
		}

		NET_TRANSFER_CONTEXT* context = &client->incoming;

		if (context->query.size() >= NET_MAX_DATA_QUERY_SIZE)
			return -5;

		byte* ndata = new byte[size];

		memcpy(
			ndata,
			data,
			size
		);

		context->query.push(NET_DATA{ size, ndata });

		return 0;
	}

	//  0 - данные не готовы
	//  1 - данные получены
	// -1 - данные ошибка параметров
	// -2 - ошибка

	int NET_SERVER::recvData(int clientnum, byte** data, size_t* size) {

		if (clientnum < 0 || clientnum >= clients.size() || 
			!data || !size
 		) {
			return -1;
		}

		NET_SERVER_CLIENT* client = &clients[clientnum];
		NET_TRANSFER_CONTEXT* context = &client->incoming;

		if (
			context->total_size == context->received_size &&
			!context->is_busy 
		) {
			// Пока приходится копировать огромные данные, т.к.
			// (context->data придется использовать потом

			*size = context->total_size;

			if (defragmentData(data, context->total_size, context->fragments)) {
				return -2;
			}

			context->total_size    = 0;
			context->received_size = 0;
			context->is_busy = false;
			context->type = NET_PACKET_UNDEFINED;

			
			return 1;
		}
		
		return 0;
	}

	// -3 - Слишком много клиентов

	int NET_SERVER::sendConnect(ipaddress_t ip) {

		NET_PACKET_HEADER header{ NET_PACKET_RESPONSE_CONNECTION, 0, 0, {} };
		memset(&header.buffer, 0, sizeof(header.buffer));
		NET_PACKET* packet = new NET_PACKET{ header, {} };

		if (clients.size() >= max_clients) {

			header.type = NET_PACKET_RESPONSE_FULL_SERVER;
			raw_sendData((byte*)packet, sizeof(*packet), ip, (UDPsocket)serverSocket);
			delete packet;
			return -3;
		}


		int status = raw_sendData((byte*)packet, sizeof(*packet), ip, (UDPsocket)serverSocket);

		if (status == 1) {
			clients.push_back(
				NET_SERVER_CLIENT {
					ip,                   // address
					1,                    // priority
					NET_CLIENT_CONNECTED, // status 
					{},                   // transfer incoming                   
					{},                   // transfer outcoming                   
					clock(),              // last heartbeat
					0,                    // cur_tick
				}
			);

		}

		delete packet;

		return status;
	}

	int NET_SERVER::sendDisconnect(int clientnum) {

		NET_PACKET_HEADER header{ NET_PACKET_REQUEST_DISCONNECT, 0, 0, {} };
		memset(&header.buffer, 0, sizeof(header.buffer));

		NET_PACKET* packet = new NET_PACKET{ header, {} };

		int status = raw_sendData((byte*)packet, sizeof(*packet), ip, (UDPsocket)serverSocket);

		delete packet;

		return status;

	}

	ipaddress_t NET_SERVER::getServerIP() {
		return *(ipaddress_t*)SDLNet_UDP_GetPeerAddress((UDPsocket)serverSocket, -1);
	}


	int NET_SERVER::getMaxClients() {
		return max_clients;
	}
	int NET_SERVER::getClientsCount() {
		return clients.size();
	}


	//  -1 - нет клиента


	int NET_SERVER::getClientNum(const ipaddress_t& ip) {

		for (int i = 0; i < clients.size(); i++) {
			const auto& client = clients[i];
			if (!memcmp(&ip, &client.ip, sizeof(ipaddress_t)))
				return i;
		}
		return -1;
	}


	ipaddress_t NET_SERVER::getClientIP(int clientnum) {
		if (clientnum < 0 || clientnum > clients.size()) {
			return { INADDR_NONE, 0xFFFF};
		}

		return clients[clientnum].ip;
	}




	/// Client's Functions

	int NET_CLIENT::connectTo(ipaddress_t address) {

		NET_PACKET_HEADER header{ NET_PACKET_REQUEST_CONNECTION, 0, 0, {} };
		memset(&header.buffer, 0, sizeof(header.buffer));
		NET_PACKET* packet = new NET_PACKET{ header, {} };


		int status = raw_sendData((byte*)packet, sizeof(*packet), address, (UDPsocket)socket);
		
		serverIP = address;

		delete packet;

		return status;
	}

	int NET_CLIENT::disconnect() {

		
		NET_PACKET_HEADER header{ NET_PACKET_REQUEST_DISCONNECT, 0, 0, {} };
		memset(&header.buffer, 0, sizeof(header.buffer));

		NET_PACKET* packet = new NET_PACKET{ header, {} };

		int status = raw_sendData((byte*)packet, sizeof(*packet), serverIP, (UDPsocket)socket);

		delete packet;

		return status;
	}

	ipaddress_t NET_CLIENT::getServerAddress() {
		return serverIP;
	}

	NET_CLIENT::NET_CLIENT() {
		socket = (NET_SOCKET)SDLNet_UDP_Open(0);
	}

	NET_CLIENT::~NET_CLIENT() {
		disconnect();
	}
};

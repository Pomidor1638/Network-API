#include "network.h"


namespace NETWORK_INTERFACE {

	int NET_SERVER::tick_input() {

		ipaddress_t ip;
		NET_PACKET* packet = nullptr;
		size_t size = 0;
		int clientnum = -1;
		
		int status = raw_recvData(&packet, &size, &ip, serverSocket);

		if (!packet || !size || status != 1) {

			printf("Error NET_SERVER::tick_input():\n");
			printf("packet == %p\nsize == %u\nstatus == %i\n", packet, size, status);
			return -1;
		}

		clientnum = getClientNum(ip);

		if (clientnum == -1) {
			if (packet->header.type == NET_PACKET_REQUEST_CONNECTION) {
				int s = sendConnect(ip);
				if (s != 1) {
					printf("Error NET_SERVER::tick_input():\nsendConnect(ip) == %i\n", s);
					return -2;
				}
			}
			return -1;
		}


		NET_SERVER_CLIENT* client = &clients[clientnum];
		client->last_heartbeat = clock();

		NET_TRANSFER_CONTEXT* incoming = &client->incoming;
		int packet_num = packet->header.packetnum;

		if (packet_num < 0 || packet_num > incoming->fragments.size()) {
			printf("Error NET_SERVER::tick_input():\npacket_num == %i\n", packet_num);
			return -2;
		}



		switch (packet->header.type) {
		case NET_PACKET_RESPONSE_DATA:

			if (!incoming->is_busy) {
				return -1;
			}

			if (incoming->fragments[packet_num] == nullptr) {
				incoming->fragments[packet_num] = new NET_PACKET;
				memcpy(
					incoming->fragments[packet_num],
					packet,
					size
				);

			}
			break;
		}

		delete packet;
		packet = nullptr;
		size = 0;
	}



	int NET_SERVER::tick_output(int clientnum) {

	}





	int NET_SERVER::tick() {

		if (status != NET_SERVER_STATUS_ALIVE)
			return -1;

		int count = clients.size();
		int clientnum = -1;
		int status = -1;

		ipaddress_t ip;
		size_t size = 0;
		NET_PACKET* packet;

		memset(&ip, 0xFF, sizeof(ipaddress_t));

		for (int i = 0; i < count + 1; i++) {



		}
		return 1;
	}
}
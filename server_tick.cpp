#include "network.h"


namespace NETWORK_INTERFACE {

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

			status = raw_recvData(&packet, &size, &ip, serverSocket);

			if (!packet || !size || status != 1) {

				printf("Error NET_SERVER::tick:\n");
				printf("packet == %p\nsize == %u\nstatus == %i\n", packet, size, status);
				continue;
			}
			
			clientnum = getClientNum(ip);

			if (clientnum == -1) {
				if (packet->header.type == NET_PACKET_REQUEST_CONNECTION) {
					int s = sendConnect(ip);
					if (s != 1) {
						printf("Error NET_SERVER::tick: sendConnect(ip) == %i\n", s);
						return -2;
					}
				}
				continue;
			} 


			NET_SERVER_CLIENT* client = &clients[clientnum];
			NET_TRANSFER_CONTEXT* outgoing = &client->outgoing;
			int packet_num = packet->header.packetnum;

			client->last_heartbeat = clock();

			switch (packet->header.type) {
			case NET_PACKET_RESPONSE_DATA:

				if (packet_num < 0 || packet_num > outgoing->fragments.size()) {
					printf("Error NET_SERVER::tick: packet_num == %i\n", packet_num);
					return -1;
				}

				if (outgoing->fragments[packet_num] == nullptr) {
					memcpy(
						outgoing->fragments[packet_num],
						packet,
						size
					);

				}
				break;


			default:
				break;
			}

			delete packet;
			packet = nullptr;
			size = 0;
		}
		return 1;
	}

}
#include "network.h"


namespace NETWORK_INTERFACE {

	int NET_SERVER::tick_process_client_packet(const ipaddress_t& ip, NET_PACKET* packet) {
		int clientnum = getClientNum(ip);

		if (clientnum == -1) {
			if (packet->header.type == NET_PACKET_REQUEST_CONNECTION) {
				int s = sendConnect(ip);
				if (s != 1) {
					printf("Error process_client_packet(): sendConnect(ip) == %i\n", s);
					return -2;
				}
			}
			return -1;
		}

		clients[clientnum].last_heartbeat = clock();
		return clientnum;
	}

	void NET_SERVER::tick_handle_client_data(int clientnum, NET_PACKET* packet) {
		NET_SERVER_CLIENT& client = clients[clientnum];
		NET_TRANSFER_CONTEXT& incoming = client.incoming;
		int packet_num = packet->header.packetnum;

		if (packet_num < 0 || packet_num >= static_cast<int>(incoming.fragments.size())) {
			printf("Error handle_client_data(): packet_num == %i\n", packet_num);
			exit(1);
			return;
		}

		if (!incoming.is_busy) {
			printf("Warning: handle_client_data(): received fragment while not busy\n");
			exit(1);
			return;
		}
		

		switch (incoming.type) {
		case NET_PACKET_UNDEFINED: {
			NET_PACKET_TYPE type = packet->header.type;
			if (	
				type <= NET_PACKET_REQUEST_STATUS and
				type >= NET_PACKET_REQUEST_CONNECTION
			) {
				incoming.is_busy = true;
				incoming.last_activity = client.last_heartbeat;

			}
			break;
		}
		case NET_PACKET_REQUEST_DATA:
			switch (packet->header.type) {
			case NET_PACKET_RESPONSE_DATA:
				
				break;

			default:
				break;
			}
			break;

		default:
			printf("Error tick_handle_client_data:\nincoming.type == %i\n", incoming.type);
			exit(1);
			return;
		}

	}


	int NET_SERVER::tick_incoming() {
		ipaddress_t ip;
		NET_PACKET* packet = nullptr;
		size_t size = 0;

		int status = raw_recvData((byte**)&packet, &size, &ip, serverSocket);
		if (status != 1) 
			return status;

		int clientnum = tick_process_client_packet(ip, packet);

		if (clientnum >= 0) {
			tick_handle_client_data(clientnum, packet);
		}

		if (packet) {
			delete packet;
		}
		
		return 0;
	}

	int NET_SERVER::tick_outgoing(int clientnum) {
		return 0;
	}

	int NET_SERVER::tick() {
		if (status != NET_SERVER_STATUS_ALIVE)
			return -1;

		int count = clients.size();
		for (int i = 0; i < count; ++i) {
			tick_outgoing(i);
		}
		tick_incoming(); 

		return 1;
	}
}

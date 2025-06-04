#include "network.h"


namespace NETWORK_INTERFACE {

	int NET_SERVER::tick_process_client_packet(const ipaddress_t& ip, NET_PACKET* packet) {
		int clientnum = getClientNum(ip);

		if (clientnum == -1) {
			if (packet->header.type == NET_PACKET_REQUEST_CONNECTION) {
				int s = sendConnect(ip);
				if (s != 1) {
					printf("Error process_client_packet():\nsendConnect(ip) == %i\n", s);
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
		NET_TRANSFER_CONTEXT& outgoing = client.outgoing;
		int packet_num = packet->header.packetnum;

		if (packet_num < 0 || packet_num >= static_cast<int>(incoming.fragments.size())) {
			printf("Error handle_client_data():\npacket_num == %i\n", packet_num);
			exit(1);
			return;
		}

		if (!incoming.is_busy) {
			printf("Warning: handle_client_data():\nreceived packet while not busy\n");
			exit(1);
			return;
		}
		
		if (packet->header.type == NET_PACKET_MESSAGE_UNEXCEPTED) {
			incoming.is_busy = false;
			for (const auto& x : incoming.fragments) {
				if (x)
					delete x;
			}
			incoming.last_activity = client.last_heartbeat;
			incoming.received_size = 0;
			incoming.total_size = 0;
			incoming.type = NET_PACKET_UNDEFINED;
		}

		switch (incoming.type) {
		case NET_PACKET_UNDEFINED: {
			NET_PACKET_TYPE type = packet->header.type;
			switch (type) {
			case NET_PACKET_REQUEST_DATA:
				incoming.is_busy = true;
				incoming.last_activity = client.last_heartbeat;
				incoming.received_size = 0;
				incoming.total_size = packet->header.size;
				incoming.type = NET_PACKET_REQUEST_DATA;
				for (const auto& x : incoming.fragments) {
					if (x)
						delete x;
				}

				incoming.fragments.resize(packet_num);
				for (auto& x : incoming.fragments) {
					x = nullptr;
				}


				break;
			case NET_PACKET_REQUEST_DISCONNECT:
				sendDisconnect(clientnum, false);
				break;
			case NET_PACKET_REQUEST_STATUS: {
				NET_DATA d = makeStatus();
				if (!d.size || !d.data) {
					printf("Error tick_handle_client_data:\n");
					printf("makeStatus() can\'t allocate status\n");
					exit(1);
				}
				outgoing.query.push(d);
			}
				break;
			default:
				sendUnExcepted(&client);
				break;
			}
			break;
		}
		case NET_PACKET_REQUEST_DATA:
			switch (packet->header.type) {
			case NET_PACKET_RESPONSE_DATA:
				NET_PACKET*& fragment = incoming.fragments[packet_num];
				if (fragment) {
					fragment = packet;
				}

				incoming.last_activity = client.last_heartbeat;
				incoming.received_size += fragment->header.size;
				
				if (incoming.received_size == incoming.total_size) {
					NET_DATA data{incoming.total_size, nullptr};
					int def_status = defragmentData(
						&data.data,
						data.size,
						incoming.fragments
					);

					if (def_status) {
						// There is must be some shit to
						// tell client that fragments corrupted
						// but now I fuck it
						printf("Error ")
					}



				}


				break;

			default:
				sendUnExcepted(&client);
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

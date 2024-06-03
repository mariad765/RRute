#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include <netinet/in.h> // Pentru htons
#include <string.h>
#include <arpa/inet.h>  // Pentru inet_ntop

struct route_table_entry *rtable;
struct arp_table_entry *arptable;

int arptable_len;
int rtable_len;

struct route_table_entry *get_router_entry(struct route_table_entry *rtable, int rtable_size, uint32_t router_ip) {
    for (int i = 0; i < rtable_size; i++) {
        if ((router_ip & rtable[i].mask) == rtable[i].prefix) {
            return &rtable[i];
        }
    }
    return NULL;
}
struct route_table_entry *get_best_route(uint32_t ip_dest) {
	/* TODO 2.2: Implement the LPM algorithm */
	/* We can iterate through rtable for (int i = 0; i < rtable_len; i++). Entries in
	 * the rtable are in network order already */
	  struct route_table_entry *best_route = NULL;
	  int max_len = -1;

	   for (int i = 0; i < rtable_len; i++) {
			struct route_table_entry *entry = &rtable[i];
			if ((entry->prefix & entry->mask) == (ip_dest & entry->mask)){
				int prefix_len = __builtin_popcount(entry->mask);
				if (prefix_len > max_len) {
					max_len = prefix_len;
					best_route = entry;
				}
			}
		}


	return best_route;
}
struct arp_table_entry *get_arp_entry(struct arp_table_entry *arptable, int arptable_size, uint32_t ip) {
	for (int i = 0; i < 100; i++) {
		if (arptable[i].ip == ip) {
			return &arptable[i];
		}
	}
	return NULL;
}
struct arp_table_entry *get_mac_entry(uint32_t given_ip) {
	/* TODO 2.4: Iterate through the MAC table and search for an entry
	 * that matches given_ip. */
	 struct arp_table_entry *mac_stuff = NULL;


	/* We can iterate thrpigh the mac_table for (int i = 0; i <
	 * mac_table_len; i++) */
	 for (int i = 0; i < arptable_len; i++){

    	struct arp_table_entry *mac_stuff_aux = &arptable[i];
		if (mac_stuff_aux->ip == given_ip) {
            mac_stuff = mac_stuff_aux;
            break;
        }

	 }
	return mac_stuff;;
}

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];
	// find the table for each router
	/* Code to allocate the route tables */
	rtable = malloc(sizeof(struct route_table_entry) * 90000);

	
	/* Read the static routing table and the MAC table */
	rtable_len = read_rtable(argv[1], rtable);

	// arp table
	arptable = malloc(sizeof(struct arp_table_entry) * 90000);
	/* DIE is a macro for sanity checks */
	DIE(arptable == NULL, "memory");

	/* Read the static routing table and the MAC table */
	arptable_len = parse_arp_table("/home/kali/Pcom/t1/homework1-public/arp_table.txt", arptable);



	// Do not modify this line
	init(argc - 2, argv + 2);


	while (1) {

		int interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *) buf;
		/* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */

		// check if router is dest
		struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));
		// set fields
		// ip_hdr->tos = 0;
		// ip_hdr->frag_off = htons(0);
		// ip_hdr->version = 4;
    	// ip_hdr->ihl = 5;
		// ip_hdr->id = htons(1);
		uint16_t received_checksum = ip_hdr->check;
		ip_hdr->check = 0;
        uint16_t calculated_checksum = checksum((uint16_t *)ip_hdr, sizeof(struct iphdr));
		ip_hdr->check = htons(calculated_checksum);
		// check equality
		char *interface_ip = get_interface_ip(interface);
		
		char dest_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(ip_hdr->daddr), dest_ip, INET_ADDRSTRLEN);

		// Comparați adresele IP
		if (strcmp(dest_ip, interface_ip) == 0) {
		
		} else {
			
			// do checksum
			if (ip_hdr->check  != received_checksum) {
			printf("Pachetul a fost corupt! Aruncăm pachetul.\n");
			// Aruncați pachetul sau luați alte măsuri în consecință
			continue;
			} 
			// Verificare TTL
			if (ip_hdr->ttl <= 1) {
				printf("TTL-ul este prea mic! Aruncăm pachetul.\n");
				break;
			} else {
				// Actualizăm TTL-ul
				ip_hdr->ttl--;
				// Recalculăm checksum-ul pachetului IP
				ip_hdr->check = 0;
				ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

				// need to get next hop
				uint32_t unit_interface_ip = ip_hdr->daddr;

				struct route_table_entry *router_entry = get_best_route(unit_interface_ip);

				// add check for null
				if (router_entry == NULL) {
					fprintf(stderr, "No route found for destination IP address.\n");
					return -1;
				}

				uint8_t mac[6];
				get_interface_mac(interface, mac);
			

				// get the mac for the next hop
				
				struct arp_table_entry *arp_entry = get_mac_entry(router_entry->next_hop);
				// add check
				if (arp_entry == NULL) {
					fprintf(stderr, "No MAC address found for destination IP address.\n");
					return -1;
				}



				memcpy(eth_hdr->ether_dhost, arp_entry->mac, 6);
				memcpy(eth_hdr->ether_shost, mac, 6);                 
			
				//Call send_to_link(best_router->interface, packet, packet_len);
				send_to_link(router_entry->interface, buf, len);

				}


		}

	}
}


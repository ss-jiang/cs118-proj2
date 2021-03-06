#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h> 
#include <signal.h>
#include <fstream>
#include <stdint.h>
#include <string>
#include <thread>
#include <iostream>
#include <vector>

#include "TCPheader.h"

// server is called with the following parameters
// server <PORT> <FILE-DIR>

bool SIG_HANDLER_CALLED = 0;

void signal_handler(int signum)
{
	std::cout.flush();
	SIG_HANDLER_CALLED = 1;
	// std::cout << "Signal handler called" << std::endl;
	exit(0);
}

void printStatement (std::string action, uint32_t seq_num, uint32_t ack_num, uint16_t cid, int cwd, int ss_thresh, std::bitset<16> fl, int dup) {
  std::cout << action << " " << seq_num << " " << ack_num << " " << cid << " " << cwd << " " << ss_thresh;
  if (fl[2]) {
    std::cout << " ACK";
  }
  if(fl[1]) {
    std::cout << " SYN";
  }
  if(fl[0]) {
    std::cout << " FIN"; 
  }
  if(dup) {
  	std::cout << " DUP";
  }
  std::cout << std::endl; 
}

void printDropStatement(std::string action, uint32_t seq_num, uint32_t ack_num, uint16_t cid, std::bitset<16> fl) {
  std::cout << action << " " << seq_num << " " << ack_num << " " << cid << std::endl;
}

struct file_metadata {
	// std::ofstream* file_d;
	uint32_t last_sent_seq;
	uint32_t next_expected_seq;
};

void pointerToBuffer(unsigned char* buf, unsigned char dest_buf[], int bytes)
{
  for (int i = 0; i < bytes; i++)
  {
    dest_buf[i] = buf[i];
  } 
}

bool in_fin_vector(std::vector<uint16_t>& in_vector, uint16_t& id)
{
	for (unsigned int i = 0; i < in_vector.size(); i++)
	{
		if (in_vector[i] == id)
			return true;
	}
	return false;
}

int main(int argc, char* argv[])
{
	if (argc < 3) {
      	std::cerr << "ERROR: server must be called with exactly two parameters: <PORT> <FILE-DIR>" << std::endl;
      	exit(-1);
   	}

   	// Handle signals
   	struct sigaction act;
   	act.sa_handler = &signal_handler;
   	sigaction(SIGQUIT, &act, NULL);
   	sigaction(SIGTERM, &act, NULL);
   	// FOR TESTING PURPOSES: Ctrl-C
   	// sigaction(SIGINT, &act, NULL);
	
   	int port_num = atoi(argv[1]);
   	std::string file_dir = argv[2];
   	struct addrinfo hints, *res;
    int status;
    std::string file_name_1 = file_dir + "/";  
    std::string file_name_2 = ".file";
    std::ofstream new_file;

   	// Check that the port number is in range
   	if (port_num < 1024 || port_num > 65535) {
	    std::cerr << "ERROR: Port number out of range [1024 - 65535]" << std::endl;
	    exit(1);
	}

	memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE; 
    hints.ai_family = AF_INET; // AF_INET specifies IPv4
    hints.ai_socktype = SOCK_DGRAM;

    // e.g. "www.example.com" or IP; e.g. "http" or port number
	if ((status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
        std::cerr << "ERROR: getaddrinfo: " << gai_strerror(status) << std::endl;
        exit(1);
    }

	// create a socket for UDP
	int sockfd = socket(res->ai_family, res->ai_socktype, 0);
	if (sockfd == -1) {
		std::cerr << "ERROR: Failed to create socket" << std::endl;
		exit(-1);
	}

	// allow others to reuse the address
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	// bind the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		std::cerr << "ERROR: Failed to bind to socket" << std::endl;
		exit(-1);
	}

	// set non-blocking
	long arg = fcntl(sockfd, F_GETFL, NULL); 
	arg |= O_NONBLOCK; 
	fcntl(sockfd, F_SETFL, arg); 

	// set socket to listen status
	// UDP does not need to listen since it's connectionless
	// if (listen(sockfd, 1) == -1) {
	// 	std::cerr << "ERROR: Failed to listen to socket" << std::endl;
	// 	exit(-1);
	// }
	// sleep(30);

	std::vector<file_metadata> file_des;
	std::vector<uint16_t> fin_connIds;

	int cwd = 512; 
	int ss_thresh = 10000;
	// accept a new connection
	struct sockaddr_storage clientAddr;
	socklen_t clientAddrSize = sizeof(clientAddr);
	char buf[524];

	uint32_t server_seq = 4321;
    uint32_t server_ack;
    uint16_t cid = 0; 

	// UDP, don't need to connect since no concept of connection
	// use recvfrom() to read
	while(1)
	{
		// receive first part of handshake 
		int rc = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&clientAddr, &clientAddrSize);
	    if (rc > 0)
	    {
	    	// isolate the header bytes from the packet
	    	unsigned char* headers_buf = new unsigned char[12]; 
		    for(int i = 0; i < 12; i++) {
		    	headers_buf[i] = (unsigned char) buf[i]; 
		    }

		    TCPheader header;
		    header.parseBuffer(headers_buf);
		    printStatement("RECV", header.getSeqNum(), header.getAckNum(), header.getConnectionId(), cwd, ss_thresh, header.getFlags(), 0);	
		    // check flags, we need the SYN/ACK/FIN flags and the connection id
		    std::bitset<16> f = header.getFlags();

		    uint16_t conn_id = header.getConnectionId();

		    // if receive a packet that is out of sequence
		    if (conn_id > 0 && file_des[conn_id-1].next_expected_seq != header.getSeqNum())
		    {
                printDropStatement("DROP", header.getSeqNum(), 0, header.getConnectionId(), header.getFlags()); 
                unsigned char* drop_buff = new unsigned char[12]; 
	            server_seq = 4322; // 0, no ACK flag set
	            server_ack = file_des[conn_id-1].next_expected_seq;
	            cid = header.getConnectionId();

	            TCPheader drop_header(server_seq, server_ack, cid, 1, 0, 0);
	            drop_buff = drop_header.toCharBuffer();

	            unsigned char drop_buf[12];
	            pointerToBuffer(drop_buff, drop_buf, 12);

	            printStatement("SEND", server_seq, server_ack, drop_header.getConnectionId(), cwd, ss_thresh, drop_header.getFlags(), 1);
                if (sendto(sockfd, drop_buf, sizeof(drop_buf), 0, (struct sockaddr*)&clientAddr, clientAddrSize) < 0)
			    {
			    	std::cerr << "ERROR: Could not send response header\n";
			        exit(1); 
			    }
			    continue;          
		    }

		    // received SYN from client 
		    if (f[1])
		    {
			    server_ack = header.getSeqNum() + 1; // ack starts on the next byte of received seq num
			    // increment connection count
				cid++;

		    	// create new file descriptor for each connection
				std::ofstream new_file;

				file_metadata new_connection_data;
				// new_connection_data.file_size = 0;
				// new_connection_data.file_name = file_dir + "/" + std::to_string(cid) + ".file";
				new_connection_data.last_sent_seq = (server_ack % 102401);
				new_connection_data.next_expected_seq = server_ack;

				file_des.push_back(new_connection_data);

				// new_file.open()

			    // send SYN-ACK to client, responds to receiving a packet from the client
			    // response headers needs to be set up with the receiver header's ack number
			    TCPheader resp_header(4321, server_ack, cid, 1, 1, 0);

			    // sending 2nd part of the handshake buffer
			    unsigned char* resp_buf = resp_header.toCharBuffer(); 
			    unsigned char hs2_buf[12];
			    pointerToBuffer(resp_buf, hs2_buf, 12);

			    printStatement("SEND", resp_header.getSeqNum(), resp_header.getAckNum(), resp_header.getConnectionId(), cwd, ss_thresh, resp_header.getFlags(), 0);	
			    if (sendto(sockfd, hs2_buf, sizeof(hs2_buf), 0, (struct sockaddr*)&clientAddr, clientAddrSize) < 0)
			    {
			    	std::cerr << "ERROR: Could not send response header\n";
			        exit(1); 
			    }
		    }
		    // FIN flag received
			if (f[0])
			{
				unsigned char* fin_ack_buff = new unsigned char[12]; 
	            server_seq = 4322; // 0, no ACK flag set
	            server_ack = header.getSeqNum() + 1;
	            cid = header.getConnectionId();

	            fin_connIds.push_back(cid);

	            TCPheader fin_ack_header(server_seq, server_ack, cid, 1, 0, 1);
	            fin_ack_buff = fin_ack_header.toCharBuffer();

	            unsigned char fin_buf[12];
	            pointerToBuffer(fin_ack_buff, fin_buf, 12);

	            printStatement("SEND", fin_ack_header.getSeqNum(), fin_ack_header.getAckNum(), fin_ack_header.getConnectionId(), cwd, ss_thresh, fin_ack_header.getFlags(), 0);	
	            if (sendto(sockfd, fin_buf, sizeof(fin_buf), 0, (struct sockaddr*)&clientAddr, clientAddrSize) < 0)
	            {
	            	std::cerr << "ERROR: Could not send file\n";
                  	exit(1);
	            }
	           	// sleep(1);
	           	// if (sendto(sockfd, fin_buf, sizeof(fin_buf), 0, (struct sockaddr*)&clientAddr, clientAddrSize) < 0)
	            // {
	            // 	std::cerr << "ERROR: Could not send file\n";
             //      	exit(1);
	            // }

	           	delete(fin_ack_buff);
			}
		    // receiving 3rd part of the handshake, the data begins to be received here
		    // ACK flag and no SYN flag
		    if (conn_id > 0 && !in_fin_vector(fin_connIds, conn_id))
		    {
	            unsigned char* headers3_buf = new unsigned char[12]; 
	            for(int i = 0; i < 12; i++) {
	              headers3_buf[i] = buf[i]; 
	            }

			    TCPheader hs3_header; 
			    hs3_header.parseBuffer(headers3_buf);

			    char data_buffer[512]; 
			    for(unsigned int i = 12; i < sizeof(buf); i++) {
			    	data_buffer[i - 12] = buf[i];
			    }

			    // file_des[conn_id-1].next_expected_seq += (rc - 12);
			    // file_des[conn_id-1].next_expected_seq %= 102401;
			    // std::cout << "NEXT EXPECTED: " << file_des[conn_id-1].next_expected_seq << std::endl;

			    std::string open_name = file_name_1 + std::to_string(conn_id) + file_name_2;
			    new_file.open(open_name, std::ios::app | std::ios::binary );

			    new_file.write(data_buffer, rc - 12);
			    // file_des[conn_id-1].file_size += (rc - 12);
			    memset(buf, 0, sizeof(buf));
			    new_file.close();

			    server_ack = (hs3_header.getSeqNum() + (rc - 12)) % 102401;
			    file_des[conn_id-1].next_expected_seq = server_ack;
			    // // THIS MAY BE WRONG 
			    // server_seq = hs3_header.getAckNum() + 1;
			    // // THIS MAY BE WRONG 

			    if (hs3_header.getAckNum() == 0)
			    {
			    	server_seq = file_des[conn_id-1].last_sent_seq;
			    }
			    else
			    {
			    	server_seq = (hs3_header.getAckNum()) % 102401;
			    	file_des[conn_id-1].last_sent_seq = server_seq;
			    }

			    TCPheader resp_header(server_seq, server_ack, hs3_header.getConnectionId(), 1, 0, 0);
			    unsigned char* ack_buf = resp_header.toCharBuffer(); 

	            unsigned char ack_buffer[12]; 
	            pointerToBuffer(ack_buf, ack_buffer, 12);

			    printStatement("SEND", resp_header.getSeqNum(), resp_header.getAckNum(), resp_header.getConnectionId(), cwd, ss_thresh, resp_header.getFlags(), 0);	

			    // send ACK back to client
			    if (sendto(sockfd, ack_buffer, sizeof(ack_buffer), 0, (struct sockaddr*)&clientAddr, clientAddrSize) < 0)
			    {
			    	std::cerr << "ERROR: Could not send response header\n";
			    	exit(1);
			    }
			    delete(headers3_buf);
			}
			delete(headers_buf);
	    }
	}
	close(sockfd);
   	exit(0);
}
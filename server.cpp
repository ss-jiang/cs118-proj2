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
	SIG_HANDLER_CALLED = 1;
	std::cout << "Signal handler called" << std::endl;
	exit(0);
}

// void handle_thread(struct sockaddr_in clientAddr, int clientSockfd, int connection_number, std::string file_dir)
// {
// 	char ipstr[INET_ADDRSTRLEN] = {'\0'};

// 	inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
// 	std::cout << "Accept a connection from: " << ipstr << ":" << ntohs(clientAddr.sin_port) << std::endl;

// 	std::string save_name = file_dir + "/" + std::to_string(connection_number) + ".file";

// 	// Set to blocking mode again... 
// 	long arg = fcntl(clientSockfd, F_GETFL, NULL); 
// 	arg &= (~O_NONBLOCK);
// 	fcntl(clientSockfd, F_SETFL, arg); 

// 	// timeout variables
// 	fd_set readfds;
// 	struct timeval tv;
// 	FD_ZERO(&readfds);
// 	FD_SET(clientSockfd, &readfds);
// 	tv.tv_sec = 10;
// 	int rv = select(clientSockfd+1, &readfds, NULL, NULL, &tv);

// 	if (rv == -1) 
// 	{
// 		std::cerr << "ERROR: Select() failure" << std::endl;
// 	    exit(1);
// 	} 
// 	else if (rv == 0) 
// 	{
// 		// variables used to open and write to a file
// 		std::ofstream new_file;
// 		new_file.open(save_name, std::ios::out | std::ios::binary);

// 		//std::string err = "ERROR";
// 		new_file.write("ERROR", 5);
// 	    std::cerr << "Timeout occurred:  No data after 10 seconds\n";
// 	} 
// 	else 
// 	{
// 		// variables used to open and write to a file
// 		std::ofstream new_file;
// 		new_file.open(save_name, std::ios::out | std::ios::binary);
// 		char receive_buf[1024] = {0};
// 		int file_size = 0;
// 		int rc = 0;

// 		/* get the file name from the client */
// 	    while( (rc = recv(clientSockfd, receive_buf, sizeof(receive_buf), 0)) > 0)
// 	    {
// 		    if (rc == -1) {
// 		    	std::cerr << "ERROR: recv() failed\n";
// 				exit(1);
// 		    }

// 		    new_file.write(receive_buf, rc);
// 		    file_size += rc;
// 		    memset(receive_buf, 0, sizeof(receive_buf));
// 		}

// 		std::cout << "Saved file as " << save_name << " : " << file_size << " bytes\n";
// 		file_size = 0;
// 		new_file.close();
// 	}
// 	close(clientSockfd);
// 

struct file_metadata {
	std::string file_name;
	// std::ofstream* file_d;
	int file_size;
};

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
	// long arg = fcntl(sockfd, F_GETFL, NULL); 
	// arg |= O_NONBLOCK; 
	// fcntl(sockfd, F_SETFL, arg); 

	// set socket to listen status
	// UDP does not need to listen since it's connectionless
	// if (listen(sockfd, 1) == -1) {
	// 	std::cerr << "ERROR: Failed to listen to socket" << std::endl;
	// 	exit(-1);
	// }

	std::vector<file_metadata> file_des;
	std::vector<uint16_t> fin_connIds;

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
		// if (rc == -1) 
	 //    {
	 //    	std::cerr << "ERROR: recvfrom() failed\n";
		// 	exit(1);
	 //    }
	    if (rc > 0)
	    {
	    	// isolate the header bytes from the packet
	    	unsigned char* headers_buf = new unsigned char[12]; 
		    for(int i = 0; i < 12; i++) {
		    	headers_buf[i] = (unsigned char) buf[i]; 
		    }

		    TCPheader header;
		    header.parseBuffer(headers_buf);

		    // check flags, we need the SYN/ACK/FIN flags and the connection id
		    std::bitset<16> f = header.getFlags();
		    uint16_t conn_id = header.getConnectionId();

		    // received SYN from client 
		    if (f[1] == 1)
		    {
		    	std::cout << ">>>>>>>>>> 1st part of handshake received" << std::endl;

			    server_ack = header.getSeqNum() + 1; // ack starts on the next byte of received seq num

		    	// create new file descriptor for each connection
	    		std::string save_name = file_dir + "/" + std::to_string(cid) + ".file";
				std::ofstream new_file;

				file_metadata new_connection_data;
				new_connection_data.file_size = 0;
				new_connection_data.file_name = file_dir + "/" + std::to_string(cid) + ".file";
				// new_file.open(save_name, std::ios::out | std::ios::binary);
				// new_connection_data.file_d = &new_file;

				file_des.push_back(new_connection_data);

				// increment connection count
				cid++;

			    // send SYN-ACK to client, responds to receiving a packet from the client
			    // response headers needs to be set up with the receiver header's ack number
			    std::cout << "<<<<<<<<<<<<< 2nd part of handshake sent" << std::endl;
			    TCPheader resp_header(server_seq, server_ack, cid, 1, 1, 0);
			    resp_header.printInfo();

			    // sending 2nd part of the handshake buffer
			    unsigned char* resp_buf = resp_header.toCharBuffer(); 
			    unsigned char hs2_buf[12]; 
			    for(int i = 0; i < 12; i++) {
			      hs2_buf[i] = resp_buf[i];
			    }

			    int sent = 0;
			    if ( (sent = sendto(sockfd, hs2_buf, sizeof(hs2_buf), 0, (struct sockaddr*)&clientAddr, clientAddrSize) > 0))
			    {
			      if (sent == -1)
			      {
			        std::cerr << "ERROR: Could not send response header\n";
			        exit(1); 
			      }
			    }

		    }
		    // client sends ACK to server's FIN-ACK
			if (f[2] && in_fin_vector(fin_connIds, conn_id))
			{
				std::cout << "closing connection: " << conn_id << std::endl;
			}

		    // receiving 3rd part of the handshake, the data begins to be received here
		    // ACK flag and no SYN flag
		    if (f[2] && !f[1] && conn_id > 0 && !in_fin_vector(fin_connIds, conn_id))
		    {
		    	//std::cout << ">>>>>>>>>> 3rd part of handshake received" << std::endl; 
		    	std::cout << ">>>>>>>>>> received ack\n";
	            unsigned char* headers3_buf = new unsigned char[12]; 
	            for(int i = 0; i < 12; i++) {
	              headers3_buf[i] = buf[i]; 
	            }

			    TCPheader hs3_header; 
			    hs3_header.parseBuffer(headers3_buf);

			    char data_buffer[512]; 
		    	int j = 0; 
			    for(int i = 12; i < 524; i++) {
			    	data_buffer[j] = buf[i];
			    	j++;
			    }

			    std::ofstream new_file;
			    new_file.open(file_des[conn_id-1].file_name, std::ios::app | std::ios::binary );

			    new_file.write(data_buffer, rc - 12);
			    file_des[conn_id-1].file_size += (rc - 12);
			    memset(buf, 0, sizeof(buf));
			    std::cout << "File size: " << file_des[conn_id-1].file_size << std::endl;
			    new_file.close();

			    std::cout << ">>>>>>>>> sending ack" << std::endl;
			    server_ack = hs3_header.getSeqNum() + (rc - 12);
			    server_seq = hs3_header.getAckNum();

			    TCPheader resp_header(server_seq, server_ack, hs3_header.getConnectionId(), 1, 0, 0);
			    resp_header.printInfo();
			    unsigned char* ack_buf = resp_header.toCharBuffer(); 
	            unsigned char ack_buffer[12]; 
	            for(int i = 0; i < 12; i++) {
	            	ack_buffer[i] = ack_buf[i];
	            }

			    // send ACK back to client
			    if (sendto(sockfd, ack_buffer, sizeof(ack_buffer), 0, (struct sockaddr*)&clientAddr, clientAddrSize) < 0)
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

	            std::cout << ">>>>>>>>>>>>>>> received fin\n";
	            TCPheader fin_ack_header(server_seq, server_ack, cid, 1, 0, 1);
	            fin_ack_buff = fin_ack_header.toCharBuffer();
	            unsigned char fin_buf[12]; 
	            for(int i = 0; i < 12; i++) {
	            	fin_buf[i] = fin_ack_buff[i];
	            }
	            fin_ack_header.printInfo();

	            std::cout << "<<<<<<<<<<< sending fin-ack\n";
	            if (sendto(sockfd, fin_buf, sizeof(fin_buf), 0, (struct sockaddr*)&clientAddr, clientAddrSize) < 0)
	            {
	            	std::cerr << "ERROR: Could not send file\n";
                  	exit(1);
	            }
	            //exit(1);
			}
	    }
	}


	close(sockfd);
	// close(clientSockfd);

   	exit(0);
}

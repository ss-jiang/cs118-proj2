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

#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string>
#include <thread>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <fstream>

#include "TCPheader.h"

#define MAX_PATH_LENGTH        4096

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cerr << "ERROR: client must be called with exactly three parameters: <HOSTNAME-OR-IP> <PORT> <FILENAME>" <<std::endl;
    exit(-1);
  }

  std::string ip_addr = argv[1];
  int port_num = atoi(argv[2]);
  std::string file_name = argv[3];
  bool syn_ack_established = false;
  int read_offset = 0;

  // for timeout
  // fd_set myset;
  // int valopt;
  // struct timeval tv;
  // socklen_t lon;
  // long arg; 

  // Check that the port number is in range
  if (port_num < 1024 || port_num > 65535) {
    std::cerr << "ERROR: Port number out of range [1024 - 65535]" << std::endl;
    exit(1);
  }

    // create a socket using UDP
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  // // set non-blocking
  // long arg = fcntl(sockfd, F_GETFL, NULL); 
  // arg |= O_NONBLOCK; 
  // fcntl(sockfd, F_SETFL, arg); 

  // stuff to get address information and connect
  struct addrinfo hints, *res;
  int status;
  // char ipstr[INET_ADDRSTRLEN] = {'\0'};

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_PASSIVE; 
  hints.ai_family = AF_INET; // AF_INET specifies IPv4
  hints.ai_socktype = SOCK_DGRAM;

  // e.g. "www.example.com" or IP; e.g. "http" or port number
  if ((status = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0) {
    std::cerr << "ERROR: getaddrinfo: " << gai_strerror(status) << std::endl;
    exit(1);
  }

  // buffer to send out to server
  std::ifstream open_file (file_name.c_str(), std::ios::in | std::ios::binary );
  int wc = 0;


  // struct sockaddr_storage clientAddr;
  // socklen_t clientAddrSize = sizeof(clientAddr);

  //build UDP packet
  std::string src_addr = "127.0.0.1"; 
  // int src_port = 1200; 
  std::string dest_addr = ip_addr; 
  // int dest_port = port_num; 
  int cid = 0; 
  uint32_t seq_num = 12345; 
  uint32_t ack_num = 0; 
  bool ack = 0; 
  bool syn = 1; 
  bool fin = 0; 

  int recv = 0;

  std::cout << ">>>>>>>>>>> 1st part of handshake sent" << std::endl;
  TCPheader header(seq_num, ack_num, cid, ack, syn, fin); 
  header.printInfo();
  unsigned char* buf = header.toCharBuffer(); 
  unsigned char hs1_buf[12]; 
  for(int i = 0; i < 12; i++) {
    hs1_buf[i] = buf[i];
  }

  int sent = sendto(sockfd, hs1_buf, sizeof(hs1_buf), 0, res->ai_addr, res->ai_addrlen);

  if (sent > 0)
  {
    // buffer to receive from server
    unsigned char recv_buffer[12];
    while ((recv = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer), 0, res->ai_addr, &res->ai_addrlen) > 0))
    {
      if (recv > 0)
      {
       
        unsigned char* headers_buf = new unsigned char[12]; 
        for(int i = 0; i < 12; i++) {
          headers_buf[i] = recv_buffer[i]; 
        }
        std::cout << ">>>>>>>>>>>>> ACK Response received\n";
        TCPheader recv_header;
        recv_header.parseBuffer(headers_buf);
        //recv_header.printInfo();

        // check flags, we need the SYN/ACK/FIN flags and the connection id
        std::bitset<16> f = recv_header.getFlags();
        f.reset(); 
        f = recv_header.getFlags();

        // received SYN-ACK from server
        if (f[2] == 1 && f[1] == 1)
        {
          syn_ack_established = true;
        }
        // if (open_file.eof() && f[2] == 1)
        // {
        //   //recv_header.printInfo();
        //   unsigned char* fin_buff = new unsigned char[12]; 
        //   seq_num = recv_header.getAckNum();
        //   ack_num = 0; // 0, no ACK flag set
        //   cid = recv_header.getConnectionId();

        //   std::cout << ">>>>>>>>>>>>>>> sending fin\n";
        //   TCPheader fin_header(seq_num, ack_num, cid, 0, 0, 1);
        //   fin_buff = fin_header.toCharBuffer();
        //   fin_header.printInfo();
        //   if (sendto(sockfd, fin_buff, sizeof(fin_buff), 0, res->ai_addr, res->ai_addrlen) < 0)
        //   {
        //     std::cerr << "ERROR: Could not send file\n";
        //     exit(1); 
        //   }

        //   if (recvfrom(sockfd, recv_buffer, sizeof(recv_buffer), 0, res->ai_addr, &res->ai_addrlen) > 0)
        //   {
        //     unsigned char* fin_ack_buf = new unsigned char[12]; 
        //     for(int i = 0; i < 12; i++) {
        //       headers_buf[i] = fin_ack_buf[i]; 
        //     }
        //     TCPheader recv_header;
        //     recv_header.parseBuffer(headers_buf);

        //     // check flags, we need the SYN/ACK/FIN flags and the connection id
        //     std::bitset<16> f = recv_header.getFlags();

        //     // receive FIN-ACK from server
        //     if (f[1] && f[0])
        //     {
        //       unsigned char* close_conn_buff = new unsigned char[12]; 
        //       seq_num = recv_header.getAckNum();
        //       ack_num = recv_header.getSeqNum() + 1; // 0, no ACK flag set
        //       cid = recv_header.getConnectionId();

        //       std::cout << ">>>>>>>>>>>>>>> received fin-ack ACK\n";
        //       TCPheader fin_header(seq_num, ack_num, cid, 0, 0, 1);
        //       close_conn_buff = fin_header.toCharBuffer();
        //       fin_header.printInfo();
        //       if (sendto(sockfd, close_conn_buff, sizeof(close_conn_buff), 0, res->ai_addr, res->ai_addrlen) < 0)
        //       {
        //         std::cerr << "ERROR: Could not send file\n";
        //         exit(1); 
        //       }
        //     }
        //   }
        // }
        if (f[2] == 1 && syn_ack_established && !open_file.eof())
        {
          std::cout << ">>>>>>>>>>>>> 3rd part of handshake sent" << std::endl;
          unsigned char* hs3_buff = new unsigned char[12]; 
          seq_num = recv_header.getAckNum();
          ack_num = recv_header.getSeqNum() + 1; // 1 for now, it needs to be 1 + however much payload we have
          cid = recv_header.getConnectionId();
          TCPheader hs3_header(seq_num, ack_num, cid, 1, 0, 0); 
          hs3_buff = hs3_header.toCharBuffer();
          hs3_header.printInfo();

          char read_buffer[512];
          unsigned char hs3_buffer[524]; 
          for(int i = 0; i < 12; i++) {
             hs3_buffer[i] = hs3_buff[i];
          }  

          // reading the file with offset
          open_file.seekg(read_offset);
          open_file.read(read_buffer, 512);
          read_offset += open_file.gcount();

          // adding data to packet
          int j = 0;
          for(int i = 12; i < 524; i++) {
            hs3_buffer[i] = read_buffer[j];
            j++;
          }

          sent = sendto(sockfd, hs3_buffer, (open_file.gcount() + 12), 0, res->ai_addr, res->ai_addrlen); 
          if(sent > 0) {
            wc += sent;
          }
          else {
            std::cerr << "ERROR: Could not send file\n";
            exit(1); 
          }

          // while(!open_file.eof())
          // {
          //   char read_buffer[512];
          //   unsigned char hs3_buffer[524]; 
          //   for(int i = 0; i < 12; i++) {
          //      hs3_buffer[i] = hs3_buff[i];
          //   }  
          //   // reading the file
          //   open_file.read(read_buffer, 512);
         
          //   // adding data to packet
          //   int j = 0;
          //   for(int i = 12; i < 524; i++) {
          //     hs3_buffer[i] = read_buffer[j];
          //     j++;
          //   }

          //   sent = sendto(sockfd, hs3_buffer, (open_file.gcount() + 12), 0, res->ai_addr, res->ai_addrlen); 
          //   if(sent > 0) {
          //     wc += sent;
          //   }
          //   else {
          //     std::cerr << "ERROR: Could not send file\n";
          //     exit(1); 
          //   }
            // send fin packet when done sending the file

          }

        }
      }
    }
  
  std::cout << "Sent file: " << file_name << std::endl;
  std::cout << "Bytes: " << wc << std::endl;

  open_file.close();
  close(sockfd);

  return 0;
}

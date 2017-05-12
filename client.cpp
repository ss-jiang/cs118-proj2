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

  // set non-blocking to check timeout
  // arg = fcntl(sockfd, F_GETFL, NULL); 
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

  std::ifstream open_file (file_name.c_str(), std::ios::in | std::ios::binary );
  char read_buffer[512];
  int wc = 0;

  //build UDP packet
  std::string src_addr = "127.0.0.1"; 
  int src_port = 1200; 
  std::string dest_addr = ip_addr; 
  int dest_port = port_num; 
  int cid = 0; 
  uint32_t seq_num = 12345; 
  uint32_t ack_num = 0; 
  bool ack = 0; 
  bool syn = 1; 
  bool fin = 0; 

  // send UDP headers
  TCPheader header(seq_num, ack_num, cid, ack, syn, fin); 
  header.printInfo();
  unsigned char* buf = header.toCharBuffer(); 
  TCPheader headers; 
  // reads the buffer and translate it to UDP header
  headers.parseBuffer(buf); 

  while(!open_file.eof())
  {
    open_file.read(read_buffer, 512);
    strcat((char*) buf, read_buffer);

    int sent = sendto(sockfd, buf, (open_file.gcount() + 12), 0, res->ai_addr, res->ai_addrlen);
    if (sent > 0)
    {
      std::cout << "WC: " << sent << std::endl;
      wc += sent;
    }
    if (sent == -1)
    {
      std::cerr << "ERROR: Could not send file\n";
      exit(1); 
    }
  }
  std::cout << "Sent file: " << file_name << std::endl;
  std::cout << "Bytes: " << wc << std::endl;

  open_file.close();
  close(sockfd);

  return 0;
}

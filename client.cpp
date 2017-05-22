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

void pointerToBuffer(unsigned char* buf, unsigned char dest_buf[], int bytes)
{
  for (int i = 0; i < bytes; i++)
  {
    dest_buf[i] = buf[i];
  } 
}

void printStatement (std::string action, uint32_t seq_num, uint32_t ack_num, uint16_t cid, int cwd, int ss_thresh, std::bitset<16> fl) {
  std::cout << action << " " << seq_num << " " << ack_num << " " << cid << " " << cwd << " " << ss_thresh;
  if(fl[2]) {
    std::cout << " ACK";
  }
  if(fl[1]) {
    std::cout << " SYN";
  }
  if(fl[0]) {
    std::cout << " FIN"; 
  }
  std::cout << std::endl; 
}

void printDropStatement(std::string action, uint32_t seq_num, uint32_t ack_num, uint16_t cid, std::bitset<16> fl) {
  std::cout << action << " " << seq_num << " " << ack_num << " " << cid;
  if(fl[2]) {
    std::cout << " ACK";
  }
  if(fl[1]) {
    std::cout << " SYN";
  }
  if(fl[0]) {
    std::cout << " FIN"; 
  }
  std::cout << std::endl; 
}

void printDupStatement (std::string action, uint32_t seq_num, uint32_t ack_num, uint16_t cid, int cwd, int ss_thresh, std::bitset<16> fl) {
  std::cout << action << " " << seq_num << " " << ack_num << " " << cid << " " << cwd << " " << ss_thresh;
  if(fl[2]) {
    std::cout << " ACK";
  }
  if(fl[1]) {
    std::cout << " SYN";
  }
  if(fl[0]) {
    std::cout << " FIN"; 
  }
  std::cout << " DUP" << std::endl; 
}

struct lastSentPacket {
  unsigned char lastSentData[524];
};

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
  bool fin_sent = false;
  bool ack_to_fin_ack = false;

  uint32_t lastSequenceSent = 12346;
  uint32_t totalSeqenceSent = 12346;
  uint32_t lastRecvAckNum = 0; 

  // Check that the port number is in range
  if (port_num < 1024 || port_num > 65535) {
    std::cerr << "ERROR: Port number out of range [1024 - 65535]" << std::endl;
    exit(1);
  }

    // create a socket using UDP
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  // // set non-blocking
  long arg = fcntl(sockfd, F_GETFL, NULL); 
  arg |= O_NONBLOCK; 
  fcntl(sockfd, F_SETFL, arg); 

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

  timeval clientTimeval; 
  clientTimeval.tv_sec = 10; 
  clientTimeval.tv_usec = 0;
  fd_set fdset; 

  //Set receive timeout of 0.5 s
  timeval recvTimeval;
  recvTimeval.tv_sec = 0;
  recvTimeval.tv_usec = 500000;
  fd_set fdsets; 

  // responds to FIN for 2 seconds
  timeval finTimeval; 
  finTimeval.tv_sec = 2; 
  finTimeval.tv_usec = 0;
  fd_set fdset2;
  int fin_rv;

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
  int cwd = 512; 
  int ss_thresh = 10000; 
  // buffer to receive from server
  unsigned char recv_buffer[12];
  //uint32_t prevSeqNum = 0;
  lastSentPacket lastSent;
  int lastSentMode = 0; //0 is none, 1 is lasthead, 2 is lastSent 
  uint32_t lastSequenceNum = 12346;
  TCPheader header(seq_num, ack_num, cid, ack, syn, fin); 
  unsigned char* buf = header.toCharBuffer(); 
  unsigned char hs1_buf[12]; 
  for(int i = 0; i < 12; i++) {
    hs1_buf[i] = buf[i];
  }

  std::bitset<16> fl = header.getFlags();

  printStatement("SEND", seq_num, ack_num, cid, cwd, ss_thresh, fl);

  int sent = sendto(sockfd, hs1_buf, sizeof(hs1_buf), 0, res->ai_addr, res->ai_addrlen);

  if (sent > 0)
  {
    while (1)
    {
      // timeval clientTimeval; 
      // clientTimeval.tv_sec = 10; 
      // clientTimeval.tv_usec = 0;
      // fd_set fdset; 
      FD_ZERO(&fdset);
      FD_SET(sockfd, &fdset);
      int rv;

      //Set receive timeout of 0.5 s
      // timeval recvTimeval;
      // recvTimeval.tv_sec = 0;
      // recvTimeval.tv_usec = 500000;
      // fd_set fdsets; 
      // FD_ZERO(&fdsets);
      // FD_SET(sockfd, &fdsets);
      // int rc;

      // rc = select(sockfd + 1, &fdsets, NULL, NULL, &recvTimeval);
      rv = select(sockfd + 1, &fdset, NULL, NULL, &clientTimeval);
      int recv = recvfrom(sockfd, recv_buffer, 12, 0, res->ai_addr, &res->ai_addrlen);
      if (rv == 0)
      {
        std::cout << "10 SECOND TIMEOUT\n";
        open_file.close();
        close(sockfd);
        exit(1);
      }
      else if (rv < 0)
      {
        break;
      }
      // if (rc == 0) { // retransmission timeout
      //   // congestion avoidance protocol
      //   ss_thresh = cwd / 2; 
      //   cwd = 512;
      //   unsigned char* retransmit_header = new unsigned char[12];
      //   for(int i = 0; i < 12; i++) {
      //     retransmit_header[i] = lastSent.lastSentData[i];
      //   }
      //   TCPheader re_header;
      //   re_header.parseBuffer(retransmit_header); 

      //   unsigned char retransmit_data[512]; 
      //   if(lastSentMode == 1) {
      //     //only retransmitting headers
      //     unsigned char rebuf[12]; 
      //     pointerToBuffer(re_header.toCharBuffer(), rebuf, 12); 
      //     if (sendto(sockfd, rebuf, sizeof(rebuf), 0, res->ai_addr, res->ai_addrlen) < 0)
      //     {
      //       std::cerr << "ERROR: Could not send file\n";
      //       exit(1); 
      //     }
      //   } else {
      //     // retransmitting the data of last sentPacket
      //     if (sendto(sockfd, lastSent.lastSentData, sizeof(lastSent.lastSentData), 0, res->ai_addr, res->ai_addrlen) < 0)
      //     {
      //       std::cerr << "ERROR: Could not send file\n";
      //       exit(1); 
      //     }
      //   }

      //   printDupStatement("SEND", re_header.getSeqNum(), re_header.getAckNum(), re_header.getConnectionId(), cwd, ss_thresh, re_header.getFlags());

      //   continue;
      // } else if (rc < 0) {
      //   break;
      // }

      if (rv > 0 && recv > 0)
      {
        unsigned char* headers_buf = new unsigned char[12]; 
        for(int i = 0; i < 12; i++) {
          headers_buf[i] = recv_buffer[i]; 
        }

        TCPheader recv_header;
        recv_header.parseBuffer(headers_buf);
        recv_header.setSeqNum(recv_header.getSeqNum() % 102401);
        recv_header.setAckNum(recv_header.getAckNum() % 102401);

        // check flags, we need the SYN/ACK/FIN flags and the connection id
        std::bitset<16> f = recv_header.getFlags();

        if (f[0])
        {
          FD_ZERO(&fdset2);
          FD_SET(sockfd, &fdset2);
        }

        printStatement("RECV", recv_header.getSeqNum(), recv_header.getAckNum(), recv_header.getConnectionId(), cwd, ss_thresh, f);

        //if ACK is received, then adjust cwd and ss-thresh accordingly 
        if(f[2]) {
          // std::cout << lastSequenceNum << "--" << recv_header.getAckNum() << std::endl;
          if(cwd < ss_thresh) {
            cwd += 512;
          }
          if(cwd >= ss_thresh) {
            cwd += (512 * 512) / cwd; 
          }
          if(lastSequenceNum != recv_header.getAckNum()){
            continue;
          }
        }

        if(fin_sent && f[0]) 
        { // scenario, sent fin, received only ack, separate case than fin-ack        
          //once recieved FIN-ACK, have 2 seconds before client terminates
          while (1)
          {
            fin_rv = select(sockfd + 1, &fdset2, NULL, NULL, &finTimeval);
            if (ack_to_fin_ack)
            {
              recv = recvfrom(sockfd, recv_buffer, 12, 0, res->ai_addr, &res->ai_addrlen);
              if (recv > 0)
                printStatement("RECV", recv_header.getSeqNum(), recv_header.getAckNum(), recv_header.getConnectionId(), cwd, ss_thresh, f);
            }

            if (fin_rv == 0) 
            {
              //std::cout << "Timed out son" << std::endl;
              open_file.close();
              close(sockfd);
              return 0;
            }
            else if (fin_rv < 0) 
            {
              break;
            } 
            else if (fin_rv > 0 && recv > 0)
            { 
              // During the wait, respond to each incoming FIN with an ACK packet; drop any other non-FIN packet.
              // printStatement("RECV", recv_header.getSeqNum(), recv_header.getAckNum(), recv_header.getConnectionId(), cwd, ss_thresh, f);
              unsigned char* wait_buf = new unsigned char[12];

              for(int i = 0; i < 12; i++) {
                wait_buf[i] = recv_buffer[i]; 
              }

              TCPheader recv_header;
              recv_header.parseBuffer(wait_buf);
              std::bitset<16> waitFlags = recv_header.getFlags();
              if (waitFlags[0])
              {
                // send ACK to FIN packet
                unsigned char* wait_buffer = new unsigned char[12]; 
                seq_num = recv_header.getAckNum();
                ack_num = recv_header.getSeqNum() + 1; // 0, no ACK flag set
                cid = recv_header.getConnectionId();
                TCPheader wait_header(seq_num, ack_num, cid, 1, 0, 0);
                wait_buffer = wait_header.toCharBuffer();

                unsigned char waiting_buf[12]; 
                pointerToBuffer(wait_buffer, waiting_buf, 12);

                // // copy all the data into lastSentPacket array member
                lastSentMode = 1; 
                memset(lastSent.lastSentData, 0, sizeof(lastSent.lastSentData)); // clear the buffer first, don't want leftover bytes
                for(int i = 0; i < 12; i++) {
                    lastSent.lastSentData[i] = waiting_buf[i];
                }

                printStatement("SEND", wait_header.getSeqNum(), wait_header.getAckNum(), wait_header.getConnectionId(), cwd, ss_thresh, wait_header.getFlags());
                if (sendto(sockfd, waiting_buf, sizeof(waiting_buf), 0, res->ai_addr, res->ai_addrlen) < 0)
                {
                  std::cerr << "ERROR: Could not send file\n";
                  exit(1); 
                }
                lastSequenceNum += 1;
                lastSequenceNum %= 102401; 
                ack_to_fin_ack = true;
              }
              else
              {
                printDropStatement("DROP", recv_header.getSeqNum(), recv_header.getAckNum(), recv_header.getConnectionId(), recv_header.getFlags());                
              }
            }
          }
        }

        // received SYN-ACK from server
        if (f[2] && f[1] && !syn_ack_established)
        {
          syn_ack_established = true;
        }

        // if done sending the file, reading is EOF
        if (f[2] && open_file.eof() && !fin_sent)
        {

          unsigned char* fin_buff = new unsigned char[12]; 
          seq_num = recv_header.getAckNum();

          // std::cout << read_offset << std::endl;
          // std::cout << seq_num << "--" << lastSequenceSent << "--" << totalSeqenceSent << std::endl;

          if(lastSequenceSent != seq_num) {
            continue;
          }
          ack_num = 0; // 0, no ACK flag set
          cid = recv_header.getConnectionId();

          TCPheader fin_header(seq_num, ack_num, cid, 0, 0, 1);
          fin_buff = fin_header.toCharBuffer();

          // change fin_sent flag
          fin_sent = true;
          unsigned char fin_buffer[12]; 
          pointerToBuffer(fin_buff, fin_buffer, 12);

          // copy all the data into lastSentHeader array member
          memset(lastSent.lastSentData, 0, sizeof(lastSent.lastSentData)); // clear the buffer first, don't want leftover bytes
          for(int i = 0; i < 12; i++) {
              lastSent.lastSentData[i] = fin_buffer[i];
          }
          lastSentMode = 1;

          printStatement("SEND", fin_header.getSeqNum(), fin_header.getAckNum(), fin_header.getConnectionId(), cwd, ss_thresh, fin_header.getFlags());
          if (sendto(sockfd, fin_buffer, sizeof(fin_buffer), 0, res->ai_addr, res->ai_addrlen) < 0)
          {
            std::cerr << "ERROR: Could not send file\n";
            exit(1); 
          }
          delete(fin_buff);
          lastSequenceNum += 1;
          lastSequenceNum %= 102401;
        }

        // while we get an ack from the server and we havent finished sending the file
        if (f[2] && syn_ack_established && !open_file.eof())
        {
          lastRecvAckNum = (recv_header.getAckNum() % 102401);
          unsigned char* hs3_buff = new unsigned char[12]; 
          seq_num = (recv_header.getAckNum() % 102401);
          ack_num = recv_header.getSeqNum() + f[1]; // 1 for now, it needs to be 1 + however much payload we have
          cid = recv_header.getConnectionId();

          TCPheader hs3_header(seq_num, ack_num, cid, 1, 0, 0);
          if (seq_num < totalSeqenceSent)
          {
            seq_num = lastSequenceSent;
            hs3_header.setSeqNum(lastSequenceSent);
            hs3_header.setAckNum(0);
            hs3_header.setFlags(0, 0, 0);
          }
          hs3_buff = hs3_header.toCharBuffer();

          char read_buffer[512];
          unsigned char hs3_buffer[524]; 
          pointerToBuffer(hs3_buff, hs3_buffer, 12);

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

          if(lastRecvAckNum != hs3_header.getSeqNum()) {
            // congestion avoidance protocol
            ss_thresh = cwd / 2; 
            cwd = 512;

            unsigned char* retransmit_header = new unsigned char[12];
            for(int i = 0; i < 12; i++) {
              retransmit_header[i] = lastSent.lastSentData[i];
            }
            TCPheader re_header;
            re_header.parseBuffer(retransmit_header);
            // retransmitting the data of last sentPacket
            printDupStatement("SEND", re_header.getSeqNum(), re_header.getAckNum(), re_header.getConnectionId(), cwd, ss_thresh, re_header.getFlags());

            if (sendto(sockfd, lastSent.lastSentData, sizeof(lastSent.lastSentData), 0, res->ai_addr, res->ai_addrlen) < 0)
            {
              std::cerr << "ERROR: Could not send file\n";
              exit(1); 
            }
            continue;
          }
          else {
            // copy all the data into lastSentPacket array member
            lastSentMode = 2;
            memset(lastSent.lastSentData, 0, sizeof(lastSent.lastSentData)); // clear the buffer first, don't want leftover bytes
            for(int i = 0; i < 524; i++) {
                lastSent.lastSentData[i] = hs3_buffer[i];
            }

            printStatement("SEND", hs3_header.getSeqNum(), hs3_header.getAckNum(), hs3_header.getConnectionId(), cwd, ss_thresh, hs3_header.getFlags());
            sent = sendto(sockfd, hs3_buffer, (open_file.gcount() + 12), 0, res->ai_addr, res->ai_addrlen); 
            if(sent > 0) {
              wc += sent;
              // lastSequenceSent = seq_num;
              lastSequenceSent += open_file.gcount();
              lastSequenceSent %= 102401;
              totalSeqenceSent += open_file.gcount();
            }
            else {
              std::cerr << "ERROR: Could not send file\n";
              exit(1); 
            }
          }
          lastSequenceNum = hs3_header.getSeqNum() + open_file.gcount();
          lastSequenceNum %= 102401;
          delete(hs3_buff);  

          //if cwnd is larger than our packet size, send another one
          int sent_packet = 512; 
          while(sent_packet < cwd && !open_file.eof()) {
            seq_num += 512;
            seq_num %= 102401;
            TCPheader packet_header(seq_num, 0, cid, 0, 0, 0);
            unsigned char* packet_buf = new unsigned char[12]; 
            packet_buf = packet_header.toCharBuffer(); 
            pointerToBuffer(packet_buf, hs3_buffer, 12); 

            open_file.seekg(read_offset);
            open_file.read(read_buffer, 512);
            read_offset += open_file.gcount();

            int k = 0;
            for(int i = 12; i < 524; i++) {
              hs3_buffer[i] = read_buffer[k];
              k++;
            }

            // copy all the data into lastSentPacket array member
            memset(lastSent.lastSentData, 0, sizeof(lastSent.lastSentData)); // clear the buffer first, don't want leftover bytes
            for(int i = 0; i < 524; i++) {
                lastSent.lastSentData[i] = hs3_buffer[i];
            }
            lastSentMode = 2;

            printStatement("SEND", packet_header.getSeqNum(), packet_header.getAckNum(), packet_header.getConnectionId(), cwd, ss_thresh, packet_header.getFlags());
            sent = sendto(sockfd, hs3_buffer, (open_file.gcount() + 12), 0, res->ai_addr, res->ai_addrlen); 
            if(sent > 0) {
              wc += sent;
              // lastSequenceSent = seq_num;
              lastSequenceSent += open_file.gcount();
              lastSequenceSent %= 102401;
              totalSeqenceSent += open_file.gcount();
            }
            else {
              std::cerr << "ERROR: Could not send file\n";
              exit(1); 
            }
            delete(packet_buf);
            sent_packet += 512;
            lastSequenceNum = packet_header.getSeqNum() + open_file.gcount();     
            lastSequenceNum %= 102401;                  
          }
        }
        delete(headers_buf);
      }
    }
  }

  open_file.close();
  close(sockfd);

  return 0;
}
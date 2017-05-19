#include "TCPheader.h" 
#include <stdio.h>
#include <iostream>

TCPheader::TCPheader(uint32_t seqNum, uint32_t ackNum, uint16_t connectionId, bool ack, bool syn, bool fin) {
	seq_num = seqNum; 
	ack_num = ackNum; 
	connection_id = connectionId; 

	setFlags(ack, syn, fin); 
}

TCPheader::TCPheader() {}

uint32_t TCPheader::getSeqNum() {
	return seq_num; 
}

void TCPheader::setSeqNum(uint32_t seqNum) {
	seq_num = seqNum; 
}

void TCPheader::setFlags(bool ack, bool syn, bool fin) {
	if(ack) {
		flags[2] = 1; 
	}

	if(syn) {
		flags[1] = 1; 
	}

	if(fin) {
		flags[0] = 1;
	}
}

uint32_t TCPheader::getAckNum() {
	return ack_num; 
}

void TCPheader::setAckNum(uint32_t ackNum) {
	ack_num = ackNum;
}

uint16_t TCPheader::getConnectionId() {
	return connection_id;
}

void TCPheader::setConnectionId(uint16_t connectionId) {
	connection_id = connectionId;
}

void TCPheader::printInfo() {
	std::cout << "Sequence Number: " << seq_num << std::endl; 
	std::cout << "Acknowledgment Number: " << ack_num << std::endl;
	std::cout << "Connection ID: " << connection_id << std::endl; 
	std::cout << "ACK, SYN, FIN Flags: " << flags << std::endl;
}

unsigned char* TCPheader::toCharBuffer() {
	unsigned char* buf = new unsigned char[12]; 
	//std::cout << "UDP Headers" << std::endl;
	
	//std::cout << "Sequence Number" << std::endl;
	buf[0] = (seq_num >> 24) & 0xFF; 
	buf[1] = (seq_num >> 16) & 0xFF;
	buf[2] = (seq_num >> 8) & 0xFF; 
	buf[3] = (seq_num) & 0xFF;
	// printf("%x %x %x %x\n", (unsigned char)buf[0],
 //                        (unsigned char)buf[1],
 //                        (unsigned char)buf[2],
 //                        (unsigned char)buf[3]);
  	
  	//std::cout << "Acknowledgment Number" << std::endl;
	buf[4] = (ack_num >> 24) & 0xFF; 
	buf[5] = (ack_num >> 16) & 0xFF;
	buf[6] = (ack_num >> 8) & 0xFF; 
	buf[7] = (ack_num) & 0xFF;
	// printf("%x %x %x %x\n", (unsigned char)buf[4],
 //                        (unsigned char)buf[5],
 //                        (unsigned char)buf[6],
 //                        (unsigned char)buf[7]);
  	
  	//std::cout << "Connection Id" << std::endl;
	buf[8] = (connection_id >> 8) & 0xFF; 
	buf[9] = (connection_id) & 0xFF;
	// printf("%x %x\n", (unsigned char)buf[8],
 //                        (unsigned char)buf[9]);
  	

  	//std::cout << "Flags" << std::endl;
  	buf[10] = (flags.to_ulong() >> 8) & 0xFF; 
	buf[11] = (flags.to_ulong()) & 0xFF;
	// printf("%x %x\n", (unsigned char)buf[10],
 //                        (unsigned char)buf[11]);
	return buf;
}

void TCPheader::parseBuffer(unsigned char* buf) {
	seq_num = ((buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3]);
	ack_num = ((buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + buf[7]);
	connection_id = ((buf[8] << 8) + buf[9]); 
	flags = ((buf[10] << 8) + buf[11]);

	std::cout << seq_num << std::endl;
	std::cout << ack_num << std::endl;
	std::cout << connection_id << std::endl;
	std::cout << flags << std::endl;
}
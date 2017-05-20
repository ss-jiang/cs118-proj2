#include "TCPheader.h" 
#include <stdio.h>
#include <iostream>
#include <arpa/inet.h>

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
	flags = 0;
	if(ack) {
		flags |= 0x4; 
	}

	if(syn) {
		flags |= 0x2; 
	}

	if(fin) {
		flags |= 0x1;
	}
}

uint16_t TCPheader::getFlags() {
	return flags;
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

	uint32_t s_num = htonl(seq_num);
	uint32_t a_num = htonl(ack_num);
	uint32_t c_num = htons(connection_id);
	uint16_t f_num = htons(flags);

	memcpy(&buf[0], &s_num, sizeof(uint32_t));
	memcpy(&buf[4], &a_num, sizeof(uint32_t));
	memcpy(&buf[8], &c_num, sizeof(uint16_t));
	memcpy(&buf[10], &f_num, sizeof(uint16_t));

	return buf;
}

void TCPheader::parseBuffer(unsigned char* buf) {
	uint32_t seq_num_n;
	uint32_t ack_num_n;
	uint32_t connection_id_n; 
	uint32_t flags_n;

	memcpy(&seq_num_n, &buf[0], sizeof(uint32_t));
	memcpy(&ack_num_n, &buf[4], sizeof(uint32_t));
	memcpy(&connection_id_n, &buf[8], sizeof(uint16_t));
	memcpy(&flags_n, &buf[10], sizeof(uint16_t));

	seq_num = ntohl(seq_num_n);
	ack_num = ntohl(ack_num_n);
	connection_id = ntohs(connection_id_n);
	flags = ntohs(flags_n);

	// std::cout << seq_num << std::endl;
	// std::cout << ack_num << std::endl;
	// std::cout << connection_id << std::endl;
	// std::cout << flags << std::endl;
}
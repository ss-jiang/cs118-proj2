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
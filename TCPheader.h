#ifndef TCPheader_H
#define TCPheader_H

#include <stdint.h>
#include <bitset>
#include <cstdint>
#include <cstring>

class TCPheader {
public: 
	uint32_t seq_num; 
	uint32_t ack_num; 
	uint16_t connection_id; 
	std::bitset<16> flags; 
	bool ACK_FLAG = 0; 
	bool FIN_FLAG = 0; 
	bool SYN_FLAG = 0; 

	TCPheader(uint32_t seqNum, uint32_t ackNum, uint16_t connectionId, bool ack, bool syn, bool fin);
	TCPheader(); 
	uint32_t getSeqNum(); 
	void setSeqNum(uint32_t seqNum); 
	uint32_t getAckNum(); 
	void setAckNum(uint32_t ackNum); 
	uint16_t getConnectionId(); 
	void setConnectionId(uint16_t connectionId); 
	void printInfo(); 
	void setFlags(bool ack, bool syn, bool fin); 
	unsigned char* toCharBuffer(); 
	void parseBuffer(unsigned char* buffer); 
}; 

#endif //TCPheader 
# CS118 Project 2

Stan Jiang 104473226
Kevin Wu 304410695

Template for for [CS118 Spring 2017 Project 2](http://web.cs.ucla.edu/classes/spring17/cs118/project-2.html) 

## High Level Specification
### Client

Receive on each iteration of a `while(1)` loop, we create a `TCPheader` object out of it and we used the `TCPheader`'s function to check the flags send in the packet. Depending on the flags set in the packet, we go through and if statement and send the corresponding response to the server. 

### Server

The server has a `while(1)` loop and checks whether anything was received from `recvfrom(...)` on each iteration. A `TCPheader` object is used to parse and determine the flags set in the packet header. If `recvfrom()` returns something greater than `0`, we go into if statement checks and sends back the appropriate `ACK` response. If the sequence number received in the packet does not correspond to the server's expected sequence number in that `connection ID`, the server drops the packet and waits for the client to resend the correct packet.

## List of Problems
### Client
On the client side, the main problem was congestion control. Detecting packet loss was difficult because we have to store the safe-state of the last received ACK packet. Another problem was keeping track of the sequence numbers when we retransmit the packets. We did not know whether to send after every ACK or wait for all the ACKs to come back from the server.

### Server
We did not run into major problems with the server implementation and it should pass the test cases.
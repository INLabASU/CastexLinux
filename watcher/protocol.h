/*
 * protocol.h
 *
 *  Created on: Feb 10, 2018
 *      Author: priori
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <sys/types.h>


// protocol

#define PACKET_MAGIC  		0x87654321
#define PACKET_TYPE_FRAME	0x01
#define PACKET_TYPE_AUDIO	0x02

struct packet_header {

	uint32_t magic;			// magic number
	uint16_t type;			// type of this packet
	uint16_t length;		// length of this packet, including the header

};

struct frame_packet {

	struct packet_header hdr;

	uint32_t fid;				// frame ID
	uint32_t total_length;		// total length of this frame for transmission in byte

	uint32_t pid;				// packet ID
	uint32_t total_packets;		// total number of packets
	uint32_t offset;			// offset of this packet in the frame in byte
	uint32_t length;			// payload length of this packet



	char data[0];				// "pointer" to the payload
};


#define FEEDBACK_MAGIC  		0x12345678
#define FEEDBACK_TYPE_PACKET	0x01
#define FEEDBACK_TYPE_FRAME		0x02


struct feedback_header {

	uint32_t magic;			// magic number
	uint16_t type;			// type of this packet
	uint16_t length;		// length of this packet, including the header

};

struct packet_feedback {

	struct feedback_header hdr;

	uint32_t fid;					// frame ID
	uint32_t pid;					// packet ID

	uint64_t receiver_ts;			// timestamp at receiver side (immediate after receiving the packet)

};

struct frame_feedback {

	struct feedback_header hdr;

	uint32_t fid;					// frame ID
	uint32_t packets_received;		// packet ID

	uint64_t watcher_ts;			// timestamp immediate after renderer wakes up
	uint64_t decrypter_ts;			// timestamp immediate after renderer finishes decryption
	uint64_t decoder_ts;				// timestamp immediate after renderer decodes the frame
	uint64_t renderer_ts;			// timestamp immediate after renderer renders the frame

};





#endif /* PROTOCOL_H_ */

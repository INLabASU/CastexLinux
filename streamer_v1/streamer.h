/*
 * streamer.h
 *
 *  Created on: Jan 20, 2018
 *      Author: priori
 */

#ifndef STREAMER_H_
#define STREAMER_H_

#include <sys/types.h>

#include "protocol.h"

// streamer implementation

// meta data of each packet for statistics and plot
struct packet_meta {

	uint32_t fid;			// frame ID
	uint32_t pid;			// packet ID
	uint32_t state;			// packet state: 0 - init; 1 - packet is sent; 2 - packet feedback is received.
	uint32_t pad;			// just a pad to make this structure aligned in 8 bytes (64 bits)
	uint32_t offset;		// offset of this packet in the frame in byte
	uint32_t length;		// payload length of this packet

	uint64_t sender_ts;		// timestamp immediate after sendto() the packet on the streamer
	uint64_t feedback_ts;	// timestamp immediate after recvfrom() the feedback on the streamer
	uint64_t receiver_ts;	// timestamp immediate after recvfrom() on the watcher
	uint64_t rtt_air;		// round trip time in the air, i.e., feedback_ts - sender_ts


};


// meta data of each frame for statistics and plot
struct fb_meta {

	uint32_t fid;			// frame ID
	uint32_t total_length;	// total length of this frame for transmission in byte
	uint32_t state;			// 0 - init; 1 - frame is sent; 2 - frame feedback is received.
	uint32_t total_packets;	// total number of packets

	uint64_t expected_streamer_ts;	// expected timestamp of the start of the iteration
	uint64_t streamer_ts;	// timestamp at the beginning of each iteration before grabbing the frame
	uint64_t grabber_ts;		// timestamp immediate after grab the frame
	uint64_t encoder_ts;	// timestamp after encoder finishes
	uint64_t encryptor_ts;	// timestamp after encryption finishes
	uint64_t sender_ts;		// timestamp after send_frame()

	uint64_t watcher_ts;	// timestamp when the receiver finishes receiving the frame
	uint64_t decrypter_ts;	// timestamp after encryption finishes
	uint64_t decoder_ts;	// timestamp after decoder finishes on the watcher
	uint64_t renderer_ts;	// timestamp after renderer finishes on the watcher

	struct packet_meta *pmeta;		// meta data of packets of this frame
};



// get timestamp in microsecond resolution
static inline uint64_t get_us() {

	struct timespec spec;

    clock_gettime(CLOCK_MONOTONIC, &spec);

    uint64_t s  = spec.tv_sec;
    uint64_t us = spec.tv_nsec / 1000 + s * 1000 * 1000;

    return us;
}





#endif /* STREAMER_H_ */

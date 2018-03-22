/*
 * watcher.h
 *
 *  Created on: Jan 23, 2018
 *      Author: priori
 */

#ifndef WATCHER_H_
#define WATCHER_H_

#include <sys/types.h>

#include "protocol.h"

// watcher implementation



// get timestamp in microsecond resolution
static inline uint64_t get_us() {

	struct timespec spec;

    clock_gettime(CLOCK_MONOTONIC, &spec);

    uint64_t s  = spec.tv_sec;
    uint64_t us = spec.tv_nsec / 1000 + s * 1000 * 1000;

    return us;
}





// FIFO queue

struct fifo {

	int in;
	int out;
	int size;
	int chunk;

	char *queue;

};

static inline struct fifo *fifo_create(int size, int chunk) {

	struct fifo *fifo = (struct fifo *)malloc(sizeof(struct fifo));

	fifo->in = 0;
	fifo->out = 0;
	fifo->size = size;
	fifo->chunk = chunk;
	fifo->queue = (char *)malloc(size * chunk);

	return fifo;
}

static inline void fifo_destory(struct fifo *fifo) {

	free(fifo->queue);
	free(fifo);

}

static inline int fifo_empty(struct fifo *fifo) {

	return fifo->in == fifo->out;
}

static inline int fifo_full(struct fifo *fifo) {

	return (fifo->in + 1) % fifo->size == fifo->out;
}

static inline char *fifo_next_enqueue(struct fifo *fifo) {

	// NOTE: avoid memory copy
	char *buffer_in = fifo->queue + fifo->in * fifo->chunk;
	return buffer_in;
}

static inline void fifo_enqueue_fast(struct fifo *fifo) {

	__sync_synchronize(); // memory barrier
	// NOTE: avoid memory copy
	fifo->in = (fifo->in + 1) % fifo->size;
}

static inline char *fifo_next_dequeue(struct fifo *fifo) {

	// NOTE: avoid memory copy
	char *buffer_out = fifo->queue + fifo->out * fifo->chunk;
	return buffer_out;
}

static inline void fifo_dequeue_fast(struct fifo *fifo) {

	__sync_synchronize(); // memory barrier
	// NOTE: avoid memory copy
	fifo->out = (fifo->out + 1) % fifo->size;
}

static inline void fifo_enqueue(struct fifo *fifo, char *buffer) {

	// NOTE: memory copy here
	char *buffer_in = fifo_next_enqueue(fifo);
	memcpy(buffer_in, buffer, fifo->chunk);

	fifo_enqueue_fast(fifo);
}

static inline void fifo_dequeue(struct fifo *fifo, char *buffer) {

	// NOTE: memory copy here
	char *buffer_out = fifo_next_dequeue(fifo);
	memcpy(buffer, buffer_out, fifo->chunk);
	__sync_synchronize(); // memory barrier
	fifo_dequeue_fast(fifo);
}







// CAUTION: frame_buffer is a lockless structure!!!
// There are potential data race, but it will not cause data corruption in our case.


// meta data for each slot of the frame buffer
struct fb_meta {

	// the following are only modified at dequeue
	volatile uint32_t fid;				// frame ID

	// the following are only modified at enqueue
	volatile uint32_t flag;
	volatile uint32_t total_length;		// total length of this frame in transmission in byte
	volatile uint32_t total_packet;		// total number of packets in this frame
	volatile uint64_t timestamp;
	volatile uint64_t frame_flag;

	volatile uint8_t bitmap[128]; 		// packet bitmap, assume at most 128 * 8 = 1024 packets in a frame

};

// buffer of frame on the receiver side
// This structure allows out-of-order packet receive (i.e., enqueue) and in-order frame renderer (i.e., dequeue).
struct frame_buffer {

	volatile int init;	// init flag, modified by the enqueue part
	volatile int out;	// frame read out position, modified by the dequeue part
	int size; 			// number of frames the frame buffer can hold
	int chunk;			// size of each frame slot in byte

	uint64_t init_stream_ts;
	uint64_t init_receiver_ts;

	struct fb_meta *meta;
	char *queue;

};

static inline struct frame_buffer *fb_create(int size, int chunk) {

	struct frame_buffer *fb = (struct frame_buffer *)malloc(sizeof(struct frame_buffer));

	fb->init = 1;
	fb->out = 0;
	fb->size = size;
	fb->chunk = chunk;

	fb->meta = (struct fb_meta *)malloc(size * sizeof(struct fb_meta));
	fb->queue = (char *)malloc(size * chunk);

	return fb;
}

static inline void fb_destory(struct frame_buffer *fb) {

	free(fb->meta);
	free(fb->queue);
	free(fb);

}

// set a bit
static inline void bitmap_set(volatile unsigned char *bitmap, unsigned int i) {

	int ii = i / 8;
	int jj = i % 8;

	bitmap[ii] |= (0x01) << jj;

}

// check how many ones are there in a single byte
static inline int bitmap_check_nr_ones_in_one_byte(volatile unsigned char *bitmap) {

	int ret = 0;
	int bb = *bitmap;

	for (int i = 0; i < 8; i++) {

		ret += (bb % 2);
		bb >>= 1;
	}

	return ret;
}

// check how many ones are there in an array of bytes
static inline int bitmap_check_nr_ones(volatile unsigned char *bitmap, unsigned int n) {

	int ii = n / 8;
	int jj = n % 8;

	int ret = 0;

	for (int i = 0; i < ii; i++) {

		unsigned char b1 = bitmap[i];
		ret = (b1 == 0xFF) ? ret + 8 : ret + bitmap_check_nr_ones_in_one_byte(&b1);
	}

	unsigned char mask = (unsigned char)((1 << jj) - 1);
	unsigned char b2 = bitmap[ii] & mask;
	ret += bitmap_check_nr_ones_in_one_byte(&b2);

	return ret;
}



// enqueue a packet to the frame buffer
static inline int fb_enqueue_packet(struct frame_buffer *fb,
		struct frame_packet *packet) {

	// sanity check
	if(packet->offset + packet->length > (unsigned int)fb->chunk) {

		// slot may overflow
		return -1;
	}

	// CAUTION: enqueue operation is lockless!!!

	// here we always read out fb->out first then check the corresponding fbm->fid next


	int out = fb->out;


	if(fb->init) {

		// upon receiving the first packet, associate fid to each slot

		fb->init_stream_ts = packet->timestamp;
		fb->init_receiver_ts = get_us();

		for (int i = 0; i < fb->size; i++) {

			int pos = (out + i) % fb->size;
			struct fb_meta *fbm = fb->meta + pos;
			fbm->fid = packet->fid + i;
			fbm->flag = 0;
			fbm->total_length = 0;
			fbm->total_packet = 0;
			fbm->timestamp = 0;
			memset((void *)fbm->bitmap, 0, sizeof(fbm->bitmap));
		}

		__sync_synchronize(); // memory barrier
		fb->init = 0;

	}

	uint64_t ts = get_us() - fb->init_receiver_ts;

	printf("enqueue: ts = %lld, out = %d, fid = %d, total_length = %d, pid = %d, total_packets = %d, "
			"offset = %d, length = %d, timestamp = %lld, timestamp_rel = %lld\n",
			ts, out, packet->fid, packet->total_length, packet->pid, packet->total_packets,
			packet->offset, packet->length, packet->timestamp, packet->timestamp - fb->init_stream_ts);


	// locate the slot
	int fid_diff = (int)(packet->fid - fb->meta[out].fid);

	if (fid_diff > fb->size || fid_diff < 0)
		return 0;

	int pos = (out + fid_diff) % fb->size;

	// update the meta data
	struct fb_meta *fbm = fb->meta + pos;
	fbm->flag = 0;
	fbm->total_length = packet->total_length;
	fbm->total_packet = packet->total_packets;
	fbm->timestamp = packet->timestamp;

	// update the bitmap
	if (packet->pid < sizeof(uint8_t) * sizeof(fbm->bitmap)) {
		bitmap_set(fbm->bitmap, packet->pid);
	}

	// copy the packet payload to the frame buffer
	char *buffer_in = fb->queue + pos * fb->chunk;
	memcpy(buffer_in + packet->offset, packet->data, packet->length);


	return 1;
}

// dequeue and retrieve the next frame from the frame buffer
static inline int fb_dequeue_frame(struct frame_buffer *fb,
		void *buffer_out, size_t size, struct fb_meta *meta) {

	int ret = 0;

	// CAUTION: dequeue operation is lockless!!!

	// here we always modify fbm->fid first and then modify fb->out next


	if(!fb->init) {

		int pos = fb->out;
		struct fb_meta *fbm = fb->meta + pos;
		size_t frame_size = (size_t)fbm->total_length;

		// copy the frame data to the outside buffer
		char *buffer_in = fb->queue + pos * fb->chunk;
		size = (size < frame_size) ? size : frame_size;
		memcpy(buffer_out, buffer_in, size);

		if (meta != NULL) {

			*meta = *fbm;
		}

		// clear the bitmap
		memset((void *)fbm->bitmap, 0, sizeof(fbm->bitmap));

		// clear the meta structure for future frames
		fbm->fid += fb->size;
		fbm->flag = 0;
		fbm->total_length = 0;
		fbm->total_packet = 0;
		fbm->timestamp = 0;

		__sync_synchronize(); // memory barrier

		// CAUTION: this the only place that fb->out is modified.
		fb->out = (pos + 1) % fb->size;

	}

	return ret;
}


// check whether the next frame is delivered and ready to dequeue
static inline int fb_is_next_frame_ready(struct frame_buffer *fb) {

	// TODO: check the segment bitmap instead of just counting packets.

	if(fb->init)
		return 0;

	int pos = fb->out;
	struct fb_meta *fbm = fb->meta + pos;

	unsigned int n = (unsigned int)bitmap_check_nr_ones(fbm->bitmap, fbm->total_packet);

//	printf("dequeue: %d, %d, %d, %d, %d, %d\n",
//			fb->init, fb->out,
//			fb->meta[pos].fid, fb->meta[pos].total_length,
//			n, fb->meta[pos].total_packet);

	// check the bitmap
	return n == fbm->total_packet && fbm->total_packet != 0;


}

static inline int fb_is_next_frame_ready_on_time(struct frame_buffer *fb,
		uint64_t ts_now, int64_t delay) {

	if(fb->init)
		return 0;

	int pos = fb->out;
	struct fb_meta *fbm = fb->meta + pos;

	int64_t diff_receiver = ts_now - fb->init_receiver_ts;
	int64_t diff_frame = fbm->timestamp - fb->init_stream_ts;

	if (fbm->timestamp != 0 && diff_receiver > (diff_frame + delay)) {

		unsigned int n = (unsigned int)bitmap_check_nr_ones(fbm->bitmap, fbm->total_packet);

		return n > (int)(fbm->total_packet * 0.8) && fbm->total_packet != 0;

		//return 1;

	} else {

		return 0;

	}
}






#endif /* WATCHER_H_ */

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <deque>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <time.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <pthread.h>

#include "watcher.h"

using namespace cv;
using namespace std;






// protocol parameters (shared by streamer and watcher)

// CAUTION: fps is frame per second, interval is in microsecond
int fps = 15;
int period = 1000000 / fps;


const int frame_width = 640;
const int frame_height = 480;
const int frame_depth = 3;

// streamer address is the address that receives feedback
// watcher address is the address that receives actual video frames


char streamer_ip[] = "192.168.43.1";
//char streamer_ip[] = "127.0.0.1";
int streamer_port = 31000;

char watcher_ip[] = "192.168.43.15";
//char watcher_ip[] = "192.168.0.30";
//char watcher_ip[] = "127.0.0.1";
int watcher_port = 32000;






// watcher parameters

// CAUTION: assume 16 * 256 KB is enough!!!
const int frame_buffer_size = 16;
const int frame_buffer_chunk = 256 * 1024;

struct frame_buffer *fb;

const int renderer_wait_limit = 4;


// the frame shown on the screen at receiver side
// CAUTION: opencv only allows the main thread to call imshow()!!!
Mat frame_to_render = Mat(frame_height, frame_width, CV_8UC3);



// FIFO queue to buffer feedback of the whole frame
struct fifo *frame_feedback_fifo;


volatile bool stop_receiver = false;
volatile bool stop_renderer = false;



// the receiver thread merely receive frames and push it to the buffer
void *receiver_thread(void *ptr) {

	int ret = 0;
	int watcher_fd;
	int feedback_fd;
	struct sockaddr_in watcher_addr, cliaddr;
	struct sockaddr_in feedback_addr;
	socklen_t len;

	// socket for receiving from the streamer
	watcher_fd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&watcher_addr, 0, sizeof(watcher_addr));
	watcher_addr.sin_family = AF_INET;
	watcher_addr.sin_addr.s_addr = inet_addr(watcher_ip);
	watcher_addr.sin_port = htons(watcher_port);
	bind(watcher_fd, (struct sockaddr *) &watcher_addr, sizeof(watcher_addr));

	// socket for sending feedback
	feedback_fd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&feedback_addr, 0, sizeof(feedback_addr));
	feedback_addr.sin_family = AF_INET;
	feedback_addr.sin_addr.s_addr = inet_addr(streamer_ip);
	feedback_addr.sin_port = htons(streamer_port);


	unsigned char buffer[64 * 1024];
	struct packet_feedback packet_feedback;

	printf("watcher started.\n");


	while (!stop_receiver) {



		len = sizeof(cliaddr);
		ret = recvfrom(watcher_fd, buffer, sizeof(buffer), 0,
				(struct sockaddr *) &cliaddr, &len);
		//if (ret <= 0)
		//	continue;


		struct frame_packet *packet = (struct frame_packet *)buffer;

		fb_enqueue_packet(fb, packet);

		//printf("watcher: %d : %d, %d / %d, %d\n",
		//		packet->fid, packet->pid, packet->offset, packet->length, ret);

		uint64_t receiver_ts = get_us();

		packet_feedback.hdr.magic = FEEDBACK_MAGIC;
		packet_feedback.hdr.type = FEEDBACK_TYPE_PACKET;
		packet_feedback.hdr.length = sizeof(struct packet_feedback);

		packet_feedback.fid = packet->fid;
		packet_feedback.pid = packet->pid;
		packet_feedback.receiver_ts = receiver_ts;

		// send the feedback packet immediately after a packet is received
		sendto(feedback_fd, &packet_feedback, sizeof(packet_feedback), 0,
				(struct sockaddr *) &feedback_addr, sizeof(feedback_addr));

		// if there is feedback of the whole frame, send it
		while (!fifo_empty(frame_feedback_fifo)) {

			struct frame_feedback *ff = (struct frame_feedback *)
				fifo_next_dequeue(frame_feedback_fifo);

			sendto(feedback_fd, ff, sizeof(frame_feedback), 0,
					(struct sockaddr *) &feedback_addr, sizeof(feedback_addr));

			fifo_dequeue_fast(frame_feedback_fifo);
		}

	}

	printf("watcher stopped.\n");

	return NULL;
}

// the renderer thread pop a frame from buffer, decode it, render it, and send the feedback of a whole frame
void *renderer_thread(void *ptr) {

	// sleep at 1 ms interval
	struct timespec idle;
	idle.tv_sec = 0;
	idle.tv_nsec = 1000 * 1000;


	uchar renderer_buffer[256 * 1024];
	size_t renderer_buffer_size = sizeof(renderer_buffer);


	printf("renderer started.\n");

	int wait = 0;
	struct fb_meta fmeta;



	uint64_t init_ts = get_us();

	for (uint32_t i = 0; !stop_renderer; i++) {

		//cout << i << endl;

		uint64_t ts_start = get_us();

		// if the buffer is empty, just wait for another iteration

		if(fb_dequeue_ready(fb)) {

			fb_dequeue_frame(fb, renderer_buffer, renderer_buffer_size, &fmeta);
			wait = 0;

			int has_frame = fmeta.total_length > 0;
			Mat frame_decoded;

			uint64_t watcher_ts = get_us();

			printf("renderer: out=%d, fid=%4d, total_length%d\n", fb->out, fmeta.fid, fmeta.total_length);

			// TODO: decryption

			uint64_t decrypt_ts = get_us();

			// decode

			if (has_frame) {
				vector<uchar> buf;
				buf.assign(renderer_buffer, renderer_buffer + fmeta.total_length);

				frame_decoded = imdecode(buf, CV_LOAD_IMAGE_COLOR);
			}

			uint64_t decode_ts = get_us();



			// render the frame
			// OpenCV only allow the main thread to create GUI gadgets and show image.
			// So we leave the actual renderer work to the main thread currently.
			if (has_frame) {
				frame_to_render = frame_decoded;
			}

			uint64_t renderer_ts = get_us();

			// construct the feedback for the whole frame
			if (!fifo_full(frame_feedback_fifo)) {

				struct frame_feedback *ff = (struct frame_feedback *)
						fifo_next_enqueue(frame_feedback_fifo);

				ff->hdr.magic = FEEDBACK_MAGIC;
				ff->hdr.type = FEEDBACK_TYPE_FRAME;
				ff->hdr.length = sizeof(struct packet_feedback);

				ff->fid = fmeta.fid;
				ff->packets_received = bitmap_check_nr_ones(fmeta.bitmap, sizeof(fmeta.bitmap), fmeta.total_packet);
				ff->watcher_ts = watcher_ts;
				ff->decrypter_ts = decrypt_ts;
				ff->decoder_ts = decode_ts;
				ff->renderer_ts = renderer_ts;

				fifo_enqueue_fast(frame_feedback_fifo);

			}

		} else {

			printf("renderer: wait %d\n", wait);

			wait++;
			if(wait >= renderer_wait_limit) {

				fb_skip_frame(fb);
				wait = 0;
			}

		}





		uint64_t next_watcher_ts = init_ts + period * (i + 1);
		uint64_t ts = get_us();

		while(ts < next_watcher_ts) {

			nanosleep(&idle, NULL);
			ts = get_us();
		}

	}

	printf("renderer stopped.\n");

	return NULL;
}









// CAUTION: the main thread always show the global variable "frame_to_render"
int main(void) {

	int ret = 0;
    pthread_t t_watcher;
    pthread_t t_renderer;


    fb = fb_create(frame_buffer_size, frame_buffer_chunk);
    frame_feedback_fifo = fifo_create(64, sizeof(struct frame_feedback));


	namedWindow("watcher", CV_WINDOW_AUTOSIZE);

	// start the feedback thread first because it is a receiver from our view
    ret = pthread_create(&t_watcher, NULL, receiver_thread, NULL);
    if(ret != 0)
    {
        fprintf(stderr,"Error - pthread_create() return code: %d\n", ret);
        exit(EXIT_FAILURE);
    }


	ret = pthread_create(&t_renderer, NULL, renderer_thread, NULL);
	if(ret != 0)
	{
		fprintf(stderr,"Error - pthread_create() return code: %d\n", ret);
		exit(EXIT_FAILURE);
	}



    while(1) {



		// show the frame_to_render
		imshow("watcher", frame_to_render);





		if (waitKey(40) == 27) //wait for 'esc' key press for 30 ms. If 'esc' key is pressed, break loop
		{
			cout << "esc key is pressed by user" << endl;
			break;
		}
    }

    stop_receiver = true;
    stop_renderer = true;

    pthread_kill(t_watcher, SIGUSR1);
    pthread_kill(t_renderer, SIGUSR1);

    pthread_join(t_watcher, NULL);
    pthread_join(t_renderer, NULL);


	fb_destory(fb);
	fifo_destory(frame_feedback_fifo);

	return EXIT_SUCCESS;
}



















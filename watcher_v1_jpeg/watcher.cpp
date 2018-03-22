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

// #include "OpenH264Decoder.h"

using namespace cv;
using namespace std;






// protocol parameters (shared by streamer and watcher)

// CAUTION: fps is frame per second, interval is in microsecond
int fps = 30;
int period = 1000000 / fps;


const int frame_width = 640;
const int frame_height = 480;
const int frame_depth = 3;

// streamer address is the address that receives feedback
// watcher address is the address that receives actual video frames


//char streamer_ip[] = "172.20.10.1";
char streamer_ip[] = "192.168.43.1";
//char streamer_ip[] = "192.168.0.25";
//char streamer_ip[] = "127.0.0.1";
int streamer_port = 31000;

//char watcher_ip[] = "172.20.10.14";
char watcher_ip[] = "192.168.43.15";
//char watcher_ip[] = "192.168.0.46";
//char watcher_ip[] = "127.0.0.1";
int watcher_port = 32000;


int codec = 0; // 0 - JPEG; 1 - H.264



// watcher parameters

// CAUTION: assume 16 * 256 KB is enough!!!
const int frame_buffer_size = 256;
const int frame_buffer_chunk = 1024 * 1024;

struct frame_buffer *fb;

const int renderer_delay = 200 * 1000; // in us

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


void dump_memory(unsigned char *ptr, int n) {

	for (int i = 0; i < n; i++) {

		printf("%02X ", ptr[i]);


		if (i % 16 == 15 && i != 0)

			printf("\n");
	}

	printf("\n");


}




// the renderer thread pop a frame from buffer, decode it, render it, and send the feedback of a whole frame
void *renderer_thread(void *ptr) {

	// sleep at 1 ms interval
	struct timespec idle;
	idle.tv_sec = 0;
	idle.tv_nsec = 1000 * 1000;


	uchar renderer_buffer[1024 * 1024];
	size_t renderer_buffer_size = sizeof(renderer_buffer);


	// OpenH264Decoder sd;
	// sd.SetUp();


	printf("renderer started.\n");

	int wait = 0;
	struct fb_meta fmeta;

	vector<uchar> buf;

	uint64_t init_ts = get_us();

	for (uint32_t i = 0; !stop_renderer; i++) {

		//cout << i << endl;

		uint64_t ts_start = get_us();

		// if the buffer is empty, just wait for another iteration

		if(fb_is_next_frame_ready_on_time(fb, ts_start, renderer_delay) || wait > period * 10 / 1000) {
		//if(fb_is_next_frame_ready_on_time(fb, ts_start, renderer_delay)) {

			fb_dequeue_frame(fb, renderer_buffer, renderer_buffer_size, &fmeta);

			uint64_t watcher_ts = get_us();


			// TODO: decryption

			uint64_t decrypt_ts = get_us();

			// decode

			Mat frame_decoded;

			int has_frame = 0;

			if(fmeta.total_length != 0) {

				if (codec == 0) {

					buf.clear();
					buf.assign(renderer_buffer, renderer_buffer + fmeta.total_length);

					try {

					frame_decoded = imdecode(buf, CV_LOAD_IMAGE_COLOR);

					} catch(exception &e) {

						printf("decoder error!!!\n");
					}

					if (frame_decoded.rows > 0 && frame_decoded.cols > 0)
						has_frame = 1;

				} else if (codec == 1) {

					// has_frame = sd.DecodeFrameCV(renderer_buffer,
						// fmeta.total_length, frame_decoded);
				}
			}


			dump_memory(renderer_buffer, 32);


			uint64_t decode_ts = get_us();



			// render the frame
			// OpenCV only allow the main thread to create GUI gadgets and show image.
			// So we leave the actual renderer work to the main thread currently.

			if (has_frame > 0)
				frame_to_render = frame_decoded;

			unsigned int packet_received = (unsigned int)bitmap_check_nr_ones(fmeta.bitmap, fmeta.total_packet);

			printf("renderer: ts = %lu, out = %d, fid = %d, total_length = %d, "
					"packet_received = %d / %d, has_frame = %d\n",
					watcher_ts - fb->init_receiver_ts, fb->out, fmeta.fid, fmeta.total_length,
					packet_received, fmeta.total_packet, has_frame);

			int pos = fb->out;
			struct fb_meta *fbm = fb->meta + pos;
			uint64_t diff_receiver = ts_start - fb->init_receiver_ts;
			uint64_t diff_frame = fbm->timestamp - fb->init_stream_ts;

//			printf("wait = %d, out = %d, diff_receiver = %d, diff_frame = %d, fbm->ts = %d, init_ts = %d, ready = %d\n", wait, fb->out,
//					(int)(diff_receiver),
//					(int)(diff_frame), (int)(fbm->timestamp), (int) fb->init_stream_ts,
//					(diff_receiver > diff_frame + renderer_delay));

			uint64_t renderer_ts = get_us();

			// construct the feedback for the whole frame
			if (!fifo_full(frame_feedback_fifo)) {

				struct frame_feedback *ff = (struct frame_feedback *)
						fifo_next_enqueue(frame_feedback_fifo);

				ff->hdr.magic = FEEDBACK_MAGIC;
				ff->hdr.type = FEEDBACK_TYPE_FRAME;
				ff->hdr.length = sizeof(struct packet_feedback);

				ff->fid = fmeta.fid;
				ff->packets_received = bitmap_check_nr_ones(fmeta.bitmap,
						fmeta.total_packet);
				ff->watcher_ts = watcher_ts;
				ff->decrypter_ts = decrypt_ts;
				ff->decoder_ts = decode_ts;
				ff->renderer_ts = renderer_ts;

				fifo_enqueue_fast(frame_feedback_fifo);

			}

			wait = 0;

		} else {



			wait++;

//			if (wait % 100 == 0) {
//
//				int pos = fb->out;
//				struct fb_meta *fbm = fb->meta + pos;
//
//				uint64_t diff_receiver = ts_start - fb->init_receiver_ts;
//				uint64_t diff_frame = fbm->timestamp - fb->init_stream_ts;
//
//
//				printf("wait = %d, out = %d, diff_receiver = %d, diff_frame = %d, fbm->ts = %d, init_ts = %d, ready = %d\n", wait, fb->out,
//						(int)(diff_receiver),
//						(int)(diff_frame), (int)(fbm->timestamp), (int) fb->init_stream_ts,
//						(diff_receiver > diff_frame + renderer_delay));
//			}

		}

		// sleep at 1 ms
		nanosleep(&idle, NULL);



	}

	// sd.TearDown();

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



















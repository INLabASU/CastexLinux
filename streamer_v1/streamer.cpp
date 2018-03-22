#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>

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

#include "streamer.h"

#include "OpenH264Encoder.h"
#include "OpenH264Decoder.h"

using namespace cv;
using namespace std;

// protocol parameters (shared by streamer and watcher)

// CAUTION: fps is frame per second, interval is in microsecond
int fps = 15;
int period = 1000000 / fps;

const int frame_width_default = 640;
const int frame_height_default = 480;
const int frame_depth_default = 3;

const unsigned int frame_size_default = frame_width_default
		* frame_height_default * frame_depth_default;

// streamer address is the address that receives feedback
// watcher address is the address that receives actual video frames

//char streamer_ip[] = "192.168.43.172";
//char streamer_ip[] = "192.168.0.25";
char streamer_ip[] = "127.0.0.1";
int streamer_port = 31000;

//char watcher_ip[] = "192.168.43.5";
//char watcher_ip[] = "192.168.0.46";
char watcher_ip[] = "127.0.0.1";
int watcher_port = 32000;


// assume the maximum frame size is 256 KB
const unsigned int packet_size_default = 1024;
const unsigned int packet_per_frame_max = 256;


int codec = 0; // 0 - JPEG; 1 - H.264







// streamer parameters

Mat show_frame(frame_height_default, frame_width_default, CV_8UC3);

volatile bool stop_streamer = false;
volatile bool stop_feedback = false;

// for meta data and statistics
unsigned int last_fid = 0;
unsigned int fmeta_size = 1024;
struct fb_meta *fmeta = (struct fb_meta *) calloc(fmeta_size,
		sizeof(struct fb_meta));
struct packet_meta *pmeta = (struct packet_meta *) calloc(
		fmeta_size * packet_per_frame_max, sizeof(struct packet_meta));






// parameter structure for send_frame() and send_frame_seg()

struct send_frame_para {

	uint32_t fid;
	uint32_t frame_size;
	uint32_t packet_size;
	uint32_t total_packets;
	unsigned char **frame_seg;
	uint32_t *seg_sizes;
	int nr_segs;

	uint64_t timestamp;
	int watcher_fd;
	struct sockaddr_in watcher_addr;


};


// packetize the frame segment, attach header, and send to the designated
// address

int send_frame_seg(struct send_frame_para *para, int seg_id,
		uint32_t pid_offset, uint32_t byte_offset) {


	int ret = 0;
	uint32_t fid = para->fid;
	uint32_t packet_id = pid_offset;
	uint32_t seg_size = para->seg_sizes[seg_id];
	uint32_t packet_size = para->packet_size;
	uint32_t nr_packet = seg_size / packet_size
			+ (seg_size % packet_size > 0);
	uint32_t remain = seg_size;
	uint32_t frame_offset = byte_offset;
	uint32_t seg_offset = 0;
	uint32_t len = (remain >= packet_size) ? packet_size : remain;

	char packet_buffer[64 * 1024];

	// UDP packet has the maximum payload size 64K, and here we reserve 64B for the header.
	if (packet_size > 64 * 1023)
		return -1;


	while (seg_offset < seg_size) {

		len = (remain >= packet_size) ? packet_size : remain;

		// fill the packet header
		struct frame_packet *packet = (struct frame_packet *) packet_buffer;

		packet->hdr.magic = PACKET_MAGIC;
		packet->hdr.type = PACKET_TYPE_FRAME;
		packet->hdr.length = sizeof(struct frame_packet) + len;

		packet->fid = fid;
		packet->total_length = para->frame_size;
		packet->pid = packet_id;
		packet->total_packets = para->total_packets;
		packet->offset = frame_offset;
		packet->length = len;

		packet->timestamp =  para->timestamp;

		memcpy(packet->data, para->frame_seg[seg_id] + seg_offset, len);

		ret = sendto(para->watcher_fd, packet, len + sizeof(struct frame_packet), 0,
				(struct sockaddr *) &para->watcher_addr, sizeof(para->watcher_addr));

		uint64_t sender_ts = get_us();

		//printf("%d / %d, %d, %s\n", offset, frame_size, ret, strerror(errno));

		if (ret <= 0) {

			// TODO: handle error
			printf("error in send_frame(): %d, %s\n", ret, strerror(errno));
			return ret;
		}

		// record packet meta data
		if (fid < fmeta_size) {

			struct fb_meta *fm = fmeta + fid;
			struct packet_meta *pm = fm->pmeta + packet_id;

			pm->fid = fid;
			pm->pid = packet_id;
			pm->state = 1;
			pm->offset = frame_offset;
			pm->pad = 0;
			pm->length = len;

			pm->sender_ts = sender_ts;

		}

		packet_id++;
		remain -= len;
		frame_offset += len;
		seg_offset += len;


	}

	return nr_packet;
}


int send_empty_frame(struct send_frame_para *para) {

	struct frame_packet packet_buffer;
	struct frame_packet *packet = &packet_buffer;

	uint32_t fid = para->fid;

	packet->hdr.magic = PACKET_MAGIC;
	packet->hdr.type = PACKET_TYPE_FRAME;
	packet->hdr.length = sizeof(struct frame_packet);

	packet->fid = fid;
	packet->total_length = 0;
	packet->pid = 0;
	packet->total_packets = 1;
	packet->offset = 0;
	packet->length = 0;

	packet->timestamp =  para->timestamp;


	int ret = sendto(para->watcher_fd, packet, sizeof(struct frame_packet), 0,
			(struct sockaddr *) &para->watcher_addr, sizeof(para->watcher_addr));


	uint64_t sender_ts = get_us();

	//printf("%d / %d, %d, %s\n", offset, frame_size, ret, strerror(errno));

	if (ret <= 0) {

		// TODO: handle error
		printf("error in send_frame(): %d, %s\n", ret, strerror(errno));
		return ret;
	}

	// record packet meta data
	if (fid < fmeta_size) {

		struct fb_meta *fm = fmeta + fid;
		struct packet_meta *pm = fm->pmeta;

		pm->fid = fid;
		pm->pid = 0;
		pm->state = 1;
		pm->offset = 0;
		pm->pad = 0;
		pm->length = 0;

		pm->sender_ts = sender_ts;

	}

	return 0;
}

// send one frame in multiple fix-sized packets

int send_frame(struct send_frame_para *para) {

	uint32_t pid_offset = 0;
	uint32_t byte_offset = 0;

	if (para->nr_segs == 0) {

		send_empty_frame(para);
		pid_offset++;

	} else {

		for (int i = 0; i < para->nr_segs; i++) {

			int nr_packets_sent = send_frame_seg(para, i, pid_offset,
					byte_offset);

			pid_offset += nr_packets_sent;
			byte_offset += para->seg_sizes[i];
		}
	}

	return (int) pid_offset;
}

void *streamer_thread(void *ptr) {

	int ret = 0;
	int watcher_fd;
	struct sockaddr_in watcher_addr;

	// prepare regular wake up
	// CAUTION: always sleep 1ms as an small interval
	struct timespec idle;
	idle.tv_sec = 0;
	idle.tv_nsec = 1000 * 1000;

	// prepare the UDP socket
	watcher_fd = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&watcher_addr, 0, sizeof(watcher_addr));
	watcher_addr.sin_family = AF_INET;
	watcher_addr.sin_addr.s_addr = inet_addr(watcher_ip);
	watcher_addr.sin_port = htons(watcher_port);

	// open the camera and prepare to grab frame

	Mat grab_frame;

	VideoCapture cam;
	cam.open(0);

	if (!cam.isOpened()) {
		cerr << "Error opening camera" << endl;
		return NULL;
	}

	cam.grab();
	cam.retrieve(grab_frame);

	cam.set(CV_CAP_PROP_FRAME_WIDTH, frame_width_default);
	cam.set(CV_CAP_PROP_FRAME_HEIGHT, frame_height_default);

	int width = (int) cam.get(CV_CAP_PROP_FRAME_WIDTH);
	int height = (int) cam.get(CV_CAP_PROP_FRAME_HEIGHT);

	cout << "camera is open: " << width << " * " << height << ", "
			<< grab_frame.type() << ": " << CV_8UC3 << endl;
	cout << "frame size in memory: " << grab_frame.step[0] << ", "
			<< grab_frame.step[1] << endl;


	// initialize encoder

	OpenH264Encoder se;
	se.SetUp(fps, width, height, 1000000);
	int max_segs = 128;
	unsigned char *buffer[128];
	int seg_sizes[128];


	vector<uchar> buf;

	printf("streamer started.\n");

	uint64_t init_ts = get_us();

	// streamer loop
	for (uint32_t fid = 0; !stop_streamer; fid++) {

		uint64_t expected_streamer_ts = init_ts + period * fid;

		uint64_t streamer_ts = get_us();

		// TODO: overrun control
		// if streamer_ts > expected_streamer_ts, we should just skip this iteration

		// grab the frame

		cam.grab();
		cam.retrieve(grab_frame);

		// also show the frame at the streamer side
		show_frame = grab_frame;

		uint64_t grabber_ts = get_us();

		// encode

		int frame_size_encoded = 0;
		int nr_segs = 0;

		if (codec == 0) {

			int params[3] = {0};
			params[0] = CV_IMWRITE_JPEG_QUALITY;
			params[1] = 80;

			buf.clear();

			ret = imencode(".jpg", grab_frame, buf, vector<int>(params, params+2));
			if (ret < 0) {

				cout << "error in imencode()!!!" << endl;
			}

			buffer[0] = reinterpret_cast<uchar*> (buf.data());
			seg_sizes[0] = buf.size();
			nr_segs = 1;
			frame_size_encoded = seg_sizes[0];


		} else if (codec == 1) {

			if (fid % fps == 0) {

				se.ForceIFrame();
			}

			int ret_codec = se.EncodeFrameCV(
					grab_frame,
					(unsigned char **)buffer,
					seg_sizes,
					max_segs,
					&frame_size_encoded,
					&nr_segs);


		}

		uint64_t encoder_ts = get_us();

		// TODO: encrypt

		uint64_t encryptor_ts = get_us();


		// record the meta data for the frame

		if (fid < fmeta_size) {

			struct fb_meta *fm = &fmeta[fid];

			fm->fid = fid;
			fm->total_length = frame_size_encoded;
			fm->state = 1;


			fm->expected_streamer_ts = expected_streamer_ts;
			fm->streamer_ts = streamer_ts;
			fm->grabber_ts = grabber_ts;
			fm->encoder_ts = encoder_ts;
			fm->encryptor_ts = encryptor_ts;

			fm->pmeta = pmeta + fid * packet_per_frame_max;
		}
		last_fid = fid;

		// send the frame

		int packet_size = packet_size_default;
		int total_packets = 0;
		for (int i = 0; i < nr_segs; i++) {

			int seg_size = seg_sizes[i];
			total_packets += seg_size / packet_size + (seg_size % packet_size > 0);
		}

		if (total_packets == 0)
			total_packets = 1;

		struct send_frame_para para;
		para.fid = fid;
		para.frame_size = frame_size_encoded;
		para.packet_size = packet_size;
		para.total_packets = total_packets;
		para.frame_seg = buffer;
		para.seg_sizes = (uint32_t *)seg_sizes;
		para.nr_segs = nr_segs;

		para.timestamp = grabber_ts - init_ts;
		para.watcher_fd = watcher_fd;
		para.watcher_addr = watcher_addr;

		ret = send_frame(&para);
		if (ret < 0) {

			cout << "error in send_frame()!!!" << endl;
		}

		uint64_t sender_ts = get_us();

		if (fid < fmeta_size) {

			struct fb_meta *fm = &fmeta[fid];

			fm->total_packets = total_packets;

			fm->sender_ts = sender_ts;
		}

		// pacing for the next frame

		uint64_t next_streamer_ts = init_ts + period * (fid + 1);
		uint64_t ts = get_us();

		printf("streamer: %4d, %d, %d, %s, %d\n", fid,
				frame_size_encoded, ret, strerror(errno), period);

		while (ts < next_streamer_ts) {

			nanosleep(&idle, NULL);
			ts = get_us();
		}

	}

	se.TearDown();

	cam.release();

	printf("streamer finished.\n");

	return 0;
}

void *feedback_thread(void *ptr) {

	// CAUTION: now the feedback thread on the streamer is the server and the watcher is the client

	int ret = 0;
	int feedback_fd;
	struct sockaddr_in feedback_addr, cliaddr;
	socklen_t len;

	feedback_fd = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&feedback_addr, 0, sizeof(feedback_addr));
	feedback_addr.sin_family = AF_INET;
	feedback_addr.sin_addr.s_addr = inet_addr(streamer_ip);
	feedback_addr.sin_port = htons(streamer_port);
	bind(feedback_fd, (struct sockaddr *) &feedback_addr,
			sizeof(feedback_addr));

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000; // 1 ms

	if (setsockopt(feedback_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		printf("error in setsockopt().\n");
	}

	// CAUTION: assume feedback packet is less than 1 KB.
	char feedback_buffer[1024];

	printf("feedback started.\n");
	while (!stop_feedback) {

		// receive the feedback
		// CAUTION: recvfrom block 1ms at most.
		len = sizeof(cliaddr);
		ret = recvfrom(feedback_fd, feedback_buffer, sizeof(feedback_buffer), 0,
				(struct sockaddr *) &cliaddr, &len);
		if (ret <= 0) {

			//printf("%d, %s\n", ret, strerror(errno));
			continue;
		}

		uint64_t feedback_ts = get_us();

		struct feedback_header *header =
				(struct feedback_header *) feedback_buffer;

		if (header->type == FEEDBACK_TYPE_PACKET) {

			// packet feedback
			struct packet_feedback *feedback =
					(struct packet_feedback *) feedback_buffer;

			uint32_t fid = feedback->fid;
			uint32_t pid = feedback->pid;

			if (fid < fmeta_size) {

				struct fb_meta *fm = fmeta + fid;
				struct packet_meta *pm = fm->pmeta + pid;

				pm->receiver_ts = feedback->receiver_ts;
				pm->feedback_ts = feedback_ts;
				pm->rtt_air = feedback_ts - pm->sender_ts;

				pm->state = 2;
			}

		} else if (header->type == FEEDBACK_TYPE_FRAME) {

			// frame feedback
			struct frame_feedback *feedback =
					(struct frame_feedback *) feedback_buffer;

			uint32_t fid = feedback->fid;

			if (fid < fmeta_size) {

				struct fb_meta *fm = fmeta + fid;

				fm->watcher_ts = feedback->watcher_ts;
				fm->decrypter_ts = feedback->decrypter_ts;
				fm->decoder_ts = feedback->decoder_ts;
				fm->renderer_ts = feedback->renderer_ts;

				fm->state = 2;
			}
		}

	}

	printf("feedback finished.\n");

	return NULL;
}

// dump meta data to file

void dump_packet_meta(FILE *fd, struct packet_meta *pm) {

	fprintf(fd, "%u,\t%u,\t%u,\t%u,\t%u,\t%u,\t\t", pm->fid, pm->pid, pm->state,
			pm->pad, pm->offset, pm->length);

	fprintf(fd, "%lu,\t%lu,\t%lu,\t%lu,\t", pm->sender_ts, pm->feedback_ts,
			pm->receiver_ts, pm->rtt_air);

	fprintf(fd, "\n");
}

void dump_frame_meta(FILE *fd, struct fb_meta *fm) {

	fprintf(fd, "%u,\t%u,\t%u,\t%u,\t", fm->fid, fm->total_length, fm->state,
			fm->total_packets);
	fprintf(fd, "%lu,\t%lu,\t%lu,\t%lu,\t%lu,\t%lu,\t\t",
			fm->expected_streamer_ts, fm->streamer_ts, fm->grabber_ts,
			fm->encoder_ts, fm->encryptor_ts, fm->sender_ts);
	fprintf(fd, "%lu,\t%lu,\t%lu,\t%lu,\t\t", fm->watcher_ts, fm->decrypter_ts,
			fm->decoder_ts, fm->renderer_ts);
	fprintf(fd, "\n");

}

void dump_meta() {

	FILE *pdump = fopen("pdump.csv", "w+");
	FILE *fdump = fopen("fdump.csv", "w+");

	for (unsigned int i = 0; i < last_fid && i < fmeta_size; i++) {

		struct fb_meta *fm = fmeta + i;
		dump_frame_meta(fdump, fm);

		for (unsigned int j = 0; j < fm->total_packets; j++) {

			struct packet_meta *pm = fm->pmeta + j;
			dump_packet_meta(pdump, pm);
		}
	}

	fclose(pdump);
	fclose(fdump);

}

int main(void) {

	int ret = 0;
	pthread_t t_streamer;
	pthread_t t_feedback;

	namedWindow("streamer", CV_WINDOW_AUTOSIZE);

	// start the feedback thread first because it is a receiver from our view
	ret = pthread_create(&t_feedback, NULL, feedback_thread, NULL);
	if (ret != 0) {
		fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
		exit(EXIT_FAILURE);
	}

	ret = pthread_create(&t_streamer, NULL, streamer_thread, NULL);
	if (ret != 0) {
		fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
		exit(EXIT_FAILURE);
	}

	for (int k = 0; k < 5000; k++) {
	//while (1) {

		// show the frame
		imshow("streamer", show_frame);

		if (waitKey(40) == 27) //wait for 'esc' key press for 30 ms. If 'esc' key is pressed, break loop
				{
			cout << "esc key is pressed by user" << endl;
			break;
		}
	}

	dump_meta();

	cout << "meta dump finished." << endl;

	stop_streamer = true;
	stop_feedback = true;

	//pthread_kill(t_streamer, SIGUSR1);
	//pthread_kill(t_feedback, SIGUSR1);

	pthread_join(t_streamer, NULL);
	pthread_join(t_feedback, NULL);

	return EXIT_SUCCESS;
}

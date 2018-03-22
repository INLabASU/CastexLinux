IDIR = -I/usr/local/include/
LDIR = 
CC = g++
CFLAGS = $(IDIR) -g -O2 -Wall




LIBS = -lpthread -lopenh264 -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video


TARGET = streamer



OBJ = streamer.o OpenH264Encoder.o OpenH264Decoder.o


%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $< 

%.o: %.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

all : $(TARGET)
	

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^  $(LIBS)

clean:
	rm -f *.o
	rm -f $(TARGET)
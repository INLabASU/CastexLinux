IDIR = -I/usr/local/include/
LDIR = -L/usr/local/lib/
CC = g++
CFLAGS = $(IDIR) $(LDIR) -g -O2 -Wall




LIBS = -lpthread -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_video


TARGET = watcher



OBJ = watcher.o


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
CXXFLAGS =	-O2 -g -Wall -fmessage-length=0

OBJS =		watcher.o

LIBS = -I/usr/local/include/ -L/usr/local/lib -lopencv_core -lopencv_highgui -lpthread

TARGET =	watcher

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

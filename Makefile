CFLAGS= -I $(HOME)/Source/Native/local_install/include/gnuVG/ -I /usr/include/freetype2/ -I $(HOME)/Source/Native/local_install/include

CLIENT_LDFLAGS= -L $(HOME)/Source/Native/local_install/lib/
CLIENT_LIBS= -lGLESv2 -lglfw -lfreetype -lgnuVG -lkamoflage -lm -lvuknobclient -lavahi-client -lavahi-common -pthread

SERVER_LDFLAGS= -L $(HOME)/Source/Native/local_install/lib/
SERVER_LIBS= -lvuknobserver -lm -lavahi-client -lavahi-common

CLIENT_OBJECTS=client.o
SERVER_OBJECTS=server.o

.PHONY: clean
all: vuknob-server vuknob-client

%.o : %.cc
	g++ -g -c $(CFLAGS) $< -o $@

vuknob-client: $(CLIENT_OBJECTS)
	g++ -g -o $@ $(CLIENT_LDFLAGS) $(CLIENT_OBJECTS) $(CLIENT_LIBS)

vuknob-server: $(SERVER_OBJECTS)
	g++ -g -o $@ $(SERVER_LDFLAGS) $(SERVER_OBJECTS) $(SERVER_LIBS) -pthread

clean:
	rm -f gnuvg-test $(CLIENT_OBJECTS) $(SERVER_OBJECTS)

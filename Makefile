
h_files = common.h proto.h pty.h
objs = main.o server.o client.o common.o pty.o proto.o

all: sexpect

${objs}: ${h_files}

sexpect: ${objs}
	$(CC) -o $@ ${objs} -lrt

clean:
	rm -f ${objs} sexpect

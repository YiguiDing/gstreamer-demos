CFLAGS += $(shell pkg-config --cflags gstreamer-1.0)
LDLIBS += $(shell pkg-config --libs gstreamer-1.0)

# 隐式规则（可以不写）
*.o: *.c
	$(CC) $(CFLAGS) -c -o main.o main.c 

main.out: main.o
	$(CC) -o $@ $^ $(LDLIBS)

all: main.out

clean:
	rm -f *.o *.out
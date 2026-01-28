# 编译器设置
CC = gcc
CFLAGS = -Wall -g

CFLAGS += $(shell pkg-config --cflags gstreamer-1.0 gstreamer-audio-1.0)
LDLIBS += $(shell pkg-config --libs gstreamer-1.0 gstreamer-audio-1.0)

# 目标
TARGET = main.out
SRCS = main.c
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

# 编译 .c 文件 (隐式规则)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
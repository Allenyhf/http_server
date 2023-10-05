
CC = g++
CFLAGS = -Wall -g

LDFLAGS = -lpthread

BUILD_DIR = build

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

TARGET = http_server 

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
$(BUILD_DIR):
	mkdir -p $@
clean:
	rm -rf $(BUILD_DIR) $(TARGET)


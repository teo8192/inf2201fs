CC = gcc
CFLAGS = -Wall `pkg-config fuse3 --cflags --libs` -D_FILE_OFFSET_BITS=64

SOURCE = inf2201fs.c
TARGET = inf2201fs

MNT_DIR = mnt

.PHONY: all clean mount umount

all: $(TARGET)

$(TARGET): $(SOURCE)
	gcc -o $@ $(SOURCE) $(CFLAGS)

umnt:
	fusermount -u $(MNT_DIR) 

clean: umnt
	-rm $(TARGET)

mnt: $(TARGET)
	./$(TARGET) $(MNT_DIR)

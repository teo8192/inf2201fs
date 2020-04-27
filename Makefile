CC = gcc
CFLAGS = -Wall `pkg-config fuse3 --cflags --libs` -D_FILE_OFFSET_BITS=64

NAME = inf2201fs
SOURCE = $(NAME).c
TARGET = mount.$(NAME)

MNT_DIR = mnt

PREFIX = /usr/local

.PHONY: all clean mount umount install

all: $(TARGET)

$(TARGET): $(SOURCE)
	gcc -o $@ $(SOURCE) $(CFLAGS)

umnt:
	df -al --type=fuse.$(NAME) && fusermount -u $(MNT_DIR) || echo "Already unmounted"

clean: umnt
	rm -f $(TARGET)

mnt: $(TARGET)
	./$(TARGET) $(MNT_DIR)

install: $(TARGET)
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f $(TARGET) ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/${TARGET}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${TARGET}

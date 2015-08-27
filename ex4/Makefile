CC = g++ -Wall
FLAG = -std=c++11 -D_FILE_OFFSET_BITS=64
FUSE = `pkg-config fuse --cflags --libs`
P =python -c "import os,fcntl; fd = os.open('/tmp/mount/ioctl.log', os.O_RDONLY); fcntl.ioctl(fd, 0); os.close(fd)"

make: CachingFileSystem

CachingFileSystem:
	g++ -Wall -std=c++11 -D_FILE_OFFSET_BITS=64 CachingFileSystem.cpp `pkg-config fuse --cflags --libs` -o CachingFileSystem

tar:
	tar cfv ex4.tar CachingFileSystem.cpp Makefile README

clean:
	rm -rf	CachingFileSystem
	
.PHONY: CachingFileSystem CachingFileSystem.o

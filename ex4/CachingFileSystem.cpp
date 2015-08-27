/*
 * CachingFileSystem.cpp
 *
 *  Created on: 15 April 2015
 *  Author: Netanel Zakay, HUJI, 67808  (Operating Systems 2014-2015).
 */

#define FUSE_USE_VERSION 26
#define NUM_OF_ARGS 5
#define USAGE_ERROR "usage: CachingFileSystem rootdir mountdir numberOfBlocks blockSize"
#define SUCCESS 0
#define FAIL -1

#include <fuse.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <climits>


using namespace std;

struct fuse_operations caching_oper;
char logPath[PATH_MAX];
char* rootPath;
int blockSize, numOfBlocks;

int writeToLog(string str, bool isTimeNeeded);

/*=================================================================================================
 * =========================================CacheDir Class=========================================
 * ================================================================================================
 */

class CacheDir {
public:
	CacheDir() : _dir_ptr(NULL), _entry(NULL), _offset(0)
	{
	}
	DIR* getDirPtr() const {
		return _dir_ptr;
	}

	void setDirPtr(DIR* dirPtr) {
		_dir_ptr = dirPtr;
	}

	const struct dirent* getEntry() const {
		return _entry;
	}

	void setEntry(struct dirent* entry) {
		_entry = entry;
	}

	off_t getOffset() const {
		return _offset;
	}

	void setOffset(off_t offset) {
		_offset = offset;
	}

private:
	DIR *_dir_ptr;
	struct dirent *_entry;
	off_t _offset;


};

/*=================================================================================================
 * ======================================End of CacheDir Class=====================================
 * ================================================================================================
 */

/*=================================================================================================
 * =========================================CacheBlock Class=========================================
 * ================================================================================================
 */

class CacheBlock
{
public:
	CacheBlock(): _path(NULL), _data(NULL), _blockEnumerator(0), _counter(0)
	{
		_data = new char[blockSize + 1];
	}

	~CacheBlock()
	{
		delete[] _data;
		delete[] _path;
	}

	CacheBlock(char* path, char* data, int blockEnumerator)
	{
		_path = new char[strlen(path) + 1];
		strcpy(_path,path);
		_data = new char[blockSize + 1];
		strcpy(_data, data);
		_blockEnumerator = blockEnumerator;
		_counter  = 1;
	}

	CacheBlock(const CacheBlock& b)
	{
		if(&b != this)
		{
			_path = new char[strlen(b.getPath()) + 1]();
			strcpy(_path, b.getPath());
			_data = new char[blockSize + 1]();
			strcpy(_data, b.getData());
			_blockEnumerator = b.getBlockEnumerator();
			_counter  = b.getCounter();
		}
	}

	CacheBlock& operator=(const CacheBlock& b)
	{
		if(&b != this)
		{
			_path = new char[strlen(b.getPath()) + 1]();
			strcpy(_path, b.getPath());
			_data = new char[blockSize + 1]();
			strcpy(_data, b.getData());
			_blockEnumerator = b.getBlockEnumerator();
			_counter  = b.getCounter();
		}
		return *this;
	}

	int getCounter() const {
		return _counter;
	}

	void setCounter(int counter) {
		_counter = counter;
	}

	char* getData() const {
		return _data;
	}

	void setData(char* data) {
		strcpy(_data, data);
	}

	void increaseCount()
	{
		++_counter;
	}

	char* getPath() const {
		return _path;
	}

	void setPath(char* path) {
		if(_path == NULL)
		{
			_path = new char[strlen(path) + 1]();
		}
		strcpy(_path, path);
	}

	int getBlockEnumerator() const {
		return _blockEnumerator;
	}

	void setBlockEnumerator(int blockEnumerator) {
		_blockEnumerator = blockEnumerator;
	}

private:
	char* _path;
	char* _data;
	int _blockEnumerator;
	int _counter;
};

/*=================================================================================================
 * ======================================End of CacheBlock Class=====================================
 * ================================================================================================
 */

vector<CacheBlock*> cacheBlocks;

/**
 * gets the full path of the path provided
 */
static void fullPath(char fpath[PATH_MAX], const char* path)
{
	strcpy(fpath, rootPath);
	strncat(fpath, path, PATH_MAX);
}

/**
 * find out if block exist in our cache
 */
int isInCache(char* path, off_t offset)
{
	int i;
	for(i = 0; i < numOfBlocks; ++i)
	{
		if(!cacheBlocks.at(i)->getPath())
		{
			continue;
		}
		if(!(strcmp(cacheBlocks[i]->getPath(), path)) && (cacheBlocks[i]->getBlockEnumerator() == offset))
		{
			return i;
		}
	}
	return FAIL;
}

/**
 * update block with given data
 */
void updateBlock(CacheBlock *bToOverride, char* path, char* data, int blockEnumerator)
{
	bToOverride->setBlockEnumerator(blockEnumerator);
	bToOverride->setCounter(0);
	bToOverride->setData(data);
	bToOverride->setPath(path);
}

/**
 * gets the relevant error message
 *
 * note that the endl is built in the returning string
 */
string getErrorMsg(string msg, bool isSystemError)
{
	if(isSystemError)
	{
		return "System Error " + msg + "\n";
	}
	else
	{
		return msg + "\n";
	}
}

/**
 * search for block with minimum counter.
 */
int searchMinCounter()
{
	int minCount = INT_MAX, indexOfMin = 0;
	for(int i = 0; i < numOfBlocks; ++i)
	{
		if(cacheBlocks[i]->getCounter() < minCount)
		{
			minCount = cacheBlocks[i]->getCounter();
			indexOfMin = i;
		}
	}
	return indexOfMin;
}

/**
 * handle the writing to log, with given string
 *
 * isTimeNeeded is needed to separate between regular writing, and ioctl writing.
 */
int writeToLog(string str, bool isTimeNeeded) {
	ofstream logStream;
	logStream.open(logPath, ios_base::out | ios::app);
	if(logStream.fail())
	{
		cerr << getErrorMsg("can't open log file", true);
		return FAIL;
	}
	if(isTimeNeeded)
	{
		time_t t = time(nullptr);
		logStream << t << " ";
	}
	logStream << str << endl;
	if (logStream.fail()) {
		cerr << getErrorMsg("Write to log error", true);
		return FAIL;
	}
	logStream.close();
	if(logStream.fail())
	{
		cerr << getErrorMsg("can't close log file", true);
		return FAIL;
	}
	return SUCCESS;
}


/*=================================================================================================
 * ======================================Library Functions=========================================
 * ================================================================================================
 */


/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int caching_getattr(const char *path, struct stat *statbuf)
{
	writeToLog("getattr", true);
	char fpath[PATH_MAX];
	fullPath(fpath, path);
	if(strcmp(fpath, logPath) == 0)
	{
		return -ENOENT;
	}
	if(lstat(fpath, statbuf) != 0) {
		return -errno;
	}
	return 0;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int caching_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi){

	(void) path;
	writeToLog("fgetattr", true);
	if(fstat(fi->fh, statbuf) == -1)
	{
		return -errno;
	}
	return 0;

}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int caching_access(const char *path, int mask)
{
	writeToLog("access", true);
	char fpath[PATH_MAX];
	fullPath(fpath, path);
	if(access(fpath, mask) == -1)
	{
		return -errno;
	}
	return 0;
}


/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.

 * pay attention that the max allowed path is PATH_MAX (in limits.h).
 * if the path is longer, return error.

 * Changed in version 2.2
 */
int caching_open(const char *path, struct fuse_file_info *fi){
	writeToLog("open", true);
	char fpath[PATH_MAX];
	fullPath(fpath, path);
	int fd = open(fpath, fi->flags);
	if (fd == -1)
	{
		return -errno;
	}
	fi->fh = fd;
	return 0;
}


/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes. 
 *
 * Changed in version 2.2
 */
int caching_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi){
	writeToLog("read", true);
	char fpath[PATH_MAX];
	fullPath(fpath, path);
	string data = "";
	off_t advOffset = offset;
	size_t sizeLeftToRead = size;
	size_t sizeToRead = blockSize;
	bool finish = false;
	struct stat s;
	caching_fgetattr(path,&s,fi);
	for (int i = (int)(offset/blockSize); !finish && i <= (int)((offset + ((unsigned int)(offset + size) > s.st_size) ? s.st_size - offset : size)/(blockSize)); ++i)
	{
		int blockIndex;
		if ((blockIndex = isInCache(const_cast<char*>(path), advOffset / blockSize + 1)) == FAIL)
		{
			char buff[blockSize + 1];
			if(sizeLeftToRead < (unsigned int) blockSize)
			{
				sizeToRead = sizeLeftToRead;
			}
			int res = pread(fi->fh, buff, sizeToRead, advOffset);
			if (res == -1)
			{
				cerr << getErrorMsg("Problem in reading", true);
				return -errno;
			}
			sizeLeftToRead -= res;
			if (res < blockSize)
			{
				finish = true;
			}
			buff[res] = '\0';
			if (res == 0)   // no need to read another block
			{
				strcpy(buf, data.c_str());
				return data.size();
			}
			blockIndex = searchMinCounter();
			updateBlock(cacheBlocks.at(blockIndex), const_cast<char*>(path), buff, advOffset / blockSize + 1);
		}
		cacheBlocks[blockIndex]->increaseCount();
		data.append(cacheBlocks[blockIndex]->getData());
		advOffset = offset + data.size();
	}
	strcpy(buf, data.c_str());
	return data.size();
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int caching_flush(const char *path, struct fuse_file_info *fi)
{
	writeToLog("flush", true);
	if(close(dup(fi->fh)) == -1)
	{
		cerr << getErrorMsg("Unable to flush", true);
		return -errno;
	}
	return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int caching_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	writeToLog("release", true);
	if(close(fi->fh) == -1)
	{
		cerr << getErrorMsg("Unable to release", true);
		return -errno;
	}
	return 0;
}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this directory
 *
 * Introduced in version 2.3
 */
int caching_opendir(const char *path, struct fuse_file_info *fi)
{
	writeToLog("opendir", true);
	CacheDir *dir;
	char fpath[PATH_MAX];
	fullPath(fpath, path);
	try
	{
		dir = new CacheDir();  // we'll free this when release is called
	}
	catch (std::bad_alloc& ba)
	{
		cerr << getErrorMsg("bad_alloc caught", true);
	}
	dir->setDirPtr(opendir(fpath));
	if(dir->getDirPtr() == NULL)
	{
		delete dir;
		return -errno;
	}
	fi->fh = (uint64_t) dir;
	return 0;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * Introduced in version 2.3
 */
int caching_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi)
{
	writeToLog("readdir", true);
	CacheDir *dir = (CacheDir*) fi->fh;
	dir->setEntry(readdir(dir->getDirPtr()));
	if (dir->getEntry() == NULL)
	{
		cerr << getErrorMsg("Unable to read directory", true);
		return -errno;
	}
	do
	{
		if (filler(buf, dir->getEntry()->d_name, NULL, 0) != 0)
		{
			return -ENOMEM;
		}
		dir->setEntry(readdir(dir->getDirPtr()));
	} while (dir->getEntry() != NULL);
	return 0;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int caching_releasedir(const char *path, struct fuse_file_info *fi){
	writeToLog("releasedir", true);
	CacheDir *dir = (CacheDir*) fi->fh;
	if (close(fi->fh) == -1)
	{
		cerr << getErrorMsg("Unable to release directory", true);
		delete dir;
		return -errno;
	}
	delete dir;
	return 0;
}

/** Rename a file */
int caching_rename(const char *path, const char *newpath){
	writeToLog("rename", true);
	char fpath[PATH_MAX];
	fullPath(fpath, path);
	char fnewpath[PATH_MAX];
	fullPath(fnewpath, newpath);
	if(rename(fpath, fnewpath) < 0)
	{
		cerr << getErrorMsg("Unable to rename", true);
		return -errno;
	}
	for(auto block : cacheBlocks)
	{
		if(block->getPath()!=NULL && !strcmp(block->getPath(), path))
		{
			block->setPath(const_cast<char*>(newpath));
		}
	}
	return 0;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *caching_init(struct fuse_conn_info *conn){
	writeToLog("init", true);
	cacheBlocks.clear();
	cacheBlocks.resize(numOfBlocks);
	for(int i = 0; i < numOfBlocks; ++i)
	{
		cacheBlocks[i] = new CacheBlock();
	}
	return NULL;
}


/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void caching_destroy(void *userdata){
	writeToLog("destroy", true);
}


/**
 * Ioctl from the FUSE sepc:
 * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
 * 64bit environment.  The size and direction of data is
 * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
 * data will be NULL, for _IOC_WRITE data is out area, for
 * _IOC_READ in area and if both are set in/out area.  In all
 * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
 *
 * However, in our case, this function only needs to print cache table to the log file .
 * 
 * Introduced in version 2.8
 */
int caching_ioctl (const char *, int cmd, void *arg,
		struct fuse_file_info *, unsigned int flags, void *data){
	writeToLog("ioctl", true);
	for(auto block : cacheBlocks)
	{
		stringstream str;
		if(block->getPath() != NULL)
		{
			str << block->getPath();
			str << " ";
			str << block->getBlockEnumerator();
			str << " ";
			str <<	block->getCounter();
			if(writeToLog(str.str(), false) == FAIL)
			{
				//	return FAIL;
			}
		}
	}
	return 0;
}


// Initialise the operations. 
// You are not supposed to change this function.
void init_caching_oper()
{
	caching_oper.getattr = caching_getattr;
	caching_oper.access = caching_access;
	caching_oper.open = caching_open;
	caching_oper.read = caching_read;
	caching_oper.flush = caching_flush;
	caching_oper.release = caching_release;
	caching_oper.opendir = caching_opendir;
	caching_oper.readdir = caching_readdir;
	caching_oper.releasedir = caching_releasedir;
	caching_oper.rename = caching_rename;
	caching_oper.init = caching_init;
	caching_oper.destroy = caching_destroy;
	caching_oper.ioctl = caching_ioctl;
	caching_oper.fgetattr = caching_fgetattr;


	caching_oper.readlink = NULL;
	caching_oper.getdir = NULL;
	caching_oper.mknod = NULL;
	caching_oper.mkdir = NULL;
	caching_oper.unlink = NULL;
	caching_oper.rmdir = NULL;
	caching_oper.symlink = NULL;
	caching_oper.link = NULL;
	caching_oper.chmod = NULL;
	caching_oper.chown = NULL;
	caching_oper.truncate = NULL;
	caching_oper.utime = NULL;
	caching_oper.write = NULL;
	caching_oper.statfs = NULL;
	caching_oper.fsync = NULL;
	caching_oper.setxattr = NULL;
	caching_oper.getxattr = NULL;
	caching_oper.listxattr = NULL;
	caching_oper.removexattr = NULL;
	caching_oper.fsyncdir = NULL;
	caching_oper.create = NULL;
	caching_oper.ftruncate = NULL;
}

//basic main. You need to complete it.
int main(int argc, char* argv[])
{
	if(argc != NUM_OF_ARGS)
	{
		cout << getErrorMsg(USAGE_ERROR, false);
		exit(-1);
	}
	numOfBlocks = atoi(argv[3]);
	blockSize = atoi(argv[4]);
	if(numOfBlocks <= 0 || blockSize <= 0)
	{
		cout << getErrorMsg(USAGE_ERROR, false);
		exit(-1);
	}
	struct stat rootDirStat, mountDirStat;
	stat(argv[1], &rootDirStat);
	stat(argv[2], &mountDirStat);
	if (!S_ISDIR(rootDirStat.st_mode) || !S_ISDIR(mountDirStat.st_mode)) {
		cout << getErrorMsg(USAGE_ERROR, false);
		exit(-1);
	}
	rootPath = realpath(argv[1], NULL);
	if(rootPath == NULL)
	{
		cerr << getErrorMsg("Can't get real path", true);
	}
	strcpy(logPath,rootPath);
	strcat(logPath, "/.filesystem.log");
	ofstream log(logPath, ofstream::out | ofstream::app);
	cacheBlocks.resize(numOfBlocks);
	init_caching_oper();
	argv[1] = argv[2];
	for (int i = 2; i< (argc - 1); i++){
		argv[i] = NULL;
	}
	argv[2] = (char*) "-s";
	argc = 3;

	int fuse_stat = fuse_main(argc, argv, &caching_oper, NULL);
	return fuse_stat;

}




/*=================================================================================================
 * ========================================  TODO List  ===========================================
 * ================================================================================================
 */


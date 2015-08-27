/*
 * srftp.h
 *
 *  Created on: Jun 9, 2015
 *      Author: lior_13
 */

#ifndef SRFTP_H_
#define SRFTP_H_

#include <map>
#include <fstream>

#define MAX_BUFF_SIZE 4096

using namespace std;


typedef struct Client
{
	Client() : _fileContent(""), _fileName(""), _fileSize(0)
	{}
	~Client()
	{
		if(_toWrite.is_open())
		{
			//TODO: do we need to check not complete copy
			_toWrite.close();
		}
	}

	string _fileContent;
	ofstream _output;
	string _fileName;
	int _fileSize;
	ifstream _toWrite;
} Client;

//class Server
//{
//
//public:
//	Server(int maxFileSize);
//	~Server();
//	State getState() const;
//	void setState(State state);
//
//	int getMaxFileSize() const
//	{
//		return _maxFileSize;
//	}
//
//
//	int getServerFd() const {
//		return _serverFd;
//	}

//
//private:
//	int _serverFd;
//	int _maxFileSize;
//};

int readFromClient(int fd);
int createClient();
int disconnectClient(int fd);
int getFileName(int fd);
int getMsgFromClient(int fd, int type);
int getFileData(int fd);
void readFromBuff(int fd, char buff[MAX_BUFF_SIZE], int length);
void* run(void* mySock);
int connectToClient(int port, int fd);

#endif /* SRFTP_H_ */

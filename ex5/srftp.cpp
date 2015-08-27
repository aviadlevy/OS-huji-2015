/*
 * srftp.cpp
 *
 *  Created on: Jun 9, 2015
 *      Author: lior_13
 */

#include <iostream>
#include <unistd.h>
#include "srftp.h"
#include <netinet/in.h>
#include <errno.h>
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <sstream>

#define UNINITIALIZE_SERVER -1
#define SUCCESS 0
#define ERROR 1
#define PROTOCOL 0
#define FAIL -1
#define MAX_CON 5
#define EXIT 1
#define BUFF_SIZE 1024
#define USAGE_MSG "Usage: srftp server-port max-file-size\n"
#define SIZE_CASE 0
#define NAME_CASE 1
#define DATA_CASE 2

std::map<int, Client*> _clients;
long gMaxFileSize;
int gServerFd;


void printSysErrorMessage(string sysFunc, int errorNum)
{
	cerr << "Error: function:" << sysFunc << " errno:" << errorNum << "." << endl;
}

bool isInteger(const std::string & s)
{
	if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;

	char* p ;
	strtol(s.c_str(), &p, 10) ;

	return (*p == 0) ;
}


/*=================================================================================================
 * ==========================================Server Class==========================================
 * ================================================================================================
 */


void freeClients()
{
	if(gServerFd != UNINITIALIZE_SERVER)
	{
		close(gServerFd);
	}
	if (!_clients.empty()) {
		for (auto &client : _clients) {
			if(disconnectClient(client.first) == FAIL)
			{
				pthread_exit(0);
			}
		}
	}
}

int connectToClient(int port)
{
	sockaddr_in mySockAddr = {};
	mySockAddr.sin_family = AF_INET;
	mySockAddr.sin_port = htons(port);
	mySockAddr.sin_addr.s_addr = INADDR_ANY;

	struct hostent *hostPort;
	memset(&mySockAddr, 0, sizeof(struct sockaddr_in));
	char hostName[10000 + 1];
	gethostname(hostName, 10000);
	hostPort = gethostbyname(hostName);

	if(hostPort == NULL)
	{
		printSysErrorMessage("gethostbyname", errno);
		return FAIL;
	}

	mySockAddr.sin_family = hostPort->h_addrtype;
	bcopy(&mySockAddr.sin_addr, hostPort->h_addr, hostPort->h_length);
	mySockAddr.sin_port = htons(port);

	//open the server
	gServerFd = socket(AF_INET, SOCK_STREAM, PROTOCOL);
	if(gServerFd == FAIL)
	{
		printSysErrorMessage("socket", errno);
		return FAIL;
	}

	if(bind(gServerFd, (struct sockaddr *) &mySockAddr, sizeof(mySockAddr)) == FAIL)
	{
		printSysErrorMessage("bind", errno);
		close(gServerFd);
		return FAIL;
	}
	if(listen(gServerFd, MAX_CON) == FAIL)
	{
		printSysErrorMessage("listen", errno);
		return FAIL;
	}
	return SUCCESS;
}

void* run(void* mySock)
{
	readFromClient(*((int*) mySock));
	pthread_exit(mySock);
	return mySock;
}

int readFromClient(int clientFd)
{
	//get file size:
	int resSize = getMsgFromClient(clientFd, SIZE_CASE);
	if(resSize)
	{
		return FAIL;
	}
	if(_clients.at(clientFd)->_fileSize > gMaxFileSize)
	{
		if(send(clientFd, to_string(ERROR).c_str(), sizeof(char), 0) == FAIL)
		{
			printSysErrorMessage("send", errno);
			pthread_exit(0);
		}
	}
	else
	{
		if(send(clientFd, to_string(SUCCESS).c_str(), sizeof(char), 0) == FAIL)
		{
			printSysErrorMessage("send", errno);
			pthread_exit(0);
		}
	}

	// get file name:
	int resName = getMsgFromClient(clientFd, NAME_CASE);
	if(resName)
	{
		return FAIL;
	}
	_clients.at(clientFd)->_output.open(_clients.at(clientFd)->_fileName.c_str(), std::ios::binary | std::ios::out);
	//get content
	getMsgFromClient(clientFd, DATA_CASE);
	_clients.at(clientFd)->_output.close();
	return SUCCESS;
}

int disconnectClient(int fd)
{
	_clients.erase(fd);
	if(close(fd) == FAIL)
	{
		printSysErrorMessage("close", errno);
		return FAIL;
	}
	return SUCCESS;
}


int getMsgFromClient(int fd, int type)
{
	//	int fileSize = 0;
	char* buff = new char[BUFF_SIZE]();
	stringstream tmp;
	int res = recv(fd, buff, BUFF_SIZE, 0);
	if (res == FAIL) {
		printSysErrorMessage("recv", errno);
		return FAIL;
	}
	tmp << buff;
	switch (type)
	{
	case SIZE_CASE:
	{
		_clients.at(fd)->_fileSize = atoi(tmp.str().c_str());
		break;
	}
	case NAME_CASE:
	{
		_clients.at(fd)->_fileName = tmp.str();
		break;
	}
	case DATA_CASE:
	{
		while (res > 0)
		{
			_clients.at(fd)->_output.write(buff, res);
			res = recv(fd, buff, BUFF_SIZE, 0);
		}
		break;
	}
	}
	delete[] buff;
	return SUCCESS;
}


/*=================================================================================================
 * ==============================================Main==============================================
 * ================================================================================================
 */

int main(int argc, char** argv)
{
	//validate that port number is a legal integer
	if (argc != 3 || !isInteger(argv[1]) || atoi(argv[1]) > 65535 || atoi(argv[1]) < 1 ||
			!isInteger(argv[2]) || atoi(argv[2]) <= 0)
	{
		cout << USAGE_MSG;
		exit(EXIT);
	}
	gMaxFileSize = atoi(argv[2]);
	//bind server to socket and start listening on given port
	if (connectToClient(atoi(argv[1])) == FAIL) {
		exit(EXIT);
	}
	int clientFd;
	while((clientFd = accept(gServerFd, NULL, NULL)))
	{
		pthread_t th;
		if(clientFd < 0)
		{
			continue;
		}
		Client* client = new Client;
		_clients[clientFd] = client;
		if(pthread_create(&th, NULL, run, &clientFd) != 0)
		{
			printSysErrorMessage("pthread_create", errno);
			return FAIL;
		}
	}
	freeClients();
	return SUCCESS;
}

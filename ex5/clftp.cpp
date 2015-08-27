/*
 * clftp.cpp
 *
 *  Created on: Jun 9, 2015
 *      Author: lior_13
 */

#include "clftp.h"
#include <netdb.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <sstream>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

using namespace std;

#define OUR_PROTOCOL 0
#define EXIT 1
#define FAIL -1
#define SUCCESS 0
#define ERROR 1
#define BUFF_SIZE 1024
#define USAGE_MSG "usage : clftp <server-port> <server-hostname> <file-to-transfer> <filename-in-server>\n"
#define TOO_BIG "Transmission failed: too big file\n"


struct hostent* server = NULL;

void printSysErrorMessage(string sysFunc, int errorNum)
{
	cerr << "Error: function:" << sysFunc << " errno:" << errorNum << "." << endl;
}

bool isInteger(const std::string & s)
{
	if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false ;

	char * p ;
	strtol(s.c_str(), &p, 10) ;

	return (*p == 0) ;
}

/*=================================================================================================
 * ==========================================Client Class==========================================
 * ================================================================================================
 */


Client::~Client()
{

}

int Client::connectToServer(int serverPort, char* serverHost)
{
	server = gethostbyname(serverHost);
	sockaddr_in serverAddr;
	if(server == NULL)
	{
		printSysErrorMessage("gethostbyname", errno);
		return FAIL;
	}
	_server = socket(AF_INET, SOCK_STREAM, OUR_PROTOCOL);

	if(_server == FAIL)
	{
		printSysErrorMessage("socket", errno);
		return FAIL;
	}

	bzero((char *) &serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
			(char *)&serverAddr.sin_addr.s_addr, server->h_length);
	serverAddr.sin_port = htons(serverPort);
	int res = connect(_server, ((struct sockaddr*)&serverAddr), sizeof(serverAddr));

	if (res < 0) {
		printSysErrorMessage("connect", errno);
		return FAIL;
	}
	return SUCCESS;
}

int Client::writeToServer(string filePath, string serverName)
{
	stringstream sStream;
	int size;
	_toWrite.open(filePath.c_str(), ios::binary | ios::ate);
	if (!_toWrite.good())
	{
		cerr << "error\n";
		return FAIL;
	}
	size = _toWrite.tellg();
	if (size < 0)
	{
		cerr << "error\n";
		return FAIL;

	}
	_toWrite.seekg(0, ios::beg);
	if (!this->_toWrite.good()) {
		cerr << "error\n";
		return FAIL;
	}
	// send the size
	sendMsgToServer(to_string(size).c_str(), to_string(size).length());

	if(reciveMsg() == FAIL)
	{
		cout << TOO_BIG;
		exit(EXIT);
	}

	//send file name
	sendMsgToServer(serverName.c_str(), serverName.length());

	char* buffer = new char[BUFF_SIZE];
	if (buffer==NULL)
	{
		cerr << "error\n";
		exit(EXIT);
	}
	int toSend = size;

	while (toSend >= BUFF_SIZE){
		_toWrite.read(buffer,BUFF_SIZE);
		sendMsgToServer(buffer , BUFF_SIZE);
		toSend -= BUFF_SIZE;
	}
	//complete the loop
	if (toSend != 0){
		_toWrite.read(buffer ,toSend);
		sendMsgToServer(buffer , toSend);
	}
	delete[] buffer;
	return SUCCESS;
}

void Client::sendMsgToServer(const char* msg, int msgLength)
{
	int sent = 0;
	sent = send(_server, msg, msgLength, 0);
	if (sent == FAIL)
	{
		printSysErrorMessage("send", errno);
		exit(EXIT);
	}

}

int Client::reciveMsg()
{
	char* buff = new char[1];
	int res = recv(_server, buff, sizeof(char), 0);
	if (res == FAIL) {
		printSysErrorMessage("recv", errno);
	}
	if(atoi(buff) != SUCCESS){
		return FAIL;
	}
	return SUCCESS;
}

/*=================================================================================================
 * ==============================================Main==============================================
 * ================================================================================================
 */

int main(int argc, char** argv)
{
	Client client;
	if(argc != 5 || !isInteger(argv[1]) || atoi(argv[1]) > 65535 || atoi(argv[1]) < 1)
	{
		cout << USAGE_MSG;
		exit(EXIT);
	}
//	timeval before, after, diff;  // Time stamps
//	timezone defalutTimeZone;  // TimeZone
//	if(gettimeofday(&before, &defalutTimeZone) == FAIL)
//	{
//		return FAIL;
//	}
	if (client.connectToServer(atoi(argv[1]), argv[2]) == FAIL) {
		exit(EXIT);
	}
	if (client.writeToServer(argv[3], argv[4]) == FAIL) {
		exit(EXIT);
	}
//	if (gettimeofday(&after,&defalutTimeZone) == FAIL)
//	{
//		return FAIL;
//	}
//	timersub(&after,&before,&diff);
//	cout << "before: " << before.tv_usec << " after: " << after.tv_usec << " diff: " << diff.tv_usec << endl;
	return SUCCESS;
}




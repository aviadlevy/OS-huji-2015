/*
 * clftp.h
 *
 *  Created on: Jun 9, 2015
 *      Author: lior_13
 */

#ifndef CLFTP_H_
#define CLFTP_H_

#include <string>
#include <fstream>

class Client {
public:
//	Client();
	~Client();
	int connectToServer(int serverPort, char* serverHost);
	int writeToServer(std::string filePath, std::string serverName);
	void sendMsgToServer(const char* msg, int msgLength);
	int reciveMsg();

private:
	int _server;
	std::ifstream _toWrite;
};

#endif /* CLFTP_H_ */

#pragma warning(disable : 4996)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits>
#include <iostream>
#include <thread>

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2tcpip.h>

//#include <sys/socket.h>
//#include <arpa/inet.h>
//#include <unistd.h> /* Needed for close() */
//#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
//typedef int SOCKET;

#ifdef _WIN32
/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif

#pragma comment (lib, "Ws2_32.lib")
#else
/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h> /* Needed for close() */
#include <thread>

#endif

struct udpMessage
{
	unsigned char nVersion;
	unsigned char nType;
	unsigned short nMsgLen;
	unsigned long lSeqNum;
	char chMsg[1000];
};

class ClientUDP {
public: // Methods
	static constexpr unsigned int kMaxMessageLength{ 1000 };
	void startClient(int portno, const char* server_address);
	void promptForCommand();
	bool parseCommand(const char command[kMaxMessageLength]);
	void sendMessage(udpMessage buffer);
	void receiveMessage();
	void handleReceivedMessage(udpMessage message);
	void closeSockets();
	void createWorkers();
private: // Methods
	int sockInit();
	int sockQuit();
	int sockClose(SOCKET sock);
private: // Members
	unsigned int versionNum{ 0 };
	int sockfd;
	struct sockaddr_in serv_addr;
	bool shutDown{ false };
};

constexpr unsigned int ClientUDP::kMaxMessageLength;

int ClientUDP::sockInit()
{
#ifdef _WIN32
	WSADATA wsa_data;
	return WSAStartup(MAKEWORD(1, 1), &wsa_data);
#else
	return 0;
#endif
}

int ClientUDP::sockQuit()
{
#ifdef _WIN32
	return WSACleanup();
#else
	return 0;
#endif
}

int ClientUDP::sockClose(SOCKET sock)
{

	int status = 0;

#ifdef _WIN32
	status = shutdown(sock, SD_BOTH);
	if (status == 0)
	{
		status = closesocket(sock);
	}
#else
	status = shutdown(sock, SHUT_RDWR);
	if (status == 0)
	{
		status = close(sock);
	}
#endif

	return status;

}

void error(const char* msg)
{
	perror(msg);

	exit(0);
}

void ClientUDP::sendMessage(udpMessage buffer)
{
	int n;
	// Set the version number
	buffer.nVersion = versionNum;

	// Sends the message to the server
	n = sendto(sockfd, (char*)&buffer, sizeof(buffer), 0, (struct sockaddr*) & serv_addr, sizeof(serv_addr));
	if (n < 0)
	{
		error("ERROR writing to socket");
	}
}

void ClientUDP::handleReceivedMessage(udpMessage message)
{
	// Convert the necessary values to host order btyes
	message.nMsgLen = ntohs(message.nMsgLen);
	message.lSeqNum = ntohl(message.lSeqNum);

	// Print out the received message
	printf("Received Msg Type: %d, Seq: %ld, Msg: %.*s\n", message.nType, message.lSeqNum, message.nMsgLen, message.chMsg);
}

void ClientUDP::receiveMessage()
{
	// Loop until shutdown
	while (!shutDown)
	{
		int n;
		socklen_t fromlen = 0;
		struct sockaddr from {};
		memset((char*)& from, 0, sizeof(sockaddr));
		udpMessage response{};

		fromlen = sizeof(serv_addr);
		n = recvfrom(sockfd, (char*)&response, sizeof(response), 0, (sockaddr*)& from, &fromlen);

		// test port
		if (n < 0)
		{
			error("ERROR reading from socket");
		}
		
		else
		{
			if (n > 0)
			{
				ClientUDP::handleReceivedMessage(response);
			}
		}
	}
}

void ClientUDP::startClient(int portno, const char* server_address)
{
	struct hostent* server;

	sockInit();

	// Create socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	// Get server address
	server = gethostbyname(server_address);

	if (server == NULL)
	{
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);  
	}
	// Zero out serv_addr variable
	memset((char*)& serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;

	memmove((char*)& serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);

	serv_addr.sin_port = htons(portno);

	// If a connection is desired
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
       error("ERROR connecting");
}

void ClientUDP::promptForCommand()
{
	int commandNum;
	char command[kMaxMessageLength];

	// Keep prompting until shutdown
	while (!shutDown)
	{
		std::cin.clear();

		// Prompt the user for a command
		std::cout << "Please enter command: " << std::endl;

		// Retrieve input from user
		fgets(command, 1023, stdin);

		// Strip the new line
		command[strcspn(command, "\n")] = 0;

		// Parse the passed in command
		parseCommand(command);
	}
	//shutdown(sockfd, SHUT_RDWR);
	sockClose(sockfd);
}

bool ClientUDP::parseCommand(const char command[kMaxMessageLength])
{
	enum CommandType { setVersion, setType, setSequence, setMessage, Quit, None };
	CommandType commandType{ CommandType::None };

	// Returns first token
	char commandToParse[kMaxMessageLength]{ 0 };
	strcpy(commandToParse, command);
	char* token = strtok(commandToParse, " ");

	// Variables regarding sending a message
	udpMessage message{};

	// Keep printing tokens 
	while (token != nullptr)
	{
		// Judge the command type 
		if (commandType == CommandType::None)
		{
			if (strlen(token) != 1)
			{
				std::cout << "invalid command!" << std::endl;
				return false;
			}

			switch (token[0])
			{
			case 'v':
				commandType = CommandType::setVersion;
				break;
			case 't':
				commandType = CommandType::setType;
				break;
			case 'q':
				shutDown = true;
				return true;
			default:
				std::cout << "invalid arguments!" << std::endl;
				return false;
			}
		}
		// Parse the version 
		else if (commandType == CommandType::setVersion)
		{
			char* pEnd;
			int tempVersionNum = strtol(token, &pEnd, 10);
			if (tempVersionNum < 0)
			{
				std::cout << "Error: wrong version!" << std::endl;
				return false;
			}
			versionNum = tempVersionNum;
			std::cout << "version is: " << versionNum << std::endl;
			return true;
		}
		else if (commandType == CommandType::setType)
		{
			char* pEnd;
			int tempMType = strtol(token, &pEnd, 10);
			if (tempMType < 0)
			{
				std::cout << "Error: wrong type!" << std::endl;
				return false;
			}
			message.nType = tempMType;
			commandType = CommandType::setSequence;
		}
		else if (commandType == CommandType::setSequence)
		{
			char* pEnd;
			int tempMSeq = strtol(token, &pEnd, 10);
			if (tempMSeq < 0)
			{
				std::cout << "Error: wrong sequence number!" << std::endl;
				return false;
			}
			message.lSeqNum = tempMSeq;
			commandType = CommandType::setMessage;
		}
		else if (commandType == CommandType::setMessage)
		{
			message.nMsgLen = strlen(token);

			// Limit client to sending out the message length
			if (message.nMsgLen > kMaxMessageLength)
			{
				message.nMsgLen = kMaxMessageLength;
			}

			// Copy the desired amount of characters
			strncpy(message.chMsg, token, message.nMsgLen);

			// Convert all values to network order
			message.nMsgLen = htons(message.nMsgLen);
			message.lSeqNum = htonl(message.lSeqNum);

			sendMessage(message);
			return true;
		}

		// If we are now setting the message, then grab the rest of the char as the message
		if (commandType == CommandType::setMessage)
		{
			token = strtok(nullptr, "\0");
		}
		// Otherwise delimit by spaces to grab the different arguments
		else
		{
			token = strtok(nullptr, " ");
		}
	}
	std::cout << "invalid command!" << std::endl;
	return false;
}

void ClientUDP::createWorkers()
{
	// Create necessary worker threads
	std::thread receiveMessagesThread(&ClientUDP::receiveMessage, this);
	std::thread promptUser(&ClientUDP::promptForCommand, this);

	// Wait for threads to finish
	receiveMessagesThread.join();
	promptUser.join();

	// Close the socket
	closeSockets();
}

void ClientUDP::closeSockets()
{
	sockClose(sockfd);
	sockQuit();

#ifdef _WIN32
	std::cin.get();
#endif
}

int main(int argc, char* argv[])
{
	int portno;

	if (argc < 3) {
		fprintf(stderr, "usage %s hostname port\n", argv[0]);
		exit(0);
	}

	// Store the port number the client wants to connect to
	portno = atoi(argv[2]);

	// Start the server
	ClientUDP udp_client;
	udp_client.startClient(portno, argv[1]);
	udp_client.createWorkers();
}
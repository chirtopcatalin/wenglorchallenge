#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h>

unsigned short int port = 5555;
PCWSTR address = L"127.0.0.1";
std::string message = "Hello world!";
char receiveBuffer[1000];
int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA data;
	std::cout << wVersionRequested << std::endl<<std::endl;
	int status = WSAStartup(wVersionRequested, &data);
	if (status != 0) {
		std::cout << "winsock dll not found"<<std::endl;
	}
	else {
		std::cout << "winsock found"<<std::endl;
		std::cout <<"STATUS: " << data.szSystemStatus << std::endl;
	}
	SOCKET mySocket = INVALID_SOCKET;
	mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (mySocket == INVALID_SOCKET) {
		std::cout << "\n\nsocket initialization failed with error code: " << WSAGetLastError << std::endl;
		WSACleanup();
	}
	else {
		std::cout <<"\n\nsocket intialized successfully!" << std::endl;
	}

	sockaddr_in bindInformation;
	bindInformation.sin_port = htons(port);
	bindInformation.sin_family = AF_INET;
	InetPton(AF_INET, address, &bindInformation.sin_addr.S_un.S_addr);
	int bindStatus = bind(mySocket, (sockaddr*)&bindInformation, sizeof(bindInformation));
	if (bindStatus == SOCKET_ERROR) {
		std::cout << "socket binding failed with error code: " << WSAGetLastError() << std::endl;
		closesocket(mySocket);
		WSACleanup();
		return 0;
	}
	else {
		std::cout << "socket successfully bound to port "<< port << std::endl;
	}


	if (listen(mySocket, 4) != 0) {
		std::cout << "socket can't be put in listening mode "<<WSAGetLastError() << std::endl;
	}
	else {
		std::cout << "socket listening for connections" << std::endl;
	}
	SOCKET acceptSocket = accept(mySocket, NULL, NULL);
	if (acceptSocket == INVALID_SOCKET) {
		std::cout << "couldn't accept: "<< WSAGetLastError() << std::endl;
		WSACleanup();
	}
	else {
		std::cout << "accepted" << std::endl;
	}

	closesocket(mySocket);
	WSACleanup();
}
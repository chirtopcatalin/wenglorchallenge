#include <iostream>
#include <any>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h>
#include <chrono>
#include <sstream>
#include <vector>
#include <map>
#include <string_view>

unsigned short int port = 8080;
PCWSTR address = L"127.0.0.1";
std::string websiteRoot = "G:/download chrome/poc/public"; //no "/" at the end !!

// Function that gets the extension of requested file and returns the content type to be put in the http header
// c++ 17 feature used: string_view - more memory-efficient because the value of the string does not get modified
std::string GetContentTypeFromExtension(const std::string_view& filePath) {
	std::any dotPos = filePath.find_last_of(".");
	if (std::any_cast<size_t>(dotPos) != std::string::npos) {
		std::string_view extension = filePath.substr(std::any_cast<size_t>(dotPos) + 1);

		std::map<std::string_view, std::string> mp;
		mp["html"] = "text/html";
		mp["css"] = "text/css";
		mp["js"] = "application/javascript";
		mp["jpg"] = "image/jpeg";
		mp["jpeg"] = "image/jpeg";
		mp["png"] = "image/png";
		mp["gif"] = "image/gif";
		mp["ico"] = "image/x-icon";
		mp["svg"] = "image/svg+xml";

		//use of structured bindings
		for (const auto& [key, value] : mp) {
			if (key == extension) {
				return value;
			}
		}
	}

	return "application/octet-stream";
}

void sendResource(SOCKET clientSocket, std::string requestedResourcePath) {
	std::ifstream t;
	std::string contentType;
	std::string fullPath = websiteRoot + requestedResourcePath;

	if (!std::ifstream(fullPath)) {
		std::string notFoundContent = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>";
		contentType = "text/html";
		std::string header = "HTTP/1.1 404 Not Found\r\nContent-Type: " + contentType + "\r\nContent-Length: " + std::to_string(notFoundContent.length()) + "\r\n\r\n";
		send(clientSocket, header.c_str(), header.length(), 0);
		send(clientSocket, notFoundContent.c_str(), notFoundContent.length(), 0);
		std::cout << requestedResourcePath << " not found\n";
		return;
	}
	contentType = GetContentTypeFromExtension(requestedResourcePath);
	t.open(fullPath);
	std::stringstream resourceContent;
	resourceContent << t.rdbuf();

	std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType + "\r\nContent-Length: " + std::to_string(resourceContent.str().length()) + "\r\n\r\n";

	send(clientSocket, header.c_str(), header.length(), 0);
	send(clientSocket, resourceContent.str().c_str(), resourceContent.str().length(), 0);
	std::cout << requestedResourcePath << " sent\n\n";
}

SOCKET InitializeSocket() {
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA data;
	std::cout << wVersionRequested << std::endl << std::endl;
	int status = WSAStartup(wVersionRequested, &data);
	if (status != 0) {
		std::cout << "winsock dll not found" << std::endl;
	}
	else {
		std::cout << "winsock found" << std::endl;
		std::cout << "STATUS: " << data.szSystemStatus << std::endl;
	}
	SOCKET mySocket = INVALID_SOCKET;
	mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (mySocket == INVALID_SOCKET) {
		std::cout << "\n\nsocket initialization failed with error code: " << WSAGetLastError << std::endl;
		WSACleanup();
	}
	else {
		std::cout << "\n\nsocket intialized successfully!" << std::endl;
	}
	return mySocket;
}

bool BindSocket(SOCKET mySocket) {
	sockaddr_in bindInformation;
	bindInformation.sin_port = htons(port);
	bindInformation.sin_family = AF_INET;
	InetPton(AF_INET, address, &bindInformation.sin_addr.S_un.S_addr);
	int bindStatus = bind(mySocket, (sockaddr*)&bindInformation, sizeof(bindInformation));
	if (bindStatus == SOCKET_ERROR) {
		std::cout << "socket binding failed with error code: " << WSAGetLastError() << std::endl;
		closesocket(mySocket);
		WSACleanup();
		return false;
	}
	else {
		std::cout << "socket successfully bound to port " << port << std::endl;
		return true;
	}
}

bool PlaceSocketInListeningMode(SOCKET mySocket) {
	if (listen(mySocket, 2) != 0) {
		std::cout << "socket can't be put in listening mode " << WSAGetLastError() << std::endl;
		return false;
	}
	else {
		std::cout << "socket listening for connections" << std::endl;
		return true;
	}
}

int findFirstOccurrenceAfterIndex(const char charArray[], char targetChar, int startIndex) {
	int length = strlen(charArray);

	for (int i = startIndex; i < length; i++) {
		if (charArray[i] == targetChar) {
			return i;
		}
	}
	return -1;
}

std::string getRequestedResourceFromRequest(char dataReceivedFromRequest[]) {
	std::string requestedResource;

	char* startOfRequestLine = strstr(dataReceivedFromRequest, "GET ");
	if (startOfRequestLine != nullptr) {
		char* endOfRequestLine = strstr(startOfRequestLine, " HTTP/1.1");
		if (endOfRequestLine != nullptr) {
			requestedResource.assign(startOfRequestLine + 4, endOfRequestLine);
			std::cout << "Requested Resource: " << requestedResource << std::endl;
		}
	}
	return requestedResource;
}

int main()
{
	SOCKET mySocket = InitializeSocket();
	if (BindSocket(mySocket) == 0) return 0;
	if (PlaceSocketInListeningMode(mySocket) == 0) return 0;

	while (true) {
		SOCKET acceptSocket = accept(mySocket, NULL, NULL);
		if (acceptSocket == INVALID_SOCKET) {
			std::cout << "couldn't accept: " << WSAGetLastError() << std::endl;
			WSACleanup();
		}
		else {
			std::cout << "connected" << std::endl;
		}


		while (true) {
			char dataReceivedFromRequest[2000];
			int numberOfReceivedBytes = recv(acceptSocket, dataReceivedFromRequest, 1000, 0);
			if (numberOfReceivedBytes == SOCKET_ERROR || numberOfReceivedBytes == 0) {
				std::cout << "couldn't receive message: " << WSAGetLastError() << std::endl;
				break;
			}
			else {
				std::cout << "received " << numberOfReceivedBytes << " bytes" << std::endl;
			}
			std::string requestedResourcePath = getRequestedResourceFromRequest(dataReceivedFromRequest);

			if (requestedResourcePath == "/") {
				sendResource(acceptSocket, "/index.html");
			}
			else {
				sendResource(acceptSocket, requestedResourcePath);
			}
			if (strstr(dataReceivedFromRequest, "Connection: keep-alive") == nullptr) {
				break;
			}
		}
		closesocket(acceptSocket);
	}

	closesocket(mySocket);
	WSACleanup();
}
#include <any>
#include <ctime>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <tchar.h>
#include <utility>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

const unsigned short int port = 8080;
const PCWSTR address = L"127.0.0.1";

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

std::string getCurrentTimeHttpFormat() {
	std::timespec ts;
	if (std::timespec_get(&ts, TIME_UTC) != 0) {
		std::tm gmTime;
		gmtime_s(&gmTime, &ts.tv_sec);

		std::ostringstream oss;
		oss << std::put_time(&gmTime, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n");
		return oss.str();
	}
	return "Failed to get current time.";
}

void sendNotFoundError(SOCKET clientSocket) {
	std::string notFoundContent = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>";
	std::string httpResponseHeader = "HTTP/1.1 404 Not Found\r\n" + getCurrentTimeHttpFormat() + "Content-Type: text/html\r\nContent-Length: " + std::to_string(notFoundContent.length()) + "\r\n\r\n";
	send(clientSocket, httpResponseHeader.c_str(), httpResponseHeader.length(), 0);
	send(clientSocket, notFoundContent.c_str(), notFoundContent.length(), 0);
}

// Sends resource from path given in the http request header
void sendResource(SOCKET clientSocket, std::string requestedResourcePath) {
	std::ifstream t;
	std::filesystem::path projectPath = std::filesystem::current_path();
	std::string fullPath = projectPath.string() + "/public" + requestedResourcePath;
	std::stringstream resourceContent;
	std::string httpResponseHeader = "";

	// if resource is not found - return 404
	if (!std::filesystem::exists(fullPath)) {
		sendNotFoundError(clientSocket);
		std::cout << requestedResourcePath << " not found" << std::endl;
		return;
	}

	std::string contentType = GetContentTypeFromExtension(requestedResourcePath);
	t.open(fullPath);
	resourceContent << t.rdbuf();

	httpResponseHeader = "HTTP/1.1 200 OK\r\n" + getCurrentTimeHttpFormat() + "Content-Type: " + contentType + "\r\nContent-Length: " + std::to_string(resourceContent.str().length()) + "\r\n\r\n";

	send(clientSocket, httpResponseHeader.c_str(), httpResponseHeader.length(), 0);
	send(clientSocket, resourceContent.str().c_str(), resourceContent.str().length(), 0);
	std::cout << requestedResourcePath << " sent\n\n";
}

SOCKET InitializeSocket() {
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA data;
	int status = WSAStartup(wVersionRequested, &data);

	if (status != 0) {
		std::cout << "winsock dll not found" << std::endl;
		return INVALID_SOCKET;
	}

	std::cout << "winsock found" << std::endl;
	std::cout << "STATUS: " << data.szSystemStatus << std::endl;

	SOCKET mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (mySocket == INVALID_SOCKET) {
		std::cout << "\n\nsocket initialization failed with error code: " << WSAGetLastError << std::endl;
		WSACleanup();
		return INVALID_SOCKET;
	}

	std::cout << "\n\nsocket intialized successfully!" << std::endl;

	return mySocket;
}
//std::as_const()
bool BindSocket(SOCKET mySocket) {
	sockaddr_in bindInformation;
	bindInformation.sin_port = htons(std::as_const(port));
	bindInformation.sin_family = AF_INET;
	InetPton(AF_INET, std::as_const(address), &bindInformation.sin_addr.S_un.S_addr);

	int bindStatus = bind(mySocket, (sockaddr*)&bindInformation, sizeof(bindInformation));

	if (bindStatus == SOCKET_ERROR) {
		std::cout << "socket binding failed with error code: " << WSAGetLastError() << std::endl;
		closesocket(mySocket);
		WSACleanup();
		return false;
	}

	std::cout << "socket successfully bound to port " << port << std::endl;
	return true;
}

bool PlaceSocketInListeningMode(SOCKET mySocket) {
	if (listen(mySocket, 2) != 0) {
		std::cout << "socket can't be put in listening mode " << WSAGetLastError() << std::endl;
		return false;
	}

	std::cout << "socket listening for connections" << std::endl;
	return true;
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
			char dataReceivedFromRequest[4096];
			int numberOfReceivedBytes = recv(acceptSocket, dataReceivedFromRequest, 4096, 0);
			if (numberOfReceivedBytes == SOCKET_ERROR) {
				std::cout << "couldn't receive message: " << WSAGetLastError() << std::endl;
				break;
			}
			/*else {
				std::cout << "received " << numberOfReceivedBytes << " bytes" << std::endl;
			}*/
			std::string requestedResourcePath = getRequestedResourceFromRequest(dataReceivedFromRequest);

			if (requestedResourcePath == "/") {
				sendResource(acceptSocket, "/index.html");
			}
			else {
				sendResource(acceptSocket, requestedResourcePath);
			}
			if (strstr(dataReceivedFromRequest, "Connection: keep-alive") == nullptr || numberOfReceivedBytes == 0) {
				break;
			}
		}
		closesocket(acceptSocket);
		std::cout << "accept socket closed" << std::endl;
	}

	closesocket(mySocket);
	std::cout << "main socket closed" << std::endl;
	WSACleanup();
}
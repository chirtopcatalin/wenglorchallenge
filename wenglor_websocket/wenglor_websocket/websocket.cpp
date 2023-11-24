#include <any>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <filesystem>
#include <functional>
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

const unsigned short int PORT = 8080;
const PCWSTR ADDRESS = L"127.0.0.1";

std::map<std::string_view, std::string> extensionMap;
enum struct ErrorCodes { SOCKET_INITIALIZE_ERROR_CODE = 1,
						 SOCKET_BIND_ERROR_CODE,
						 SOCKET_LISTEN_ERROR_CODE};

// Map of file extensions and their content types
std::map<std::string_view, std::string> CreateExtensionMap() {
		std::map<std::string_view, std::string> extensionMap;

		extensionMap["html"] = "text/html";
		extensionMap["css"] = "text/css";
		extensionMap["js"] = "application/javascript";
		extensionMap["jpg"] = "image/jpeg";
		extensionMap["jpeg"] = "image/jpeg";
		extensionMap["png"] = "image/png";
		extensionMap["gif"] = "image/gif";
		extensionMap["ico"] = "image/x-icon";
		extensionMap["svg"] = "image/svg+xml";

		return extensionMap;
}



// Function that gets the extension of requested file and returns the content type to be put in the http header
// c++ 17 feature used: string_view - more memory-efficient because the value of the string does not get modified
std::string GetContentTypeFromExtension(const std::string_view& filePath){
	std::any dotPos = filePath.find_last_of(".");
	if (std::any_cast<size_t>(dotPos) != std::string::npos) {
		std::string_view extension = filePath.substr(std::any_cast<size_t>(dotPos) + 1);

		//use of structured bindings
		for (const auto& [key, value] : extensionMap) {
			if (key == extension) {
				return value;
			}
		}
	}
	return "application/octet-stream";
}

// Returns current time in HTTP format
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
void sendResource(SOCKET clientSocket, const std::string& requestedResourcePath) {
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
	bindInformation.sin_port = htons(std::as_const(PORT));
	bindInformation.sin_family = AF_INET;
	InetPton(AF_INET, std::as_const(ADDRESS), &bindInformation.sin_addr.S_un.S_addr);

	int bindStatus = bind(mySocket, (sockaddr*)&bindInformation, sizeof(bindInformation));

	if (bindStatus == SOCKET_ERROR) {
		std::cout << "socket binding failed with error code: " << WSAGetLastError() << std::endl;
		closesocket(mySocket);
		WSACleanup();
		return false;
	}

	std::cout << "socket successfully bound to port " << PORT << std::endl;
	return true;
}

bool PlaceSocketInListeningMode(SOCKET mySocket) {
	if (listen(mySocket, 1) != 0) {
		std::cout << "socket can't be put in listening mode " << WSAGetLastError() << std::endl;
		return false;
	}

	std::cout << "socket listening for connections" << std::endl;
	return true;
}

// Takes all data received in the request and returns the requested resource
std::string getRequestedResourceFromRequest(char dataReceivedFromRequest[]) {
	std::string requestedResource = "";
	int resourcePathOffset = 4;

	char* startOfRequestLine = strstr(dataReceivedFromRequest, "GET ");
	if (startOfRequestLine != nullptr) {
		char* endOfRequestLine = strstr(startOfRequestLine, " HTTP/1.1");
		if (endOfRequestLine != nullptr) {
			requestedResource.assign(startOfRequestLine + resourcePathOffset, endOfRequestLine);
			std::cout << "Requested Resource: " << requestedResource << std::endl;
		}
	}
	return requestedResource;
}

// Accepts connection by creating an accept socket from the listening socket
SOCKET AcceptConnection(SOCKET listeningSocket) {
	SOCKET acceptSocket = accept(listeningSocket, NULL, NULL);
	if (acceptSocket == INVALID_SOCKET) {
		std::cout << "couldn't accept connection! error code: " << WSAGetLastError() << std::endl;
		WSACleanup();
	}
	else {
		std::cout << "connected" << std::endl;
	}
	return acceptSocket;
}

int main()
{
	SOCKET mySocket = InitializeSocket();
	if(mySocket == INVALID_SOCKET) return static_cast<int>(ErrorCodes::SOCKET_INITIALIZE_ERROR_CODE);
	if (BindSocket(mySocket) == 0) return static_cast<int>(ErrorCodes::SOCKET_BIND_ERROR_CODE);
	if (PlaceSocketInListeningMode(mySocket) == 0) return static_cast<int>(ErrorCodes::SOCKET_LISTEN_ERROR_CODE);

	extensionMap = CreateExtensionMap();

	while (true) {
		SOCKET acceptSocket = AcceptConnection(mySocket);

		while (true) {
			char dataReceivedFromRequest[4096];
			int numberOfReceivedBytes = recv(acceptSocket, dataReceivedFromRequest, 4096, 0);
			if (numberOfReceivedBytes == SOCKET_ERROR) {
				std::cout << "couldn't receive message: " << WSAGetLastError() << std::endl;
				break;
			}
			else {
				std::cout << "received " << numberOfReceivedBytes << " bytes" << std::endl;
			}
			std::string requestedResourcePath = getRequestedResourceFromRequest(dataReceivedFromRequest);
			
			sendResource(acceptSocket, (requestedResourcePath == "/") ? "/index.html" : requestedResourcePath);

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
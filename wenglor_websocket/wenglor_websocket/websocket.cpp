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

std::map<std::string_view, std::string> extension_map;
enum struct ErrorCodes { SOCKET_INITIALIZE_ERROR_CODE = 1,
						 SOCKET_BIND_ERROR_CODE,
						 SOCKET_LISTEN_ERROR_CODE};

// Map of file extensions and their content types
std::map<std::string_view, std::string> CreateExtensionMap() {
		std::map<std::string_view, std::string> extension_map;

		extension_map["html"] = "text/html";
		extension_map["css"] = "text/css";
		extension_map["js"] = "application/javascript";
		extension_map["jpg"] = "image/jpeg";
		extension_map["jpeg"] = "image/jpeg";
		extension_map["png"] = "image/png";
		extension_map["gif"] = "image/gif";
		extension_map["ico"] = "image/x-icon";
		extension_map["svg"] = "image/svg+xml";

		return extension_map;
}



// Function that gets the extension of requested file and returns the content type to be put in the http header
// c++ 17 feature used: string_view - more memory-efficient because the value of the string does not get modified
std::string GetContentTypeFromExtension(const std::string_view& file_path){
	std::any dot_pos = file_path.find_last_of(".");
	if (std::any_cast<size_t>(dot_pos) != std::string::npos) {
		std::string_view extension = file_path.substr(std::any_cast<size_t>(dot_pos) + 1);

		//use of structured bindings
		for (const auto& [key, value] : extension_map) {
			if (key == extension) {
				return value;
			}
		}
	}
	return "application/octet-stream";
}

// Returns current time in HTTP format
std::string GetCurrentTimeHttpFormat() {
	std::timespec ts;
	if (std::timespec_get(&ts, TIME_UTC) != 0) {
		std::tm gm_time;
		gmtime_s(&gm_time, &ts.tv_sec);

		std::ostringstream oss;
		oss << std::put_time(&gm_time, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n");
		return oss.str();
	}
	return "Failed to get current time.";
}

void SendNotFoundError(SOCKET client_socket) {
	std::string not_found_content = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>";
	std::string http_response_header = "HTTP/1.1 404 Not Found\r\n" + GetCurrentTimeHttpFormat() + "Content-Type: text/html\r\nContent-Length: " + std::to_string(not_found_content.length()) + "\r\n\r\n";
	send(client_socket, http_response_header.c_str(), http_response_header.length(), 0);
	send(client_socket, not_found_content.c_str(), not_found_content.length(), 0);
}

// Sends resource from path given in the http request header
void SendResource(SOCKET client_socket, const std::string& requested_resource_path) {
	std::ifstream t;
	std::filesystem::path project_path = std::filesystem::current_path();
	std::string full_path = project_path.string() + "/public" + requested_resource_path;
	std::stringstream resource_content;
	std::string http_response_header = "";

	// if resource is not found - return 404
	if (!std::filesystem::exists(full_path)) {
		SendNotFoundError(client_socket);
		std::cout << requested_resource_path << " not found" << std::endl;
		return;
	}

	std::string content_type = GetContentTypeFromExtension(requested_resource_path);
	t.open(full_path);
	resource_content << t.rdbuf();

	http_response_header = "HTTP/1.1 200 OK\r\n" + GetCurrentTimeHttpFormat() + "Content-Type: " + content_type + "\r\nContent-Length: " + std::to_string(resource_content.str().length()) + "\r\n\r\n";

	send(client_socket, http_response_header.c_str(), http_response_header.length(), 0);
	send(client_socket, resource_content.str().c_str(), resource_content.str().length(), 0);
	std::cout << requested_resource_path << " sent\n\n";
}

SOCKET InitializeSocket() {
	WORD w_version_requested = MAKEWORD(2, 2);
	WSADATA data;
	int status = WSAStartup(w_version_requested, &data);

	if (status != 0) {
		std::cout << "winsock dll not found" << std::endl;
		return INVALID_SOCKET;
	}

	SOCKET my_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (my_socket == INVALID_SOCKET) {
		std::cout << "\n\nsocket initialization failed with error code: " << WSAGetLastError << std::endl;
		WSACleanup();
		return INVALID_SOCKET;
	}

	std::cout << "socket intialized successfully!" << std::endl;

	return my_socket;
}
//std::as_const()
bool BindSocket(SOCKET my_socket) {
	sockaddr_in bind_information;
	bind_information.sin_port = htons(std::as_const(PORT));
	bind_information.sin_family = AF_INET;
	InetPton(AF_INET, std::as_const(ADDRESS), &bind_information.sin_addr.S_un.S_addr);

	int bind_status = bind(my_socket, (sockaddr*)&bind_information, sizeof(bind_information));

	if (bind_status == SOCKET_ERROR) {
		std::cout << "socket binding failed with error code: " << WSAGetLastError() << std::endl;
		closesocket(my_socket);
		WSACleanup();
		return false;
	}

	std::cout << "socket successfully bound to port " << PORT << std::endl;
	return true;
}

bool PlaceSocketInListeningMode(SOCKET my_socket) {
	if (listen(my_socket, 1) != 0) {
		std::cout << "socket can't be put in listening mode " << WSAGetLastError() << std::endl;
		return false;
	}

	std::cout << "socket listening for connections" << std::endl;
	return true;
}

// Takes all data received in the request and returns the requested resource
std::string GetRequestedResourceFromRequest(char data_received_from_request[]) {
	std::string requested_resource = "";
	int resource_path_offset = 4;

	char* start_of_request_line = strstr(data_received_from_request, "GET ");
	if (start_of_request_line != nullptr) {
		char* end_of_requestLine = strstr(start_of_request_line, " HTTP/1.1");
		if (end_of_requestLine != nullptr) {
			requested_resource.assign(start_of_request_line + resource_path_offset, end_of_requestLine);
			std::cout << "Requested Resource: " << requested_resource << std::endl;
		}
	}
	return requested_resource;
}

// Accepts connection by creating an accept socket from the listening socket
SOCKET AcceptConnection(SOCKET listening_socket) {
	SOCKET accept_socket = accept(listening_socket, NULL, NULL);
	if (accept_socket == INVALID_SOCKET) {
		std::cout << "couldn't accept connection! error code: " << WSAGetLastError() << std::endl;
		WSACleanup();
	}
	else {
		std::cout << "connected" << std::endl;
	}
	return accept_socket;
}

int main()
{
	SOCKET my_socket = InitializeSocket();
	if(my_socket == INVALID_SOCKET) return static_cast<int>(ErrorCodes::SOCKET_INITIALIZE_ERROR_CODE);
	if (BindSocket(my_socket) == 0) return static_cast<int>(ErrorCodes::SOCKET_BIND_ERROR_CODE);
	if (PlaceSocketInListeningMode(my_socket) == 0) return static_cast<int>(ErrorCodes::SOCKET_LISTEN_ERROR_CODE);

	extension_map = CreateExtensionMap();

	while (true) {
		SOCKET accept_socket = AcceptConnection(my_socket);

		while (true) {
			char data_received_from_request[4096];
			int number_of_received_bytes = recv(accept_socket, data_received_from_request, 4096, 0);
			if (number_of_received_bytes == SOCKET_ERROR) {
				std::cout << "couldn't receive message: " << WSAGetLastError() << std::endl;
				break;
			}
			else {
				std::cout << "received " << number_of_received_bytes << " bytes" << std::endl;
			}
			std::string requested_resource_path = GetRequestedResourceFromRequest(data_received_from_request);
			
			SendResource(accept_socket, (requested_resource_path == "/") ? "/index.html" : requested_resource_path);

			if (strstr(data_received_from_request, "Connection: keep-alive") == nullptr || number_of_received_bytes == 0) {
				break;
			}
		}
		closesocket(accept_socket);
		std::cout << "accept socket closed" << std::endl;
	}

	closesocket(my_socket);
	std::cout << "main socket closed" << std::endl;
	WSACleanup();
}
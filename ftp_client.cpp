
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>

#define PORT 2121
#define BUFFER_SIZE 1024
#define EOF_MARKER "EOF_MARKER"

int connectToServer(const std::string& serverIP) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "

Socket creation error." << std::endl;
        return -1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection to server failed." << std::endl;
        close(clientSocket);
        return -1;
    }

    return clientSocket;
}

void uploadFile(int clientSocket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file for upload: " << filename << std::endl;
        return;
    }

    std::string command = "STOR " + filename + "\r\n";
    send(clientSocket, command.c_str(), command.size(), 0);

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE)) {
        send(clientSocket, buffer, file.gcount(), 0);
    }
    if (file.gcount() > 0) {
        send(clientSocket, buffer, file.gcount(), 0);
    }
    file.close();

    std::cout << "File upload completed: " << filename << std::endl;
}

void downloadFile(int clientSocket, const std::string& remoteFilename, const std::string& localFilename) {
    std::string command = "RETR " + remoteFilename + "\r\n";
    send(clientSocket, command.c_str(), command.size(), 0);

    char buffer[BUFFER_SIZE];
    int bytesReceived;
    std::ofstream file(localFilename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file for download: " << localFilename << std::endl;
        return;
    }

    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        std::string data(buffer, bytesReceived);
        if (data.find(EOF_MARKER) != std::string::npos) {
            data.erase(data.find(EOF_MARKER)); // Remove EOF marker
            file.write(data.c_str(), data.size());
            break;
        }
        file.write(buffer, bytesReceived);
    }

    if (bytesReceived < 0) {
        perror("recv");
    }

    file.close();
    std::cout << "File download completed: " << localFilename << std::endl;
}

void listFiles(int clientSocket) {
    std::string command = "LIST\r\n";
    send(clientSocket, command.c_str(), command.size(), 0);

    char buffer[BUFFER_SIZE];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        std::string data(buffer, bytesReceived);
        if (data.find(EOF_MARKER) != std::string::npos) {
            data.erase(data.find(EOF_MARKER)); // Remove EOF marker
            std::cout << data;
            break;
        }
        std::cout << data;
    }
    std::cout << "End of file list." << std::endl;
}

void searchFiles(int clientSocket, const std::string& query) {
    std::string command = "SEARCH " + query + "\r\n";
    send(clientSocket, command.c_str(), command.size(), 0);

    char buffer[BUFFER_SIZE];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        std::string data(buffer, bytesReceived);
        if (data.find(EOF_MARKER) != std::string::npos) {
            data.erase(data.find(EOF_MARKER)); // Remove EOF marker
            std::cout << data;
            break;
        }
        std::cout << data;
    }
    std::cout << "End of search results." << std::endl;
}

void deleteFile(int clientSocket, const std::string& filename) {
    std::string command = "DELE " + filename + "\r\n";
    send(clientSocket, command.c_str(), command.size(), 0);

    char buffer[BUFFER_SIZE];
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
    std::string response(buffer, bytesReceived);
    std::cout << response;
}

int main() {
    std::string serverIP = "127.0.0.1";
    int clientSocket = connectToServer(serverIP);
    if (clientSocket < 0) return -1;

    std::string command;
    while (true) {
        std::cout << "Enter command (upload <filename>, download <remote_filename> <local_filename>, list, search <query>, delete <filename>, quit): ";
        std::getline(std::cin, command);

        if (command.substr(0, 6) == "upload") {
            std::string filename = command.substr(7);
            uploadFile(clientSocket, filename);
        } else if (command.substr(0, 8) == "download") {
            size_t pos = command.find(" ", 9);
            std::string remoteFilename = command.substr(9, pos - 9);
            std::string localFilename = command.substr(pos + 1);
            downloadFile(clientSocket, remoteFilename, localFilename);
        } else if (command == "list") {
            listFiles(clientSocket);
        } else if (command.substr(0, 6) == "search") {
            std::string query = command.substr(7);
            searchFiles(clientSocket, query);
        } else if (command.substr(0, 6) == "delete") {
            std::string filename = command.substr(7);
            deleteFile(clientSocket, filename);
        } else if (command == "quit") {
            std::cout << "Disconnecting from server." << std::endl;
            close(clientSocket);
            break;
        } else {
            std::cout << "Unknown command. Use 'upload <filename>', 'download <remote_filename> <local_filename>', 'list', 'search <query>', 'delete <filename>', or 'quit'." << std::endl;
        }
    }

    return 0;
}

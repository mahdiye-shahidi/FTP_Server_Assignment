
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <vector>

#define PORT 2121
#define BUFFER_SIZE 1024
#define UPLOAD_DIRECTORY "uploads/"
#define EOF_MARKER "EOF_MARKER"

void ensureUploadDirectoryExists() {
    struct stat info;
    if (stat(UPLOAD_DIRECTORY, &info) != 0) {
        if (mkdir(UPLOAD_DIRECTORY, 0777) != 0) {
            std::cerr << "Failed to create uploads directory." << std::endl;
            exit(1);
        } else {
            std::cout << "Uploads directory created." << std::endl;
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        std::cerr << "Uploads path exists but is not a directory." << std::endl;
        exit(1);
    }
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string command;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }

        command = std::string(buffer, bytesReceived);

        // STOR command (upload)
        if (command.substr(0, 4) == "STOR") {
            std::string filename = command.substr(5, command.size() - 7);
            std::string filePath = UPLOAD_DIRECTORY + filename;
            std::ofstream file(filePath, std::ios::binary);
            if (file.is_open()) {
                std::string response = "150 Ready to receive\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);

                while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
                    file.write(buffer, bytesReceived);
                }
                file.close();

                response = "226 Transfer complete\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
                std::cout << "File uploaded: " << filePath << std::endl;
            } else {
                std::string response = "550 Cannot open file for writing\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // RETR command (download)
        else if (command.substr(0, 4) == "RETR") {
            std::string filename = command.substr(5, command.size() - 7);
            std::string filePath = UPLOAD_DIRECTORY + filename;
            std::ifstream file(filePath, std::ios::binary);
            if (file.is_open()) {
                std::string response = "150 Opening file\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);

                char buffer[BUFFER_SIZE];
                while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
                    send(clientSocket, buffer, file.gcount(), 0);
                }
                file.close();

                send(clientSocket, EOF_MARKER, strlen(EOF_MARKER), 0);

                response = "226 Transfer complete\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
                std::cout << "File downloaded: " << filePath << std::endl;
            } else {
                std::string response = "550 File not found\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // LIST command
        else if (command.substr(0, 4) == "LIST") {
            DIR *dir;
            struct dirent *entry;
            std::string response = "150 Here comes the directory listing\r\n";
            send(clientSocket, response.c_str(), response.size(), 0);

            dir = opendir(UPLOAD_DIRECTORY);
            if (dir == nullptr) {
                response = "550 Failed to open directory\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            } else {
                while ((entry = readdir(dir)) != nullptr) {
                    if (entry->d_type == DT_REG) {
                        response = std::string(entry->d_name) + "\n";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
                closedir(dir);

                send(clientSocket, EOF_MARKER, strlen(EOF_MARKER), 0);
                response = "226 Directory listing complete\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // DELE command (delete file)
        else if (command.substr(0, 4) == "DELE") {
            std::string filename = command.substr(5, command.size() - 7);
            std::string filePath = UPLOAD_DIRECTORY + filename;
            if (remove(filePath.c_str()) == 0) {
                std::string response = "250 File deleted successfully\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
                std::cout << "File deleted: " << filePath << std::endl;
            } else {
                std::string response = "550 File not found\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // SEARCH command (search for a file)
        else if (command.substr(0, 6) == "SEARCH") {
            std::string query = command.substr(7, command.size() - 9);
            std::string response = "150 Here are the search results:\r\n";
            send(clientSocket, response.c_str(), response.size(), 0);

            DIR *dir;
            struct dirent *entry;
            dir = opendir(UPLOAD_DIRECTORY);
            bool found = false;

            if (dir == nullptr) {
                response = "550 Failed to open directory\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            } else {
                while ((entry = readdir(dir)) != nullptr) {
                    if (entry->d_type == DT_REG) {
                        std::string filename(entry->d_name);
                        if (filename.find(query) != std::string::npos) {
                            response = filename + "\n";
                            send(clientSocket, response.c_str(), response.size(), 0);
                            found = true;
                        }
                    }
                }
                closedir(dir);

                if (!found) {
                    response = "No files found matching your query.\r\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }

                send(clientSocket, EOF_MARKER, strlen(EOF_MARKER), 0);
                response = "226 Search complete\r\n";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Unknown command
        else {
            std::string response = "502 Command not implemented\r\n";
            send(clientSocket, response.c_str(), response.size(), 0);
        }
    }
    close(clientSocket);
}

int main() {
    ensureUploadDirectoryExists();

    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrSize = sizeof(clientAddr);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Socket creation error." << std::endl;
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed. Check port or permissions." << std::endl;
        close(serverSocket);
        return -1;
    }

    if (listen(serverSocket, 5) == 0) {
        std::cout << "Server listening on port " << PORT << "..." << std::endl;
    } else {
        std::cerr << "Error in listening." << std::endl;
        close(serverSocket);
        return -1;
    }

    while (true) {
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrSize);
        if (clientSocket < 0) {
            std::cerr << "Error accepting client." << std::endl;
            continue;
        }
        std::cout << "Client connected: " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;
        handleClient(clientSocket);
    }

    close(serverSocket);
    return 0;
}

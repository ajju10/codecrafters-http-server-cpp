#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>

#define BUFF_SIZE 1024

void clientHandler(int connfd, std::string file_path) {
  if (connfd < 0) {
    std::cerr << "Error with connection.\n";
    return;
  }
  
  char receive_buffer[BUFF_SIZE];
  if (read(connfd, receive_buffer, sizeof(receive_buffer)) < 0) {
    std::cerr << "Failed to receive\n";
    close(connfd);
    return;
  }
  
  std::string receive(receive_buffer);
  std::string send_buffer;
  
  if (receive.find("/ ") != std::string::npos) {
    send_buffer = "HTTP/1.1 200 OK\r\n\r\n";
  } else if (receive.find("/echo/") != std::string::npos) {
    send_buffer = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
    std::string::size_type pos1 = receive.find("echo") + 5;
    std::string::size_type pos2 = receive.find(" ", pos1);
    send_buffer += std::to_string(pos2-pos1) + "\r\n\r\n";
    send_buffer += receive.substr(pos1, pos2-pos1) + "\r\n";
  } else if (receive.find("/user-agent") != std::string::npos) {
    send_buffer = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
    std::string::size_type pos1 = receive.rfind("User-Agent") + 12;
    std::string::size_type pos2 = receive.find("\r\n", pos1);
    std::string messg = receive.substr(pos1, pos2-pos1);
    send_buffer += std::to_string(messg.size()) + "\r\n\r\n";
    send_buffer += messg + "\r\n";
  } else if (receive.find("/files") != std::string::npos) {
    std::string::size_type dir_start = receive.find("/files") + 7;
    std::string::size_type dir_end = receive.find(" ", dir_start);
    std::string file_name = receive.substr(dir_start, dir_end - dir_start);
    std::fstream file(file_path + "/" + file_name);
    if (file.is_open()) {
      send_buffer = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: ";
      std::string file_content;
      char char_buffer;
      while (!file.eof()) {
        file.get(char_buffer);
        if (char_buffer == '\n') {
          file_content += '\r';
        }
        file_content += char_buffer;
      }
      send_buffer += std::to_string(file_content.size() - 1) + "\r\n\r\n" + file_content + "\r\n";
    } else {
      send_buffer = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
  } else {
    send_buffer = "HTTP/1.1 404 Not Found\r\n\r\n";
  } 

  if (write(connfd, send_buffer.c_str(), send_buffer.size()) < 0) {
      return;
  }
  
  std::cout << "Client " << connfd << " disconnected\n";
  close(connfd);
}

int setup() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    //ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    std::cout << "listening for incoming requests\n";

    return server_fd;
}

int main(int argc, char **argv) {
  std::string file_path;
  if (argc == 3 && strcmp(argv[1], "--directory") == 0) {
    file_path = argv[2];
  }
  int server_fd = setup();
  std::vector<std::thread> client_pool;
  while (true) {
    int connfd = accept(server_fd, NULL, NULL);
    if (connfd < 0) {
      continue;
    }
    std::cout << "Client " << connfd << " connected\n";
    client_pool.emplace_back(clientHandler, connfd, file_path);
  }
  
  for (auto &x : client_pool) {
    x.join();
  }

  close(server_fd);

  return 0;
}
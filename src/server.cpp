#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

const int BUFFER_SIZE = 1024;

enum HTTPVersion {
  HTTP1_0 = 0,
  HTTP1_1 = 1,
  HTTP2_0 = 2
};

enum RequestMethod {
  GET = 0,
  POST = 1
};

struct http_client {
  RequestMethod method;
  std::string path;
  HTTPVersion http_version;
  std::unordered_map<std::string, std::string> headers;
};

enum HTTPCode {
  OK = 200,
  NOT_FOUND = 404
};

std::vector<std::string> split(const std::string &str, const char delim) {
  std::stringstream stream(str);
  std::vector<std::string> result;

  std::string word;
  while (std::getline(stream, word, delim)) {
    result.push_back(word);
  }
  return result;
}

std::unordered_map<std::string, std::string> parse_headers(const std::vector<std::string> &req_lines) {
  std::cout << "Parsing headers\n";
  for (auto &word: req_lines)
    std::cout << word << '\n';

  std::unordered_map<std::string, std::string> headers;
  return headers;
}

http_client parse_request_data(std::string &req_data) {
  std::vector<std::string> req_lines = split(req_data, '\n');

  std::vector<std::string> first_line = split(req_lines[0], ' ');

  http_client client;
  
  if (first_line[0] == "GET") {
    client.method = RequestMethod::GET;
  } else if (first_line[0] == "POST") {
    client.method = RequestMethod::POST;
  }

  client.path = first_line[1];
  if (first_line[2] == "HTTP/1.0") {
    client.http_version = HTTPVersion::HTTP1_0;
  } else if (first_line[2] == "HTTP/1.1") {
    client.http_version = HTTPVersion::HTTP1_1;
  } else if (first_line[2] == "HTTP/2.0") {
    client.http_version = HTTPVersion::HTTP2_0;
  }

  // client.headers = parse_headers(req_lines);

  return client;
}

int main(int argc, char **argv) {
  // Uncomment this block to pass the first stage
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting REUSE_PORT
  // ensures that we don't run into 'Address already in use' errors
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
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  std::cout << "Client connected\n";

  char buffer[BUFFER_SIZE];
  int client_req = read(client_fd, buffer, sizeof(buffer));
  if (client_req <= 0) {
    std::cerr << "Client error\n";
  }

  std::string req_data(buffer);
  http_client client = parse_request_data(req_data);

  std::string res = "";
  if (client.path == "/") {
    res = "HTTP/1.1 200 OK\r\n\r\n";
  } else if (client.path.find("/echo") != std::string::npos) {
    std::string random_str = "";
    for (int i = 6; i < client.path.size(); i++) {
      random_str += client.path[i];
    }
    res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " 
          + std::to_string(random_str.size()) + "\r\n\r\n" + random_str;
  } else if (client.path == "/user-agent") {
    size_t start = req_data.find("User-Agent: ");
    size_t end = req_data.find("\r\n", start);
    size_t agent_len = strlen("User-Agent: ");
    std::string header_value = req_data.substr(start + agent_len, end - agent_len - start);
    std::cout << "Header value: " << header_value << '\n';
    res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
          + std::to_string(header_value.size()) + "\r\n\r\n" + header_value;
  } else {
    res = "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  send(client_fd, res.c_str(), res.size(), 0);
  
  close(client_fd);

  close(server_fd);

  return 0;
}

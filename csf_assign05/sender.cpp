#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include "csapp.h"
#include "message.h"
#include "connection.h"
#include "client_util.h"

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cerr << "Usage: ./sender [server_address] [port] [username]\n";
    return 1;
  }

  std::string server_hostname;
  int server_port;
  std::string username;

  server_hostname = argv[1];
  server_port = std::stoi(argv[2]);
  username = argv[3];

  // TODO: connect to server
Connection conn;
  try {
    conn.connect(server_hostname, server_port);
  } catch (const std::exception &e) {
    std::cerr << "Error connecting to server: " << e.what() << std::endl;
    return 1;
  }

  // TODO: send slogin message
  Message msg(TAG_SLOGIN, username);
  if (!conn.send(msg) || !conn.receive(msg) || msg.tag == TAG_ERR) {
    std::cerr << msg.data << std::endl;
    return 1;
  }

  // TODO: loop reading commands from user, sending messages to
  //       server as appropriate
  std::string line;
    while (std::getline(std::cin, line)) {
      line = trim(line);
      if (line.empty()) continue;

      Message out;
      if (line.rfind("/join ", 0) == 0) {
        out = Message(TAG_JOIN, line.substr(6));
      } else if (line == "/leave") {
        out = Message(TAG_LEAVE, "");
      } else if (line == "/quit") {
        out = Message(TAG_QUIT, "");
      } else if (line[0] == '/') {
        std::cerr << "Unknown command\n";
        continue;
      } else {
        out = Message(TAG_SENDALL, line);
      }

      if (!conn.send(out)) break;
      if (!conn.receive(msg)) break;

      if (msg.tag == TAG_ERR) {
        std::cerr << msg.data << std::endl;
      } else if (out.tag == TAG_QUIT) {
        break;
      }
    }

  return 0;
}

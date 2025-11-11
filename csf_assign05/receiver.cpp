#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include "csapp.h"
#include "message.h"
#include "connection.h"
#include "client_util.h"

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "Usage: ./receiver [server_address] [port] [username] [room]\n";
    return 1;
  }

  std::string server_hostname = argv[1];
  int server_port = std::stoi(argv[2]);
  std::string username = argv[3];
  std::string room_name = argv[4];

  Connection conn;

  // TODO: connect to server

  try {
    conn.connect(server_hostname, server_port);
  } catch (const std::exception &e) {
    std::cerr << "There is unfortunately an error when we try connecting to server which is " << e.what() << std::endl;
    return 1;
  }



  // TODO: send rlogin and join messages (expect a response from
  //       the server for each one)
  Message msg(TAG_RLOGIN, username);
  if (!conn.send(msg) || !conn.receive(msg) || msg.tag == TAG_ERR) {
    std::cerr << msg.data << std::endl;
    return 1;
  }

  msg = Message(TAG_JOIN, room_name);
  if (!conn.send(msg) || !conn.receive(msg) || msg.tag == TAG_ERR) {
    std::cerr << msg.data << std::endl;
    return 1;
  }


  // TODO: loop waiting for messages from server
  //       (which should be tagged with TAG_DELIVERY)
  while (true) {
    if (!conn.receive(msg)) break;

    if (msg.tag == TAG_DELIVERY) {
      // delivery:<room>:<sender>:<message>
      size_t first = msg.data.find(':');
      size_t second = msg.data.find(':', first + 1);
      if (first != std::string::npos && second != std::string::npos) {
        std::string sender = msg.data.substr(first + 1, second - first - 1);
        std::string text = msg.data.substr(second + 1);
        std::cout << sender << ": " << text << std::endl;
      }
    } else if (msg.tag == TAG_ERR) {
      std::cerr << msg.data << std::endl;
    }
  }


  return 0;
}

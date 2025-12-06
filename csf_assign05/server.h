#ifndef SERVER_H
#define SERVER_H

#include <map>
#include <string>
#include <pthread.h>
#include "connection.h"
#include "user.h"

class Room;

class Server {
public:
    Server(int port);
    ~Server();

    bool listen();
    void handle_client_requests();

    struct client_info {
        int sockfd;
        char role;
        std::string uname;
        Connection* conn;
        pthread_t tid;
        Room* room;
        User* user;
        client_info() :
            sockfd(-1), role('?'),
            conn(nullptr), tid(0),
            room(nullptr), user(nullptr) {}
    };

    void chat_with_sender(client_info* c);
    void chat_with_receiver(client_info* c);

    Room* find_or_create_room(const std::string& room_name);

private:
  // prohibit value semantics
  Server(const Server &);
  Server &operator=(const Server &);

  typedef std::map<std::string, Room *> RoomMap;

  // These member variables are sufficient for implementing
  // the server operations
  int m_port;
  int m_ssock;
  RoomMap m_rooms;
  pthread_mutex_t m_lock;
};

#endif

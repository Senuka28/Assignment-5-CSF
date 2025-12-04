#ifndef SERVER_H
#define SERVER_H

#include <map>
#include <string>
#include <pthread.h>
class Room;

class Server {
public:
  Server(int port);
  ~Server();

  bool listen();

  void handle_client_requests();

  Room *find_or_create_room(const std::string &room_name);

  typedef std::map<std::string, Room *> RoomMap;
  RoomMap &get_rooms() { return m_rooms; } // gives read/write access to rooms
  static Server *get_instance() { return m_instance; }


private:
  // prohibit value semantics
  Server(const Server &);
  Server &operator=(const Server &);

  
  // These member variables are sufficient for implementing
  // the server operations
  int m_port;
  int m_ssock;
  RoomMap m_rooms;
  pthread_mutex_t m_lock;

  static Server *m_instance;

};

#endif // SERVER_H

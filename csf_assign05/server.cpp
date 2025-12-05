#include <pthread.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <set>
#include <vector>
#include <cctype>
#include <cassert>
#include "message.h"
#include "connection.h"
#include "user.h"
#include "room.h"
#include "guard.h"
#include "server.h"

////////////////////////////////////////////////////////////////////////
// Server implementation data types
////////////////////////////////////////////////////////////////////////

struct ThreadArgs {
  Server *server;
  Connection *conn;
};

// TODO: add any additional data types that might be helpful
//       for implementing the Server member functions

////////////////////////////////////////////////////////////////////////
// Client thread functions
////////////////////////////////////////////////////////////////////////

namespace {

void chat_with_sender(Server *server, Connection *conn, User *user) {
  Message msg;
  Room *current_room = nullptr;  // track which room the sender is in

  while (conn->receive(msg)) {
    if (msg.tag == TAG_JOIN) {
      current_room = server->find_or_create_room(msg.data);
      current_room->add_member(user);
      conn->send(Message(TAG_OK, "joined " + msg.data));
    } 
    else if (msg.tag == TAG_LEAVE) {
      if (current_room) {
        current_room->remove_member(user);
        conn->send(Message(TAG_OK, "left"));
        current_room = nullptr;
      } else {
        conn->send(Message(TAG_ERR, "not in any room"));
      }
    } 
    else if (msg.tag == TAG_SENDALL) {
      if (current_room) {
        current_room->broadcast_message(user->username, msg.data);
        conn->send(Message(TAG_OK, "delivered"));
      } else {
        conn->send(Message(TAG_ERR, "not in a room"));
      }
    } 
    else if (msg.tag == TAG_QUIT) {
      if (current_room) {
        current_room->remove_member(user);
      }
      conn->send(Message(TAG_OK, "bye"));
      break;
    } 
    else {
      conn->send(Message(TAG_ERR, "unknown command"));
    }
  }
}

void chat_with_receiver(Server *server, Connection *conn, User *user) {
  Message *msg;
  while (true) {
    msg = user->mqueue.dequeue();
    if (!msg) {
      if (!conn->send(Message(TAG_EMPTY, ""))) break;
    } else {
      if (!conn->send(*msg)) break;
      delete msg;
    }
  }
}


void *worker(void *arg) {
  pthread_detach(pthread_self());

  ThreadArgs *args = static_cast<ThreadArgs *>(arg);
  Server *server = args->server;
  Connection *conn = args->conn;
  delete args; 

  Message msg;
  if (!conn->receive(msg)) {
    delete conn;
    return nullptr;
  }

  if (msg.tag == TAG_SLOGIN) {
    User *user = new User(msg.data);
    conn->send(Message(TAG_OK, "logged in as sender"));
    chat_with_sender(server, conn, user);
    delete user;
  } else if (msg.tag == TAG_RLOGIN) {
    User *user = new User(msg.data);
    conn->send(Message(TAG_OK, "logged in as receiver"));
    chat_with_receiver(server, conn, user);
    delete user;
  } else {
    conn->send(Message(TAG_ERR, "expected login"));
  }

  delete conn;
  return nullptr;
}

}

////////////////////////////////////////////////////////////////////////
// Server member function implementation
////////////////////////////////////////////////////////////////////////

Server::Server(int port)
  : m_port(port)
  , m_ssock(-1) {
  pthread_mutex_init(&m_lock, nullptr); // init server mutex
}

Server::~Server() {
  // TODO: destroy mutex
  pthread_mutex_destroy(&m_lock);
}

bool Server::listen() {
  m_ssock = open_listenfd(std::to_string(m_port).c_str());
  return m_ssock >= 0; // true if socket created fine
}

void Server::handle_client_requests() {
  while (true) {
    int client_fd = Accept(m_ssock, nullptr, nullptr);
    if (client_fd < 0) continue;
    
    Connection *conn = new Connection(client_fd);
    ThreadArgs *args = new ThreadArgs{this, conn};
    pthread_t th;
    pthread_create(&th, nullptr, worker, args); // create a new thread
  }
}

Room *Server::find_or_create_room(const std::string &room_name) {
  Guard g(m_lock);
  auto it = m_rooms.find(room_name);
  if (it != m_rooms.end()) {
    return it->second;
  }
  Room *r = new Room(room_name);
  m_rooms[room_name] = r;
  return r;
}
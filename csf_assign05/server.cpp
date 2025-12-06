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

// No additional helper structs needed (client_info defined in server.h)

////////////////////////////////////////////////////////////////////////
// Client thread functions
////////////////////////////////////////////////////////////////////////

namespace {

struct worker_args {
    Server* server;
    Server::client_info* info;
};

void *worker(void *arg) {
  pthread_detach(pthread_self());

  worker_args* wa = static_cast<worker_args*>(arg);
  Server* server = wa->server;
  Server::client_info* ci = wa->info;
  delete wa;

  Message login;
  if (!ci->conn->receive(login)) {
    std::cerr << "[worker] Failed to read login message\n";
    close(ci->sockfd);
    delete ci->conn;
    delete ci;
    return nullptr;
  }

  if (login.tag == TAG_SLOGIN) {
    ci->role = 'S';
    ci->uname = login.data;
    ci->user = new User(login.data);
    ci->conn->send(Message(TAG_OK, "ok"));
    server->chat_with_sender(ci);
  }
  else if (login.tag == TAG_RLOGIN) {
    ci->role = 'R';
    ci->uname = login.data;
    ci->user = new User(login.data);
    ci->conn->send(Message(TAG_OK, "ok"));
    server->chat_with_receiver(ci);
  }
  else {
    ci->conn->send(Message(TAG_ERR, "Invalid login"));
    close(ci->sockfd);
    delete ci->conn;
    delete ci;
    return nullptr;
  }

  return nullptr;
}

}


////////////////////////////////////////////////////////////////////////
// Server member function implementation
////////////////////////////////////////////////////////////////////////

Server::Server(int port)
  : m_port(port), m_ssock(-1)
{
  pthread_mutex_init(&m_lock, nullptr);
}

Server::~Server() {
    pthread_mutex_destroy(&m_lock);
}

bool Server::listen() {
  std::ostringstream ss;
  ss << m_port;
  std::string port_str = ss.str();

  m_ssock = open_listenfd(port_str.c_str());
  if (m_ssock < 0) {
    std::cerr << "[server] open_listenfd failed\n";
    return false;
  }

  std::cout << "[server] Listening on port " << m_port << "\n";
  return true;
}

void Server::handle_client_requests() {
  while (true) {
    int client_fd = Accept(m_ssock, nullptr, nullptr);

    if (client_fd < 0) {
      std::cerr << "[server] accept failed\n";
      continue;
    }

    client_info* c = new client_info;
    c->sockfd = client_fd;
    c->conn = new Connection(client_fd);

    worker_args* wa = new worker_args{this, c};

    pthread_t tid;
    if (pthread_create(&tid, nullptr, worker, wa) != 0) {
      std::cerr << "[server] thread creation failed\n";
    }
  }
}

Room* Server::find_or_create_room(const std::string& name) {
  Guard g(m_lock);

  auto it = m_rooms.find(name);
  if (it != m_rooms.end()) {
    return it->second;
  }

  Room* new_room = new Room(name);
  m_rooms[name] = new_room;
  return new_room;
}

////////////////////////////////////////////////////////////////////////
// Sender + Receiver communication logic
////////////////////////////////////////////////////////////////////////

void Server::chat_with_sender(client_info* ci) {
  while (true) {
    Message msg;

    if (!ci->conn->receive(msg)) {
      std::cerr << "[sender] read failed\n";
      close(ci->sockfd);
      return;
    }

    if (msg.tag == TAG_JOIN) {
      pthread_mutex_lock(&m_lock);
      ci->room = find_or_create_room(msg.data);
      ci->room->add_member(ci->user);
      ci->conn->send(Message(TAG_OK, msg.data));
      pthread_mutex_unlock(&m_lock);
    }
    else if (msg.tag == TAG_SENDALL) {
      if (!ci->room) {
        ci->conn->send(Message(TAG_ERR, "not in room"));
        return;
      }

      pthread_mutex_lock(&m_lock);
      ci->room->broadcast_message(ci->uname, msg.data);
      pthread_mutex_unlock(&m_lock);

      ci->conn->send(Message(TAG_OK, msg.data));
    }
    else if (msg.tag == TAG_LEAVE) {
      if (!ci->room) {
        ci->conn->send(Message(TAG_ERR, "not in room"));
        return;
      }
      ci->room->remove_member(ci->user);
      ci->room = nullptr;
      ci->conn->send(Message(TAG_OK, msg.data));
    }
    else if (msg.tag == TAG_QUIT) {
      ci->conn->send(Message(TAG_OK, "bye"));
      close(ci->sockfd);
      return;
    }
    else if (msg.tag == TAG_ERR) {
      ci->conn->send(Message(TAG_ERR, "err"));
      return;
    }
  }
}


void Server::chat_with_receiver(client_info* ci) {
  Message first;

  if (!ci->conn->receive(first)) {
    close(ci->sockfd);
    return;
  }

  if (first.tag != TAG_JOIN) {
    ci->conn->send(Message(TAG_ERR, "invalid tag"));
    return;
  }

  pthread_mutex_lock(&m_lock);
  ci->room = find_or_create_room(first.data);
  ci->room->add_member(ci->user);
  ci->conn->send(Message(TAG_OK, first.data));
  pthread_mutex_unlock(&m_lock);

  while (true) {
    Message* pending = ci->user->mqueue.dequeue();
    if (!pending) continue;

    pending->tag = TAG_DELIVERY;
    ci->conn->send(*pending);

    delete pending;
  }
}

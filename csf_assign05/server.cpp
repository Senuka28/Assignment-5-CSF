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

void* worker(void* arg) {
  pthread_detach(pthread_self());

  worker_args* w = static_cast<worker_args*>(arg);
  Server* srv = w->server;
  Server::client_info* c = w->info;
  delete w;

  Message login;

  if (!c->conn->receive(login)) {
    std::cerr << "[worker] login recv fail\n";
    close(c->sockfd);
    delete c->conn;
    delete c;
    return nullptr;
  }

  if (login.tag == TAG_SLOGIN) {
    c->role = 'S';
    c->uname = login.data;
    c->user = new User(login.data);
    c->conn->send(Message(TAG_OK, "ok"));
    srv->chat_with_sender(c);
  }
  else if (login.tag == TAG_RLOGIN) {
    c->role = 'R';
    c->uname = login.data;
    c->user = new User(login.data);
    c->conn->send(Message(TAG_OK, "ok"));
    srv->chat_with_receiver(c);
  }
  else {
    c->conn->send(Message(TAG_ERR, "invalid login"));
    close(c->sockfd);
    delete c->conn;
    delete c;
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
  std::string pstr = ss.str();

  m_ssock = open_listenfd(pstr.c_str());
  if (m_ssock < 0) {
    std::cerr << "listenfd fail\n";
    return false;
  }

  std::cout << "[server] listening on port " << m_port << "\n";
  return true;
}

void Server::handle_client_requests() {
  while (true) {
    int cfd = Accept(m_ssock, nullptr, nullptr);

    if (cfd < 0) {
      std::cerr << "[server] accept fail\n";
      continue;
    }

    auto* ci = new client_info;
    ci->sockfd = cfd;
    ci->conn = new Connection(cfd);

    auto* pkg = new worker_args{this, ci};

    pthread_t tid;
    if (pthread_create(&tid, nullptr, worker, pkg) != 0) {
      std::cerr << "[server] thread fail\n";
    }
  }
}

Room* Server::find_or_create_room(const std::string& room_name) {
  if (m_rooms.count(room_name) == 0) {
    Room* r = new Room(room_name);
    m_rooms[room_name] = r;
  }
  return m_rooms[room_name];
}

////////////////////////////////////////////////////////////////////////
// Sender + Receiver communication logic
////////////////////////////////////////////////////////////////////////

void Server::chat_with_sender(client_info* c) {
  while (true) {
    Message msg;

    if (!c->conn->receive(msg)) {
      std::cerr << "[sender] read fail\n";
      close(c->sockfd);
      return;
    }

    // JOIN
    if (msg.tag == TAG_JOIN) {
      pthread_mutex_lock(&m_lock);
      c->room = find_or_create_room(msg.data);
      c->room->add_member(c->user);
      c->conn->send(Message(TAG_OK, msg.data));
      pthread_mutex_unlock(&m_lock);
    }

    // SENDALL
    else if (msg.tag == TAG_SENDALL) {
      if (!c->room) {
        c->conn->send(Message(TAG_ERR, "not in room"));
        return;
      }

      pthread_mutex_lock(&m_lock);
      c->room->broadcast_message(c->uname, msg.data);
      pthread_mutex_unlock(&m_lock);

      c->conn->send(Message(TAG_OK, msg.data));
    }

    // LEAVE
    else if (msg.tag == TAG_LEAVE) {
      if (!c->room) {
        c->conn->send(Message(TAG_ERR, "not in room"));
        return;
      }

      c->room->remove_member(c->user);
      c->room = nullptr;
      c->conn->send(Message(TAG_OK, msg.data));
    }

    // QUIT
    else if (msg.tag == TAG_QUIT) {
      c->conn->send(Message(TAG_OK, "bye"));
      close(c->sockfd);
      return;
    }

    // ERR
    else if (msg.tag == TAG_ERR) {
      c->conn->send(Message(TAG_ERR, "err"));
      return;
    }
  }
}


void Server::chat_with_receiver(client_info* c) {
  Message first;

  if (!c->conn->receive(first)) {
    close(c->sockfd);
    return;
  }

  bool joined = false;

  if (first.tag == TAG_JOIN) {
    pthread_mutex_lock(&m_lock);
    c->room = find_or_create_room(first.data);
    c->room->add_member(c->user);
    c->conn->send(Message(TAG_OK, first.data));
    pthread_mutex_unlock(&m_lock);
    joined = true;
  }
  else if (first.tag == TAG_ERR) {
    c->conn->send(Message(TAG_ERR, first.data));
    return;
  }
  else {
    c->conn->send(Message(TAG_ERR, "invalid tag"));
    return;
  }

  while (joined) {
    Message* pending = c->user->mqueue.dequeue();
    if (!pending) continue;

    pending->tag = TAG_DELIVERY;
    c->conn->send(*pending);

    delete pending;
  }
}

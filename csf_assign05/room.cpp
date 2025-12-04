#include "guard.h"
#include "message.h"
#include "message_queue.h"
#include "user.h"
#include "room.h"

Room::Room(const std::string &room_name)
  : room_name(room_name) {
  pthread_mutex_init(&m_lock, nullptr); // init mutex
}

Room::~Room() {
  pthread_mutex_destroy(&m_lock); // destroy mutex
}

void Room::add_member(User *user) {
  Guard g(m_lock);
  members.insert(user); // add user to room
}

void Room::remove_member(User *user) {
  // TODO: remove User from the room
  Guard g(m_lock);
  members.erase(user);
}

void Room::broadcast_message(const std::string &sender_username, const std::string &message_text) {
  // TODO: send a message to every (receiver) User in the room
  Guard g(m_lock);
  for (auto u : members) {
    Message *msg = new Message(TAG_DELIVERY, room_name + ":" + sender_username + ":" + message_text);
    u->mqueue.enqueue(msg); // enqueue for each receiver
  }
}

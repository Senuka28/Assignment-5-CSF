#include "guard.h"
#include "message.h"
#include "message_queue.h"
#include "user.h"
#include "room.h"

Room::Room(const std::string &room_name)
  : room_name(room_name) {
  pthread_mutex_init(&lock, nullptr); // init mutex
}

Room::~Room() {
  pthread_mutex_destroy(&lock); // destroy mutex
}

void Room::add_member(User *user) {
  //Guard g(lock);
  pthread_mutex_lock(&lock);
  members.insert(user); // add user to room
  pthread_mutex_unlock(&lock);
}

void Room::remove_member(User *user) {
  // TODO: remove User from the room
  /*Guard g(m_lock);
  members.erase(user);*/
  pthread_mutex_lock(&lock);
  members.erase(user);
  pthread_mutex_unlock(&lock);
}

void Room::broadcast_message(const std::string &sender_username, const std::string &message_text) {
  // TODO: send a message to every (receiver) User in the room
  pthread_mutex_lock(&lock);

  //Guard g(lock);
  for (User* user_in_members : members) {
    Message *msg = new Message(TAG_DELIVERY, room_name + ": " + sender_username + ": " + message_text);
    user_in_members->mqueue.enqueue(msg); // enqueue for each receiver
  }

  pthread_mutex_unlock(&lock);
}
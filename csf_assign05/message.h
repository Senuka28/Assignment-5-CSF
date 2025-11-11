#ifndef MESSAGE_H
#define MESSAGE_H

#include <vector>
#include <string>

struct Message {
  // An encoded message may have at most this many characters,
  // including the trailing newline ('\n'). Note that this does
  // *not* include a NUL terminator (if one is needed to
  // temporarily store the encoded message.)
  static const unsigned MAX_LEN = 255;

  std::string tag;
  std::string data;

  Message() { }

  Message(const std::string &tag, const std::string &data)
    : tag(tag), data(data) { }

  std::string encode() const {
    std::ostringstream oss;
    oss << tag << ":" << data << "\n";
    return oss.str();
  }

  bool decode(const std::string &raw) {
    size_t sep = raw.find(':');
    if (sep == std::string::npos)
      return false;
    tag = raw.substr(0, sep);
    data = raw.substr(sep + 1);
    while (!data.empty() && (data.back() == '\n' || data.back() == '\r'))
      data.pop_back();
    return true;
  }
};

// standard message tags (note that you don't need to worry about
// "senduser" or "empty" messages)
#define TAG_ERR       "err"       // protocol error
#define TAG_OK        "ok"        // success response
#define TAG_SLOGIN    "slogin"    // register as specific user for sending
#define TAG_RLOGIN    "rlogin"    // register as specific user for receiving
#define TAG_JOIN      "join"      // join a chat room
#define TAG_LEAVE     "leave"     // leave a chat room
#define TAG_SENDALL   "sendall"   // send message to all users in chat room
#define TAG_SENDUSER  "senduser"  // send message to specific user in chat room
#define TAG_QUIT      "quit"      // quit
#define TAG_DELIVERY  "delivery"  // message delivered by server to receiving client
#define TAG_EMPTY     "empty"     // sent by server to receiving client to indicate no msgs available

#endif // MESSAGE_H

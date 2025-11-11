#include <sstream>
#include <cctype>
#include <cassert>
#include "csapp.h"
#include "message.h"
#include "connection.h"

Connection::Connection()
  : m_fd(-1)
  , m_last_result(SUCCESS) {
}

Connection::Connection(int fd)
  : m_fd(fd)
  , m_last_result(SUCCESS) {
  // TODO: call rio_readinitb to initialize the rio_t object
  rio_readinitb(&m_fdbuf, fd);

}

void Connection::connect(const std::string &hostname, int port) {
  // TODO: call open_clientfd to connect to the server
  // TODO: call rio_readinitb to initialize the rio_t object
  std::ostringstream oss;
  oss << port;
  std::string port_str = oss.str();

  m_fd = open_clientfd(const_cast<char *>(hostname.c_str()), const_cast<char *>(port_str.c_str()));
  if (m_fd < 0) {
    m_last_result = EOF_OR_ERROR;
    throw std::runtime_error("Failed to connect to server");
  }
  rio_readinitb(&m_fdbuf, m_fd);
  m_last_result = SUCCESS;
}

Connection::~Connection() {
  // TODO: close the socket if it is open
  close();
}

bool Connection::is_open() const {
  // TODO: return true if the connection is open
  return m_fd >= 0;
}

void Connection::close() {
  // TODO: close the connection if it is open
  if (m_fd >= 0) {
    ::close(m_fd);
    m_fd = -1;
  }
}

bool Connection::send(const Message &msg) {
  // TODO: send a message
  // return true if successful, false if not
  // make sure that m_last_result is set appropriately
  std::string encoded = msg.encode();
  if (encoded.size() > Message::MAX_LEN) {
    m_last_result = INVALID_MSG;
    return false;
  }
  ssize_t n = rio_writen(m_fd, const_cast<char *>(encoded.c_str()), encoded.size());
  if (n != (ssize_t)encoded.size()) {
    m_last_result = EOF_OR_ERROR;
    return false;
  }
  m_last_result = SUCCESS;
  return true;
}

bool Connection::receive(Message &msg) {
  // TODO: receive a message, storing its tag and data in msg
  // return true if successful, false if not
  // make sure that m_last_result is set appropriately
  char buf[Message::MAX_LEN + 1];
  ssize_t n = rio_readlineb(&m_fdbuf, buf, Message::MAX_LEN);
  if (n <= 0) {
    m_last_result = EOF_OR_ERROR;
    return false;
  }
  buf[n] = '\0';
  std::string line(buf);
  if (!msg.decode(line)) {
    m_last_result = INVALID_MSG;
    return false;
  }
  m_last_result = SUCCESS;
  return true;
}

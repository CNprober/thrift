/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _THRIFT_TRANSPORT_TSOCKET_H_
#define _THRIFT_TRANSPORT_TSOCKET_H_ 1

#include <string>

#include <thrift/transport/TTransport.h>
#include <thrift/transport/TVirtualTransport.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/PlatformSocket.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

namespace apache { namespace thrift { namespace transport {

/**
 * TCP Socket implementation of the TTransport interface.
 *TCP套接字封装
 */
class TSocket : public TVirtualTransport<TSocket> {
 public:
  /**
   * Constructs a new socket. Note that this does NOT actually connect the
   * socket.
   *建立一个新的套接字,但是不连接
   */
  TSocket();

  /**
   * Constructs a new socket. Note that this does NOT actually connect the
   * socket.
   *
   * @param host An IP address or hostname to connect to
   * @param port The port to connect on
   */
  TSocket(std::string host, int port);

  /**
   * Constructs a new Unix domain socket.
   * Note that this does NOT actually connect the socket.
   *Unix域套接字
   * @param path The Unix domain socket e.g. "/tmp/ThriftTest.binary.thrift"
   */
  TSocket(std::string path);

  /**
   * Destroyes the socket object, closing it if necessary.
   */
  virtual ~TSocket();

  /**
   * Whether the socket is alive.
   *
   * @return Is the socket alive?
   */
  virtual bool isOpen();

  /**看socket是否有数据可读
   * Calls select on the socket to see if there is more data available.
   */
  virtual bool peek();

  /**
   * Creates and opens the UNIX socket.
   *新建连接
   * @throws TTransportException If the socket could not connect
   */
  virtual void open();

  /**
   * Shuts down communications on the socket. 断开连接
   */
  virtual void close();

  /**
   * Reads from the underlying socket. 从底层socket读取数据
   */
  virtual uint32_t read(uint8_t* buf, uint32_t len);

  /**
   * Writes to the underlying socket.  Loops until done or fail. 向底层socket写数据,循环写直到写完或者失败
   */
  virtual void write(const uint8_t* buf, uint32_t len);

  /** partial 部分
   * Writes to the underlying socket.  Does single send() and returns result. 向底层写数据,调用一次send,返回数据
   */
  uint32_t write_partial(const uint8_t* buf, uint32_t len);

  /**
   * Get the host that the socket is connected to
   *获取对端socket的host, 返回host标识串
   * @return string host identifier
   */
  std::string getHost();

  /**
   * Get the port that the socket is connected to
   *获取对端sockt的端口
   * @return int port number
   */
  int getPort();

  /**
   * Set the host that socket will connect to
   *设置对端host
   * @param host host identifier
   */
  void setHost(std::string host);

  /**
   * Set the port that socket will connect to
   *设置对端端口
   * @param port port number
   */
  void setPort(int port);

  /** linger 逗留,徘徊,慢慢消失, 这个选项是控制断开连接时是否继续发送缓冲区数据
   * Controls whether the linger option is set on the socket.
   *SO_LINGER选项开关 
   * @param on      Whether SO_LINGER is on
   * @param linger  If linger is active, the number of seconds to linger for
   */
  void setLinger(bool on, int linger);

  /**
   * Whether to enable/disable Nagle's algorithm. Nagle算法
   *Nagle算法只允许一个未被ACK的小包存在于网络, 这避免了大量的小包充斥网络. 提高了网络吞吐率,但是增加了网络时延
   * @param noDelay Whether or not to disable the algorithm. 对时延敏感的应用要关闭Nagle算法, 即设置TCP_NODELAY选项
   * @return
   */
  void setNoDelay(bool noDelay);

  /**
   * Set the connect timeout connect超时时间
   */
  void setConnTimeout(int ms);

  /**
   * Set the receive timeout receive超时时间
   */
  void setRecvTimeout(int ms);

  /**
   * Set the send timeout send超时时间
   */
  void setSendTimeout(int ms);

  /**
   * Set the max number of recv retries in case of an THRIFT_EAGAIN recv的重试次数
   * error
   */
  void setMaxRecvRetries(int maxRecvRetries);

  /**
   * Get socket information formated as a string <Host: x Port: x> socket信息
   */
  std::string getSocketInfo();

  /**
   * Returns the DNS name of the host to which the socket is connected 对端DNS转换的URL
   */
  std::string getPeerHost();

  /**
   * Returns the address of the host to which the socket is connected 对端地址
   */
  std::string getPeerAddress();

  /**
   * Returns the port of the host to which the socket is connected 对端端口
   **/
  int getPeerPort();

  /**
   * Returns the underlying socket file descriptor. 返回底层socket结构体对象, 非windows就是int
   */
  THRIFT_SOCKET getSocketFD() {
    return socket_;
  }

  /**
   * (Re-)initialize a TSocket for the supplied descriptor.  This is only
   * intended for use by TNonblockingServer -- other use may result in
   * unfortunate surprises. 用一个描述符初始化/重新初始化TSocket结构, 只能用于TNonblockingServer
   *
   * @param fd the descriptor for an already-connected socket 参数是一个已经连接的socket套接字
   */
  void setSocketFD(THRIFT_SOCKET fd);

  /*
   * Returns a cached copy of the peer address. 对端地址的拷贝
   */
  sockaddr* getCachedAddress(socklen_t* len) const;

  /**
   * Sets whether to use a low minimum TCP retransmission timeout. 设置是否使用一个较低的最小TCP重传超时时间
   */
  static void setUseLowMinRto(bool useLowMinRto);

  /**
   * Gets whether to use a low minimum TCP retransmission timeout. 获取是否使用...
   */
  static bool getUseLowMinRto();

  /**
   * Constructor to create socket from raw UNIX handle. 构造杉树
   */
  TSocket(THRIFT_SOCKET socket);

  /**
   * Set a cache of the peer address (used when trivially available: e.g. 设置对端地址, 一般情况下用在accept, connect等
   * accept() or connect()). Only caches IPV4 and IPV6; unset for others.
   */
  void setCachedAddress(const sockaddr* addr, socklen_t len);

 protected:
  /** connect, called by open */
  void openConnection(struct addrinfo *res); //open调用，连接

  /** Host to connect to */
  std::string host_;

  /** Peer hostname */
  std::string peerHost_;

  /** Peer address */
  std::string peerAddress_;

  /** Peer port */
  int peerPort_;

  /** Port number to connect on */
  int port_;

  /** UNIX domain socket path */
  std::string path_;

  /** Underlying UNIX socket handle */
  THRIFT_SOCKET socket_;

  /** Connect timeout in ms */
  int connTimeout_;

  /** Send timeout in ms */
  int sendTimeout_;

  /** Recv timeout in ms */
  int recvTimeout_;

  /** Linger on */
  bool lingerOn_;

  /** Linger val */
  int lingerVal_;

  /** Nodelay */
  bool noDelay_;

  /** Recv EGAIN retries */
  int maxRecvRetries_;

  /** Recv timeout timeval */
  struct timeval recvTimeval_;

  /** Cached peer address */
  union {
    sockaddr_in ipv4;
    sockaddr_in6 ipv6;
  } cachedPeerAddr_;

  /** Whether to use low minimum TCP retransmission timeout */
  static bool useLowMinRto_;

 private:
  void unix_open();
  void local_open();
};

}}} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TSOCKET_H_


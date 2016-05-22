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
 *TCP�׽��ַ�װ
 */
class TSocket : public TVirtualTransport<TSocket> {
 public:
  /**
   * Constructs a new socket. Note that this does NOT actually connect the
   * socket.
   *����һ���µ��׽���,���ǲ�����
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
   *Unix���׽���
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

  /**��socket�Ƿ������ݿɶ�
   * Calls select on the socket to see if there is more data available.
   */
  virtual bool peek();

  /**
   * Creates and opens the UNIX socket.
   *�½�����
   * @throws TTransportException If the socket could not connect
   */
  virtual void open();

  /**
   * Shuts down communications on the socket. �Ͽ�����
   */
  virtual void close();

  /**
   * Reads from the underlying socket. �ӵײ�socket��ȡ����
   */
  virtual uint32_t read(uint8_t* buf, uint32_t len);

  /**
   * Writes to the underlying socket.  Loops until done or fail. ��ײ�socketд����,ѭ��дֱ��д�����ʧ��
   */
  virtual void write(const uint8_t* buf, uint32_t len);

  /** partial ����
   * Writes to the underlying socket.  Does single send() and returns result. ��ײ�д����,����һ��send,��������
   */
  uint32_t write_partial(const uint8_t* buf, uint32_t len);

  /**
   * Get the host that the socket is connected to
   *��ȡ�Զ�socket��host, ����host��ʶ��
   * @return string host identifier
   */
  std::string getHost();

  /**
   * Get the port that the socket is connected to
   *��ȡ�Զ�sockt�Ķ˿�
   * @return int port number
   */
  int getPort();

  /**
   * Set the host that socket will connect to
   *���öԶ�host
   * @param host host identifier
   */
  void setHost(std::string host);

  /**
   * Set the port that socket will connect to
   *���öԶ˶˿�
   * @param port port number
   */
  void setPort(int port);

  /** linger ����,�ǻ�,������ʧ, ���ѡ���ǿ��ƶϿ�����ʱ�Ƿ�������ͻ���������
   * Controls whether the linger option is set on the socket.
   *SO_LINGERѡ��� 
   * @param on      Whether SO_LINGER is on
   * @param linger  If linger is active, the number of seconds to linger for
   */
  void setLinger(bool on, int linger);

  /**
   * Whether to enable/disable Nagle's algorithm. Nagle�㷨
   *Nagle�㷨ֻ����һ��δ��ACK��С������������, ������˴�����С���������. ���������������,��������������ʱ��
   * @param noDelay Whether or not to disable the algorithm. ��ʱ�����е�Ӧ��Ҫ�ر�Nagle�㷨, ������TCP_NODELAYѡ��
   * @return
   */
  void setNoDelay(bool noDelay);

  /**
   * Set the connect timeout connect��ʱʱ��
   */
  void setConnTimeout(int ms);

  /**
   * Set the receive timeout receive��ʱʱ��
   */
  void setRecvTimeout(int ms);

  /**
   * Set the send timeout send��ʱʱ��
   */
  void setSendTimeout(int ms);

  /**
   * Set the max number of recv retries in case of an THRIFT_EAGAIN recv�����Դ���
   * error
   */
  void setMaxRecvRetries(int maxRecvRetries);

  /**
   * Get socket information formated as a string <Host: x Port: x> socket��Ϣ
   */
  std::string getSocketInfo();

  /**
   * Returns the DNS name of the host to which the socket is connected �Զ�DNSת����URL
   */
  std::string getPeerHost();

  /**
   * Returns the address of the host to which the socket is connected �Զ˵�ַ
   */
  std::string getPeerAddress();

  /**
   * Returns the port of the host to which the socket is connected �Զ˶˿�
   **/
  int getPeerPort();

  /**
   * Returns the underlying socket file descriptor. ���صײ�socket�ṹ�����, ��windows����int
   */
  THRIFT_SOCKET getSocketFD() {
    return socket_;
  }

  /**
   * (Re-)initialize a TSocket for the supplied descriptor.  This is only
   * intended for use by TNonblockingServer -- other use may result in
   * unfortunate surprises. ��һ����������ʼ��/���³�ʼ��TSocket�ṹ, ֻ������TNonblockingServer
   *
   * @param fd the descriptor for an already-connected socket ������һ���Ѿ����ӵ�socket�׽���
   */
  void setSocketFD(THRIFT_SOCKET fd);

  /*
   * Returns a cached copy of the peer address. �Զ˵�ַ�Ŀ���
   */
  sockaddr* getCachedAddress(socklen_t* len) const;

  /**
   * Sets whether to use a low minimum TCP retransmission timeout. �����Ƿ�ʹ��һ���ϵ͵���СTCP�ش���ʱʱ��
   */
  static void setUseLowMinRto(bool useLowMinRto);

  /**
   * Gets whether to use a low minimum TCP retransmission timeout. ��ȡ�Ƿ�ʹ��...
   */
  static bool getUseLowMinRto();

  /**
   * Constructor to create socket from raw UNIX handle. ����ɼ��
   */
  TSocket(THRIFT_SOCKET socket);

  /**
   * Set a cache of the peer address (used when trivially available: e.g. ���öԶ˵�ַ, һ�����������accept, connect��
   * accept() or connect()). Only caches IPV4 and IPV6; unset for others.
   */
  void setCachedAddress(const sockaddr* addr, socklen_t len);

 protected:
  /** connect, called by open */
  void openConnection(struct addrinfo *res); //open���ã�����

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


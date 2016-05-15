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

#ifndef _THRIFT_TRANSPORT_TTRANSPORT_H_
#define _THRIFT_TRANSPORT_TTRANSPORT_H_ 1

#include <thrift/Thrift.h>
#include <boost/shared_ptr.hpp>
#include <thrift/transport/TTransportException.h>
#include <string>

namespace apache { namespace thrift { namespace transport {

/**
 * Helper template to hoist readAll implementation out of TTransport 辅助函数,辅助调用Transport的readAll函数
 */
template <class Transport_>
uint32_t readAll(Transport_ &trans, uint8_t* buf, uint32_t len) {
  uint32_t have = 0;
  uint32_t get = 0;

  while (have < len) {
    get = trans.read(buf+have, len-have);
    if (get <= 0) {
      throw TTransportException(TTransportException::END_OF_FILE,
                                "No more data to read.");
    }
    have += get;
  }

  return have;
}


/**
 * Generic interface for a method of transporting data. A TTransport may be
 * capable of either reading or writing, but not necessarily both.
 * 传输数据抽象接口,读写二选一,不能是两者兼得
 */
class TTransport {
 public:
  /**
   * Virtual deconstructor.
   */
  virtual ~TTransport() {}

  /**
   * Whether this transport is open.
   */
  virtual bool isOpen() {
    return false;
  }

  /**
   * Tests whether there is more data to read or if the remote side is
   * still open. By default this is true whenever the transport is open,
   * but implementations should add logic to test for this condition where
   * possible (i.e. on a socket).
   * This is used by a server to check if it should listen for another
   * request. 用于server类检查是否应该监听另外一个请求
   */
  virtual bool peek() {
    return isOpen();
  }

  /**
   * Opens the transport for communications.
   *
   * @return bool Whether the transport was successfully opened
   * @throws TTransportException if opening failed
   */
  virtual void open() {
    throw TTransportException(TTransportException::NOT_OPEN, "Cannot open base TTransport.");
  }

  /**
   * Closes the transport.
   */
  virtual void close() {
    throw TTransportException(TTransportException::NOT_OPEN, "Cannot close base TTransport.");
  }

  /**
   * Attempt to read up to the specified number of bytes into the string. 尝试读取特定字节数的内容
   *
   * @param buf  Reference to the location to write the data
   * @param len  How many bytes to read
   * @return How many bytes were actually read 返回实际读取的字节数
   * @throws TTransportException If an error occurs
   */
  uint32_t read(uint8_t* buf, uint32_t len) {
    T_VIRTUAL_CALL();
    return read_virt(buf, len);
  }
  virtual uint32_t read_virt(uint8_t* /* buf */, uint32_t /* len */) {
    throw TTransportException(TTransportException::NOT_OPEN,
                              "Base TTransport cannot read.");
  }

  /**
   * Reads the given amount of data in its entirety no matter what. 整个读取
   *
   * @param s     Reference to location for read data
   * @param len   How many bytes to read
   * @return How many bytes read, which must be equal to size	读取的字节数,必须等于参数len
   * @throws TTransportException If insufficient data was read 如果没有读取到足够的数据则抛出异常
   */
  uint32_t readAll(uint8_t* buf, uint32_t len) {
    T_VIRTUAL_CALL();
    return readAll_virt(buf, len);
  }
  virtual uint32_t readAll_virt(uint8_t* buf, uint32_t len) {
    return apache::thrift::transport::readAll(*this, buf, len);
  }

  /**
   * Called when read is completed.
   * This can be over-ridden to perform a transport-specific action
   * e.g. logging the request to a file 读取结束会调用这个函数,可以用来执行一些特定transport的动作,如记录日志
   *
   * @return number of bytes read if available, 0 otherwise. 
   */
  virtual uint32_t readEnd() {
    // default behaviour is to do nothing
    return 0;
  }

  /**
   * Writes the string in its entirety to the buffer. 写整个数据,之后一定要flush,否则transport不保证数据可达
   *
   * Note: You must call flush() to ensure the data is actually written,
   * and available to be read back in the future.  Destroying a TTransport
   * object does not automatically flush pending data--if you destroy a
   * TTransport object with written but unflushed data, that data may be
   * discarded.
   *
   * @param buf  The data to write out
   * @throws TTransportException if an error occurs
   */
  void write(const uint8_t* buf, uint32_t len) {
    T_VIRTUAL_CALL();
    write_virt(buf, len);
  }
  virtual void write_virt(const uint8_t* /* buf */, uint32_t /* len */) {
    throw TTransportException(TTransportException::NOT_OPEN,
                              "Base TTransport cannot write.");
  }

  /**
   * Called when write is completed.
   * This can be over-ridden to perform a transport-specific action
   * at the end of a request. 写结束自动调用这个函数
   *
   * @return number of bytes written if available, 0 otherwise
   */
  virtual uint32_t writeEnd() {
    // default behaviour is to do nothing
    return 0;
  }

  /**
   * Flushes any pending data to be written. Typically used with buffered
   * transport mechanisms. 刷新写入挂起的数据,通常在带缓冲的transport中使用
   *
   * @throws TTransportException if an error occurs
   */
  virtual void flush() {
    // default behaviour is to do nothing
  }

  /**
   * Attempts to return a pointer to \c len bytes, possibly copied into \c buf.
   * Does not consume the bytes read (i.e.: a later read will return the same
   * data).  This method is meant to support protocols that need to read
   * variable-length fields.  They can attempt to borrow the maximum amount of
   * data that they will need, then consume (see next method) what they
   * actually use.  Some transports will not support this method and others
   * will fail occasionally, so protocols must be prepared to use read if
   * borrow fails. 尝试读取数据到buf里,但是不消费read到的数据;这个方法用于支持可变长度的协议处理
   *先读取填所需的最大长度的字节,然后分析之后,只消费他们实际所需的字节数
   * @oaram buf  A buffer where the data can be stored if needed.
   *             If borrow doesn't return buf, then the contents of
   *             buf after the call are undefined.  This parameter may be
   *             NULL to indicate that the caller is not supplying storage,
   *             but would like a pointer into an internal buffer, if
   *             available. 借来的数据存储在这里.如果不能借,这个返回参数的值是未定义的;也可以置表示transport不支持存储,希望直接指针操作中间缓存区
   * @param len  *len should initially contain the number of bytes to borrow.
   *             If borrow succeeds, *len will contain the number of bytes 初始是要借的字节数,借成功之后,是实际借来可用的字节数
   *             available in the returned pointer.  This will be at least 
   *             what was requested, but may be more if borrow returns 至少是初始要借的字节大小.如果支持直接操作中间缓冲区,len会比初始值更大
   *             a pointer to an internal buffer, rather than buf. 
   *             If borrow fails, the contents of *len are undefined. 如果借失败, len未定义
   * @return If the borrow succeeds, return a pointer to the borrowed data. 能借返回参数buf,不能借则返回指向transport的中间缓冲区
   *         This might be equal to \c buf, or it might be a pointer into
   *         the transport's internal buffers.
   * @throws TTransportException if an error occurs
   */
  const uint8_t* borrow(uint8_t* buf, uint32_t* len) {
    T_VIRTUAL_CALL();
    return borrow_virt(buf, len);
  }
  virtual const uint8_t* borrow_virt(uint8_t* /* buf */, uint32_t* /* len */) {
    return NULL;
  }

  /**
   * Remove len bytes from the transport.  This should always follow a borrow
   * of at least len bytes, and should always succeed. 从read来的缓冲区中移除len字节数
   * TODO(dreiss): Is there any transport that could borrow but fail to
   * consume, or that would require a buffer to dump the consumed data?
   *
   * @param len  How many bytes to consume
   * @throws TTransportException If an error occurs
   */
  void consume(uint32_t len) {
    T_VIRTUAL_CALL();
    consume_virt(len);
  }
  virtual void consume_virt(uint32_t /* len */) {
    throw TTransportException(TTransportException::NOT_OPEN,
                              "Base TTransport cannot consume.");
  }

 protected:
  /**
   * Simple constructor.
   */
  TTransport() {}
};

/**
 * Generic factory class to make an input and output transport out of a
 * source transport. Commonly used inside servers to make input and output
 * streams out of raw clients. 工厂,server中用于使用Transport制造in和out 流.
 *
 */
class TTransportFactory {
 public:
  TTransportFactory() {}

  virtual ~TTransportFactory() {}

  /**
   * Default implementation does nothing, just returns the transport given. 默认直接返回给定的Transport实例
   */
  virtual boost::shared_ptr<TTransport> getTransport(boost::shared_ptr<TTransport> trans) {
    return trans;
  }

};

}}} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TTRANSPORT_H_

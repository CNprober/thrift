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

#ifndef _THRIFT_TRANSPORT_TVIRTUALTRANSPORT_H_
#define _THRIFT_TRANSPORT_TVIRTUALTRANSPORT_H_ 1

#include <thrift/transport/TTransport.h>

namespace apache { namespace thrift { namespace transport {


/**
 * Helper class that provides default implementations of TTransport methods.
 * 提供TTransport的read, readAll, write, borrow, consume接口的默认实现
 * This class provides default implementations of read(), readAll(), write(),
 * borrow() and consume().
 *
 * In the TTransport base class, each of these methods simply invokes its
 * virtual counterpart.  This class overrides them to always perform the
 * default behavior, without a virtual function call. 在TTransport基类中这5个函数只是简单调用虚函数的版本
 *在这个类中,这些函数被覆盖为默认实现,而不是调用虚函数(但是其实还是调用虚函数啊, 估计是后来改了实现但是没改注释,汗)
 * The primary purpose of this class is to serve as a base class for
 * TVirtualTransport, and prevent infinite recursion if one of its subclasses
 * does not override the TTransport implementation of these methods.  (Since
 * TVirtualTransport::read_virt() calls read(), and TTransport::read() calls
 * read_virt().) 这个类的主要目的是作为TVirtualTransport类的几类,这样可以阻止由于子类没有覆盖TTransport的这5个函数时发生的无限循环,即TVT的read_virt调用TT的read,而TT的read又调用read_vitr
 *///但是这里的实现依旧是调用virtual副本函数,猜测原本不是这样实现的
class TTransportDefaults : public TTransport {
 public:
  /*
   * TTransport *_virt() methods provide reasonable default implementations.
   * Invoke them non-virtually.
   */
  uint32_t read(uint8_t* buf, uint32_t len) {
    return this->TTransport::read_virt(buf, len);
  }
  uint32_t readAll(uint8_t* buf, uint32_t len) {
    return this->TTransport::readAll_virt(buf, len);
  }
  void write(const uint8_t* buf, uint32_t len) {
    this->TTransport::write_virt(buf, len);
  }
  const uint8_t* borrow(uint8_t* buf, uint32_t* len) {
    return this->TTransport::borrow_virt(buf, len);
  }
  void consume(uint32_t len) {
    this->TTransport::consume_virt(len);
  }

 protected:
  TTransportDefaults() {}
};

/**
 * Helper class to provide polymorphism for subclasses of TTransport.
 *提供TTransport子类多态性的辅助类
 * This class implements *_virt() methods of TTransport, to call the
 * non-virtual versions of these functions in the proper subclass.
 *这个类实现virt版本函数, 这些函数调用非virt版本
 * To define your own transport class using TVirtualTransport:
 * 1) Derive your subclass from TVirtualTransport<your class>
 *    e.g:  class MyTransport : public TVirtualTransport<MyTransport> {
 * 2) Provide your own implementations of read(), readAll(), etc.
 *    These methods should be non-virtual.
 *所以自己实现的transsport函数需要实现read,readAll等函数,且不能是virtual的
 * Transport implementations that need to use virtual inheritance when
 * inheriting from TTransport cannot use TVirtualTransport.
 *Transport如果通过虚拟继承TTransport类则不能再继承TVirtualTransport ??不懂 
 * @author Chad Walters <chad@powerset.com>
 */
template <class Transport_, class Super_=TTransportDefaults>
class TVirtualTransport : public Super_ {
 public:
  /*
   * Implementations of the *_virt() functions, to call the subclass's
   * non-virtual implementation function.
   */
  virtual uint32_t read_virt(uint8_t* buf, uint32_t len) {
    return static_cast<Transport_*>(this)->read(buf, len);
  }

  virtual uint32_t readAll_virt(uint8_t* buf, uint32_t len) {
    return static_cast<Transport_*>(this)->readAll(buf, len);
  }

  virtual void write_virt(const uint8_t* buf, uint32_t len) {
    static_cast<Transport_*>(this)->write(buf, len);
  }

  virtual const uint8_t* borrow_virt(uint8_t* buf, uint32_t* len) {
    return static_cast<Transport_*>(this)->borrow(buf, len);
  }

  virtual void consume_virt(uint32_t len) {
    static_cast<Transport_*>(this)->consume(len);
  }

  /*
   * Provide a default readAll() implementation that invokes
   * read() non-virtually.
   *
   * Note: subclasses that use TVirtualTransport to derive from another
   * transport implementation (i.e., not TTransportDefaults) should beware that
   * this may override any non-default readAll() implementation provided by
   * the parent transport class.  They may need to redefine readAll() to call
   * the correct parent implementation, if desired.
   */
  uint32_t readAll(uint8_t* buf, uint32_t len) {
    Transport_* trans = static_cast<Transport_*>(this);
    return ::apache::thrift::transport::readAll(*trans, buf, len);
  }

 protected:
  TVirtualTransport() {}

  /*
   * Templatized constructors, to allow arguments to be passed to the Super_
   * constructor.  Currently we only support 0, 1, or 2 arguments, but
   * additional versions can be added as needed.
   */
  template <typename Arg_>
  TVirtualTransport(Arg_ const& arg) : Super_(arg) { }

  template <typename Arg1_, typename Arg2_>
  TVirtualTransport(Arg1_ const& a1, Arg2_ const& a2) : Super_(a1, a2) { }
};

}}} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TVIRTUALTRANSPORT_H_

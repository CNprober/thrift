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

#ifndef _THRIFT_CONCURRENCY_MONITOR_H_
#define _THRIFT_CONCURRENCY_MONITOR_H_ 1

#include <thrift/concurrency/Exception.h>
#include <thrift/concurrency/Mutex.h>

#include <boost/utility.hpp>


namespace apache { namespace thrift { namespace concurrency {

/**
 * A monitor is a combination mutex and condition-event.  Waiting and
 * notifying condition events requires that the caller own the mutex.  Mutex
 * lock and unlock operations can be performed independently of condition
 * events.  This is more or less analogous to java.lang.Object multi-thread
 * operations. 互斥量和条件变量的结合体, 等待和通知事件需要调用者拥有互斥量
 *
 * Note the Monitor can create a new, internal mutex; alternatively, a
 * separate Mutex can be passed in and the Monitor will re-use it without
 * taking ownership.  It's the user's responsibility to make sure that the
 * Mutex is not deallocated before the Monitor. Monitor可以创建一个互斥量也可以接受传入的互斥量
 *
 * Note that all methods are const.  Monitors implement logical constness, not
 * bit constness.  This allows const methods to call monitor methods without
 * needing to cast away constness or change to non-const signatures. 逻辑const语义
 *
 * @version $Id:$
 */
class Monitor : boost::noncopyable {
 public:
  /** Creates a new mutex, and takes ownership of it. */
  Monitor();

  /** Uses the provided mutex without taking ownership. */
  explicit Monitor(Mutex* mutex);

  /** Uses the mutex inside the provided Monitor without taking ownership. */
  explicit Monitor(Monitor* monitor);

  /** Deallocates the mutex only if we own it. */
  virtual ~Monitor();

  Mutex& mutex() const;

  virtual void lock() const;

  virtual void unlock() const;

  /**
   * Waits a maximum of the specified timeout in milliseconds for the condition
   * to occur, or waits forever if timeout_ms == 0.
   *
   * Returns 0 if condition occurs, THRIFT_ETIMEDOUT on timeout, or an error code.
   */
  int waitForTimeRelative(int64_t timeout_ms) const; //等待事件, 最多timeout_ms之后返回

  /**
   * Waits until the absolute time specified using struct THRIFT_TIMESPEC.
   * Returns 0 if condition occurs, THRIFT_ETIMEDOUT on timeout, or an error code.
   */
  int waitForTime(const THRIFT_TIMESPEC* abstime) const; //等待事件, 最迟到abstime绝对时间点

  /**
   * Waits until the absolute time specified using struct timeval.
   * Returns 0 if condition occurs, THRIFT_ETIMEDOUT on timeout, or an error code.
   */
  int waitForTime(const struct timeval* abstime) const; //等待事件,最迟到abstime绝对时间点

  /**
   * Waits forever until the condition occurs.
   * Returns 0 if condition occurs, or an error code otherwise.
   */
  int waitForever() const;  //永久等待

  /**
   * Exception-throwing version of waitForTimeRelative(), called simply
   * wait(int64) for historical reasons.  Timeout is in milliseconds.
   *
   * If the condition occurs,  this function returns cleanly; on timeout or
   * error an exception is thrown.
   */
  void wait(int64_t timeout_ms = 0LL) const;    //第一个等待函数的抛出异常版本


  /** Wakes up one thread waiting on this monitor. */
  virtual void notify() const;      //唤醒等待这个monitor的一个线程

  /** Wakes up all waiting threads on this monitor. */
  virtual void notifyAll() const;   //唤醒所有等待这个monitor的线程

 private:

  class Impl;

  Impl* impl_;
};

class Synchronized {    //同步
 public:
 Synchronized(const Monitor* monitor) : g(monitor->mutex()) { }
 Synchronized(const Monitor& monitor) : g(monitor.mutex()) { }

 private:
  Guard g;  //简单互斥量守卫, 构造函数中锁住mutex, 析构时解锁
};


}}} // apache::thrift::concurrency

#endif // #ifndef _THRIFT_CONCURRENCY_MONITOR_H_

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

#ifndef _THRIFT_CONCURRENCY_THREADMANAGER_H_
#define _THRIFT_CONCURRENCY_THREADMANAGER_H_ 1

#include <boost/shared_ptr.hpp>
#include <thrift/cxxfunctional.h>
#include <sys/types.h>
#include <thrift/concurrency/Thread.h>

namespace apache { namespace thrift { namespace concurrency {

/**
 * Thread Pool Manager and related classes
 *
 * @version $Id:$
 */
class ThreadManager;

/**
 * ThreadManager class
 *
 * This class manages a pool of threads. It uses a ThreadFactory to create
 * threads. It never actually creates or destroys worker threads, rather
 * It maintains statistics on number of idle threads, number of active threads,
 * task backlog, and average wait and service times and informs the PoolPolicy
 * object bound to instances of this manager of interesting transitions. It is
 * then up the PoolPolicy object to decide if the thread pool size needs to be
 * adjusted and call this object addWorker and removeWorker methods to make
 * changes. 线程池类,使用线程工厂创建工作线程.他并不真正的创建或者摧毁线程，而是维护空闲线程，活动线程，积压任务，平均等待时间，服务时间等数据，
 *然后通知绑定到这个线程池实例的PoolPolicy实例,以便其对线程数量进行动态调整
 * This design allows different policy implementations to used this code to
 * handle basic worker thread management and worker task execution and focus on
 * policy issues. The simplest policy, StaticPolicy, does nothing other than
 * create a fixed number of threads.这种设计允许不同的线程管理策略对工作线程进行管理，最简单的线程管理就是创建了固定数量的线程之后，什么也不做。
 */
class ThreadManager {

 protected:
  ThreadManager() {}

 public:
  class Task;
  typedef apache::thrift::stdcxx::function<void(boost::shared_ptr<Runnable>)> ExpireCallback;   //定义函数指针, ExpireCallback, 返回值void, 参数share_ptr<Runnable>

  virtual ~ThreadManager() {}

  /**
   * Starts the thread manager. Verifies all attributes have been properly
   * initialized, then allocates necessary resources to begin operation
   */
  virtual void start() = 0;

  /**
   * Stops the thread manager. Aborts all remaining unprocessed task, shuts
   * down all created worker threads, and realeases all allocated resources.
   * This method blocks for all worker threads to complete, thus it can
   * potentially block forever if a worker thread is running a task that
   * won't terminate. 丢弃所有没有处理到任务,关闭惯有线程,释放所有资源,阻塞直到所有线程完成.因此有可能永远阻塞
   */
  virtual void stop() = 0;

  /**
   * Joins the thread manager. This is the same as stop, except that it will
   * block until all the workers have finished their work. At that point
   * the ThreadManager will transition into the STOPPED state. 和stop一样,但是阻塞到所有线程处理完任务,更改线程池状态
   */
  virtual void join() = 0;

  enum STATE {
    UNINITIALIZED,
    STARTING,
    STARTED,
    JOINING,
    STOPPING,
    STOPPED
  };

  virtual STATE state() const = 0;

  virtual boost::shared_ptr<ThreadFactory> threadFactory() const = 0;

  virtual void threadFactory(boost::shared_ptr<ThreadFactory> value) = 0;

  virtual void addWorker(size_t value=1) = 0;   //添加线程

  virtual void removeWorker(size_t value=1) = 0;

  /**
   * Gets the current number of idle worker threads
   */
  virtual size_t idleWorkerCount() const = 0;

  /**
   * Gets the current number of total worker threads
   */
  virtual size_t workerCount() const = 0;

  /**
   * Gets the current number of pending tasks
   */
  virtual size_t pendingTaskCount() const  = 0;     //挂起任务数量

  /**
   * Gets the current number of pending and executing tasks
   */
  virtual size_t totalTaskCount() const = 0;    //挂起以及正在执行的任务数量之和

  /**
   * Gets the maximum pending task count.  0 indicates no maximum
   */
  virtual size_t pendingTaskCountMax() const = 0;   //最大挂起任务数量, 0没有上限

  /**
   * Gets the number of tasks which have been expired without being run.
   */
  virtual size_t expiredTaskCount() = 0;    //过期任务数量

  /**
   * Adds a task to be executed at some time in the future by a worker thread.
   *
   * This method will block if pendingTaskCountMax() in not zero and pendingTaskCount()
   * is greater than or equalt to pendingTaskCountMax().  If this method is called in the
   * context of a ThreadManager worker thread it will throw a
   * TooManyPendingTasksException  如果挂起任务数量超过上限, add函数阻塞; 如果在线程池线程中调用add, 会抛出异常
   *
   * @param task  The task to queue for execution
   *
   * @param timeout Time to wait in milliseconds to add a task when a pending-task-count 等待add时间
   * is specified. Specific cases:
   * timeout = 0  : Wait forever to queue task. 0无限等待
   * timeout = -1 : Return immediately if pending task count exceeds specified max -1立即返回
   * @param expiration when nonzero, the number of milliseconds the task is valid 任务有效时间,超时任务被丢弃
   * to be run; if exceeded, the task will be dropped off the queue and not run.
   *
   * @throws TooManyPendingTasksException Pending task count exceeds max pending task count 超上限抛异常
   */
  virtual void add(boost::shared_ptr<Runnable>task,     //添加任务
                   int64_t timeout=0LL,
                   int64_t expiration=0LL) = 0;

  /**
   * Removes a pending task
   */
  virtual void remove(boost::shared_ptr<Runnable> task) = 0;

  /**
   * Remove the next pending task which would be run.
   *
   * @return the task removed.
   */
  virtual boost::shared_ptr<Runnable> removeNextPending() = 0;

  /**
   * Remove tasks from front of task queue that have expired.
   */
  virtual void removeExpiredTasks() = 0;

  /**
   * Set a callback to be called when a task is expired and not run.
   *
   * @param expireCallback a function called with the shared_ptr<Runnable> for
   * the expired task.
   */
  virtual void setExpireCallback(ExpireCallback expireCallback) = 0; //设置任务超时过期的回调

  static boost::shared_ptr<ThreadManager> newThreadManager();   //Impl实例

  /**
   * Creates a simple thread manager the uses count number of worker threads and has
   * a pendingTaskCountMax maximum pending tasks. The default, 0, specified no limit
   * on pending tasks 新建一个有count个线程,最大挂起任务上限pendingTaskCountMax的线程池
   */
  static boost::shared_ptr<ThreadManager> newSimpleThreadManager(size_t count=4, size_t pendingTaskCountMax=0); //SimpleThreadManager实例

  class Task;

  class Worker;

  class Impl;
};

}}} // apache::thrift::concurrency

#endif // #ifndef _THRIFT_CONCURRENCY_THREADMANAGER_H_

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

#include <thrift/thrift-config.h>

#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/Exception.h>
#include <thrift/concurrency/Monitor.h>
#include <thrift/concurrency/Util.h>

#include <boost/shared_ptr.hpp>

#include <assert.h>
#include <queue>
#include <set>

#if defined(DEBUG)
#include <iostream>
#endif //defined(DEBUG)

namespace apache { namespace thrift { namespace concurrency {

using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/**
 * ThreadManager class
 *
 * This class manages a pool of threads. It uses a ThreadFactory to create
 * threads.  It never actually creates or destroys worker threads, rather
 * it maintains statistics on number of idle threads, number of active threads,
 * task backlog, and average wait and service times.
 *
 * @version $Id:$
 */
class ThreadManager::Impl : public ThreadManager  {

 public:
  Impl() :
    workerCount_(0),
    workerMaxCount_(0),
    idleCount_(0),
    pendingTaskCountMax_(0),
    expiredCount_(0),
    state_(ThreadManager::UNINITIALIZED),
    monitor_(&mutex_),
    maxMonitor_(&mutex_) {}

  ~Impl() { stop(); }

  void start();

  void stop() { stopImpl(false); }

  void join() { stopImpl(true); }

  ThreadManager::STATE state() const {
    return state_;
  }

  shared_ptr<ThreadFactory> threadFactory() const {
    Synchronized s(monitor_);   //构造锁住monitor_中的mutex, 析构解锁. 所以其实就是实现了一个线程互斥锁
    return threadFactory_;
  }

  void threadFactory(shared_ptr<ThreadFactory> value) { //threadFactory_是一个shared_ptr,所以ThreadManager使用前一定要调用本函数指定一个TreadFactory
    Synchronized s(monitor_);
    threadFactory_ = value;
  }

  void addWorker(size_t value);

  void removeWorker(size_t value);

  size_t idleWorkerCount() const {
    return idleCount_;
  }

  size_t workerCount() const {
    Synchronized s(monitor_);
    return workerCount_;
  }

  size_t pendingTaskCount() const {
    Synchronized s(monitor_);
    return tasks_.size();
  }

  size_t totalTaskCount() const {
    Synchronized s(monitor_);
    return tasks_.size() + workerCount_ - idleCount_;
  }

  size_t pendingTaskCountMax() const {
    Synchronized s(monitor_);
    return pendingTaskCountMax_;
  }

  size_t expiredTaskCount() {
    Synchronized s(monitor_);
    size_t result = expiredCount_;
    expiredCount_ = 0;
    return result;
  }

  void pendingTaskCountMax(const size_t value) {
    Synchronized s(monitor_);
    pendingTaskCountMax_ = value;
  }

  bool canSleep();

  void add(shared_ptr<Runnable> value, int64_t timeout, int64_t expiration);

  void remove(shared_ptr<Runnable> task);

  shared_ptr<Runnable> removeNextPending();     //大致是获取下一个任务, 但是没有被用到. 估计是有锁的原因,在Worker的run函数中没有调用这个

  void removeExpiredTasks();        //pop过期task,直到当前task为未过期的task, 这个函数没有锁,需要使用者自己加锁

  void setExpireCallback(ExpireCallback expireCallback);

private:
  void stopImpl(bool join);

  size_t workerCount_;      //线程数量
  size_t workerMaxCount_;
  size_t idleCount_;        //空闲线程数量
  size_t pendingTaskCountMax_;  //挂起任务上限
  size_t expiredCount_; //过期任务数量
  ExpireCallback expireCallback_;

  ThreadManager::STATE state_;
  shared_ptr<ThreadFactory> threadFactory_;


  friend class ThreadManager::Task;
  std::queue<shared_ptr<Task> > tasks_; //任务队列
  Mutex mutex_;         //锁定任务队列 tasks_
  Monitor monitor_;     //锁定线程数量等
  Monitor maxMonitor_;  //最大任务队列
  Monitor workerMonitor_;   //锁定线程池 workers_

  friend class ThreadManager::Worker;
  std::set<shared_ptr<Thread> > workers_;   //线程池
  std::set<shared_ptr<Thread> > deadWorkers_;   //僵死线程池
  std::map<const Thread::id_t, shared_ptr<Thread> > idMap_; //线程id-线程map
};

class ThreadManager::Task : public Runnable {

 public:
  enum STATE {
    WAITING,
    EXECUTING,
    CANCELLED,
    COMPLETE
  };

  Task(shared_ptr<Runnable> runnable, int64_t expiration=0LL)  :
    runnable_(runnable),
    state_(WAITING),
    expireTime_(expiration != 0LL ? Util::currentTime() + expiration : 0LL) {}

  ~Task() {}

  void run() {
    if (state_ == EXECUTING) {
      runnable_->run();     //执行塞在Task里的Runnable子类的实例的run方法
      state_ = COMPLETE;
    }
  }

  shared_ptr<Runnable> getRunnable() {
    return runnable_;
  }

  int64_t getExpireTime() const {
    return expireTime_;
  }

 private:
  shared_ptr<Runnable> runnable_;   //目测这又是一个指向自身的指针
  friend class ThreadManager::Worker;
  STATE state_;
  int64_t expireTime_;
};

class ThreadManager::Worker: public Runnable {      //Thread类的核心是一个Runnable的shared_ptr, 因此这里的Worker会被传入Thread中
  enum STATE {
    UNINITIALIZED,
    STARTING,
    STARTED,
    STOPPING,
    STOPPED
  };

 public:
  Worker(ThreadManager::Impl* manager) :
    manager_(manager),
    state_(UNINITIALIZED),
    idle_(false) {}

  ~Worker() {}

 private:
  bool isActive() const {   //worker线程没到最大或者状态为joining且任务不为空
    return
      (manager_->workerCount_ <= manager_->workerMaxCount_) ||
      (manager_->state_ == JOINING && !manager_->tasks_.empty());   //join,需要处理完所有的task; stop,忽略task队列的pendingTask,直接退出
  }

 public:
  /**
   * Worker entry point
   *
   * As long as worker thread is running, pull tasks off the task queue and
   * execute. 从ThreadManager的task queue pop一个task,然后处理 --> 维护taskqueue, mutex_; 维护Thread --> workerMonitor_  但是其实mutex_, monitor_, workerMonitor_是共享同一个mutex的啊
   */
  void run() {
    bool active = false;
    bool notifyManager = false;

    /** 
     * Increment worker semaphore and notify manager if worker count reached
     * desired max //递增信号量, 如果达到上限还需要通知ThreadManager
     *
     * Note: We have to release the monitor and acquire the workerMonitor //获取worker数量需要对monitor_加锁，但是通知ThreadManager需要先释放monitor,
     * since that is what the manager blocks on for worker add/remove // 再获取workerMonitor锁, 因为worker的add和remove会阻塞在workerMonitor锁
     */
    {
      Synchronized s(manager_->monitor_);   //锁定线程池的monitor_, 增加线程池workerCount_计数
      active = manager_->workerCount_ < manager_->workerMaxCount_;
      if (active) {
        manager_->workerCount_++;
        notifyManager = manager_->workerCount_ == manager_->workerMaxCount_;    //相等则notifyManager为true
      }
    }

    if (notifyManager) {    //一次addworker可以新建多个worker, worker依次run起来, 所以开始时会发生workerCount < workerMaxCount的情况
      Synchronized s(manager_->workerMonitor_);//线程池已满,通知manager_
      manager_->workerMonitor_.notify();
      notifyManager = false;
    }

    while (active) {
      shared_ptr<ThreadManager::Task> task;

      /**
       * While holding manager monitor block for non-empty task queue (Also
       * check that the thread hasn't been requested to stop). Once the queue
       * is non-empty, dequeue a task, release monitor, and execute. If the
       * worker max count has been decremented such that we exceed it, mark
       * ourself inactive, decrement the worker count and notify the manager
       * (technically we're notifying the next blocked thread but eventually
       * the manager will see it. 在while循环中循环处理task, 锁定manager_->mutex_; 如果workerMaxCount减小,导致workerCount大于MaxCount[即非active状态],本线程就可以结束运行
       */
      {
        Guard g(manager_->mutex_);  //空任务队列时阻塞; 当队列不空，弹出一个任务执行; 更改线程池线程数量,通知线程池
        active = isActive();

        while (active && manager_->tasks_.empty()) {    //active状态且没有task要处理, 标记当前worker为idle线程,等待task到来
          manager_->idleCount_++;
          idle_ = true;
          manager_->monitor_.wait();            //idle,等待task到来
          active = isActive();
          idle_ = false;
          manager_->idleCount_--;
        }

        if (active) {       //active状态,正常处理
          manager_->removeExpiredTasks();       //无锁,pop掉头部的过期task

          if (!manager_->tasks_.empty()) {
            task = manager_->tasks_.front();        //这里的操作类似manager_->removeNextPending, 但是那个函数是加锁的,而且没有通知机制
            manager_->tasks_.pop();
            if (task->state_ == ThreadManager::Task::WAITING) {
              task->state_ = ThreadManager::Task::EXECUTING;
            }

            /* If we have a pending task max and we just dropped below it, wakeup any
               thread that might be blocked on add. */
            if (manager_->pendingTaskCountMax_ != 0 &&
                manager_->tasks_.size() <= manager_->pendingTaskCountMax_ - 1) {        //task队列之前满了,现在pop出一个之后,就可以通知其他线程可以重新添加新的task了
              manager_->maxMonitor_.notify();
            }
          }
        } else {        //非active状态,退出while循环, 准备线程退出
          idle_ = true;
          manager_->workerCount_--;
          notifyManager = (manager_->workerCount_ == manager_->workerMaxCount_);
        }
      }     //end of Guard(manager_->mutex_) 到此完成pop任务的过程

      if (task) {
        if (task->state_ == ThreadManager::Task::EXECUTING) {
          try {
            task->run();    //最终执行的是Task的run函数
          } catch(...) {
            // XXX need to log this
          }
        }
      }
    }   //end while(active)...

    {
      Synchronized s(manager_->workerMonitor_);     //非active状态(manager状态正常,且线程池已满)才会到这里
      manager_->deadWorkers_.insert(this->thread());    //加入到dead线程队列
      if (notifyManager) {      //当前线程退出之后,如果workerCount和workerMaxCount相等(已满),通知manager
        manager_->workerMonitor_.notify();
      }
    }

    return;
  }

  private:
    ThreadManager::Impl* manager_;
    friend class ThreadManager::Impl;
    STATE state_;
    bool idle_;
};


  void ThreadManager::Impl::addWorker(size_t value) {
  std::set<shared_ptr<Thread> > newThreads;
  for (size_t ix = 0; ix < value; ix++) {
    shared_ptr<ThreadManager::Worker> worker = shared_ptr<ThreadManager::Worker>(new ThreadManager::Worker(this));
    newThreads.insert(threadFactory_->newThread(worker));       //通过threadFactory用worker制造thread, worker也是runnable子类
  }

  {
    Synchronized s(monitor_);
    workerMaxCount_ += value;   //monitor_ <-> workerMaxCount_, workers
    workers_.insert(newThreads.begin(), newThreads.end());  //workers_, set<shared_ptr<Thread> >
  }

  for (std::set<shared_ptr<Thread> >::iterator ix = newThreads.begin(); ix != newThreads.end(); ix++) {
    shared_ptr<ThreadManager::Worker> worker = dynamic_pointer_cast<ThreadManager::Worker, Runnable>((*ix)->runnable());    //智能指针像下类型转换(基类向派生类)
    worker->state_ = ThreadManager::Worker::STARTING;
    (*ix)->start(); //调用thread的start, 具体的thread(如PthreadThread)新建线程,调用绑定的runnable的run接口,最终执行的是worker的run函数
    idMap_.insert(std::pair<const Thread::id_t, shared_ptr<Thread> >((*ix)->getId(), *ix));
  }

  {
    Synchronized s(workerMonitor_);
    while (workerCount_ != workerMaxCount_) {       //在worker的run里会递增workerCount
      workerMonitor_.wait();
    }
  }
}

void ThreadManager::Impl::start() {

  if (state_ == ThreadManager::STOPPED) {
    return;
  }

  {
    Synchronized s(monitor_);
    if (state_ == ThreadManager::UNINITIALIZED) {
      if (!threadFactory_) {
        throw InvalidArgumentException();
      }
      state_ = ThreadManager::STARTED;
      monitor_.notifyAll();
    }

    while (state_ == STARTING) {
      monitor_.wait();
    }
  }
}

void ThreadManager::Impl::stopImpl(bool join) { //stop->false, join->true
  bool doStop = false;
  if (state_ == ThreadManager::STOPPED) {
    return;
  }

  {
    Synchronized s(monitor_);
    if (state_ != ThreadManager::STOPPING &&
        state_ != ThreadManager::JOINING &&
        state_ != ThreadManager::STOPPED) {
      doStop = true;
      state_ = join ? ThreadManager::JOINING : ThreadManager::STOPPING;
    }
  }

  if (doStop) {
    removeWorker(workerCount_);
  }

  // XXX
  // should be able to block here for transition to STOPPED since we're no
  // using shared_ptrs

  {
    Synchronized s(monitor_);
    state_ = ThreadManager::STOPPED;
  }

}

void ThreadManager::Impl::removeWorker(size_t value) {
  std::set<shared_ptr<Thread> > removedThreads;
  {
    Synchronized s(monitor_);
    if (value > workerMaxCount_) {
      throw InvalidArgumentException();
    }

    workerMaxCount_ -= value;

    if (idleCount_ < value) {   //idle的线程比需要减少的线程数多,可以直接通知相关的idle线程退出
      for (size_t ix = 0; ix < idleCount_; ix++) {
        monitor_.notify();
      }
    } else {    //否则通知所有线程??
      monitor_.notifyAll();
    }
  }

  {
    Synchronized s(workerMonitor_);

    while (workerCount_ != workerMaxCount_) {   //等待这些线程退出
      workerMonitor_.wait();
    }

    for (std::set<shared_ptr<Thread> >::iterator ix = deadWorkers_.begin(); ix != deadWorkers_.end(); ix++) {
      idMap_.erase((*ix)->getId());
      workers_.erase(*ix);
    }

    deadWorkers_.clear();
  }
}

  bool ThreadManager::Impl::canSleep() {
    const Thread::id_t id = threadFactory_->getCurrentThreadId();
    return idMap_.find(id) == idMap_.end();
  }

  void ThreadManager::Impl::add(shared_ptr<Runnable> value,
                                int64_t timeout,
                                int64_t expiration) {
    Guard g(mutex_, timeout);

    if (!g) {
      throw TimedOutException();
    }

    if (state_ != ThreadManager::STARTED) {
      throw IllegalStateException("ThreadManager::Impl::add ThreadManager "
                                  "not started");
    }

    removeExpiredTasks();
    if (pendingTaskCountMax_ > 0 && (tasks_.size() >= pendingTaskCountMax_)) {
      if (canSleep() && timeout >= 0) { //可以等待,等待timeout时间
        while (pendingTaskCountMax_ > 0 && tasks_.size() >= pendingTaskCountMax_) {
          // This is thread safe because the mutex is shared between monitors.
          maxMonitor_.wait(timeout);
        }
      } else {  //否则直接抛出异常,add Task失败
        throw TooManyPendingTasksException();
      }
    }

    tasks_.push(shared_ptr<ThreadManager::Task>(new ThreadManager::Task(value, expiration)));

    // If idle thread is available notify it, otherwise all worker threads are
    // running and will get around to this task in time.
    if (idleCount_ > 0) {   //如果有idle线程,唤醒
      monitor_.notify();
    }
  }

void ThreadManager::Impl::remove(shared_ptr<Runnable> task) {   //不能删除已经添加的task
  (void) task;
  Synchronized s(monitor_);
  if (state_ != ThreadManager::STARTED) {
    throw IllegalStateException("ThreadManager::Impl::remove ThreadManager not "
                                "started");
  }
}

boost::shared_ptr<Runnable> ThreadManager::Impl::removeNextPending() {
  Guard g(mutex_);
  if (state_ != ThreadManager::STARTED) {
    throw IllegalStateException("ThreadManager::Impl::removeNextPending "
                                "ThreadManager not started");
  }

  if (tasks_.empty()) {
    return boost::shared_ptr<Runnable>();
  }

  shared_ptr<ThreadManager::Task> task = tasks_.front();
  tasks_.pop();
  
  return task->getRunnable();
}

void ThreadManager::Impl::removeExpiredTasks() {
  int64_t now = 0LL; // we won't ask for the time untile we need it

  // note that this loop breaks at the first non-expiring task
  while (!tasks_.empty()) {
    shared_ptr<ThreadManager::Task> task = tasks_.front();
    if (task->getExpireTime() == 0LL) {
      break;
    }
    if (now == 0LL) {
      now = Util::currentTime();
    }
    if (task->getExpireTime() > now) {
      break;
    }
    if (expireCallback_) {
      expireCallback_(task->getRunnable());
    }
    tasks_.pop();
    expiredCount_++;
  }
}


void ThreadManager::Impl::setExpireCallback(ExpireCallback expireCallback) {
  expireCallback_ = expireCallback;
}

class SimpleThreadManager : public ThreadManager::Impl {

 public:
  SimpleThreadManager(size_t workerCount=4, size_t pendingTaskCountMax=0) :
    workerCount_(workerCount),
    pendingTaskCountMax_(pendingTaskCountMax),
    firstTime_(true) {
  }

  void start() {
    ThreadManager::Impl::pendingTaskCountMax(pendingTaskCountMax_);
    ThreadManager::Impl::start();
    addWorker(workerCount_);
  }

 private:
  const size_t workerCount_;
  const size_t pendingTaskCountMax_;
  bool firstTime_;
  Monitor monitor_;
};


shared_ptr<ThreadManager> ThreadManager::newThreadManager() {
  return shared_ptr<ThreadManager>(new ThreadManager::Impl());
}

shared_ptr<ThreadManager> ThreadManager::newSimpleThreadManager(size_t count, size_t pendingTaskCountMax) {
  return shared_ptr<ThreadManager>(new SimpleThreadManager(count, pendingTaskCountMax));
}

}}} // apache::thrift::concurrency


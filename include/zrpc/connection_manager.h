// Copyright 2011 Google Inc. All Rights Reserved.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: nadavs@google.com <Nadav Samet>

#ifndef ZRPC_CONNECTION_MANAGER_H
#define ZRPC_CONNECTION_MANAGER_H

#include <pthread.h>
#include <string>
#include <vector>
#include <zmq.hpp>

#include "zrpc/macros.h"
#include "zrpc/pointer_vector.h"

namespace zmq {
class context_t;
class message_t;
}  // namespace zmq

namespace zrpc {
class Closure;
class Connection;
class ConnectionManagerController;
struct RemoteResponse;
class RpcChannel;
class StoppingCondition;

typedef PointerVector<zmq::message_t> MessageVector;


// A ConnectionManager is a multi-threaded asynchronous system for client-side
// communication over ZeroMQ sockets. Each thread in a connection manager holds
// a socket that is connected to each server we speak to.
// The purpose of the ConnectionManager is to enable all threads in a program
// to share a pool of connections in a lock-free manner.
//
// ConnectionManager cm(2);
// Connnectin* c = cm.Connect("tcp://localhost:5557");
// 
// Now, it is possible to send requests to this backend fron any thread:
// c->SendRequest(...);
//
// ConnectionManager and Connection are thread-safe.
class ConnectionManager {
 public:
  // Constructs a ConnectionManager with nthreads worker threads. 
  // By the time the constructor returns all the threads are running.
  // The actual number of threads that are started may be larger than nthreads
  // by a small constant (such as 2), for internal worker threads.
  explicit ConnectionManager(int nthreads = 1);

  virtual ~ConnectionManager();

  // Constructs an EventManager that uses the provided ZeroMQ context and
  // has nthreads worker threads. The ConnectionManager does not take ownership
  // of the given ZeroMQ context. See the above for constructor documentation
  // for details on nthreads.
  ConnectionManager(zmq::context_t* context, int nthreads = 1);

  // Returns the number of threads in this ConnectionManager.
  inline int GetThreadCount() const { return nthreads_; }

  // Connects all ConnectionManager threads to the given endpoint. On success
  // this method returns a Connection object that can be used from any thread
  // to communicate with this endpoint. Returns NULL in error.
  virtual Connection* Connect(const std::string& endpoint);

 private:
  ConnectionManagerController* GetController() const;

  void Init();

  zmq::context_t* context_;
  int nthreads_;
  std::vector<pthread_t> threads_;
  pthread_t worker_device_thread_;
  pthread_t pubsub_device_thread_;
  pthread_key_t controller_key_;
  bool owns_context_;
  friend class Connection;
  friend class ConnectionImpl;
  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

// Installs a SIGINT and SIGTERM handlers that causes all ZRPC's event loops
// to cleanly quit.
void InstallSignalHandler();

// Represents a connection to a server. Thread-safe.
class Connection {
 public:
  // Asynchronously sends a request over the connection.
  // request: a vector of messages to be sent.
  // response: points to a RemoteResponse object that will receive the response.
  //           this object must live at least until when the closure has been
  //           ran (and may be deleted by the closure).
  // deadline_ms - milliseconds before giving up on this request. -1 means
  //               forever.
  // closure - a closure that will be ran by WaitUntil() when a response
  //           arrives. The closure gets called also if the request times out.
  //           Hence, it is necessary to check response->status.
  virtual void SendRequest(
      const MessageVector& request,
      RemoteResponse* reply,
      int64 deadline_ms,
      Closure* closure) = 0;

  virtual int WaitUntil(StoppingCondition* stopping_condition) = 0;

  // Creates a thread-specific RpcChannel for this connection.
  virtual RpcChannel* MakeChannel() = 0;

 protected:
  Connection() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(Connection);
};

struct RemoteResponse {
  enum Status {
    INACTIVE = 0,
    ACTIVE = 1,
    DONE = 2,
    DEADLINE_EXCEEDED = 3,
  };
  Status status;
  MessageVector reply;
};
}  // namespace zrpc
#endif
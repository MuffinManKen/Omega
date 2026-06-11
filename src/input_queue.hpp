#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

// Thrown by InputQueue::pop() when shutdown() has been called and the queue
// is empty. Propagates up through all blocking input calls, unwinding the
// game thread's call stack so join() returns immediately.
struct ShutdownException
{};

// Thread-safe blocking queue for player input.
// The game thread calls pop() and blocks until Godot pushes a key via push().
// This is the sole inbound crossing point across the thread boundary.
class InputQueue
{
public:
  static InputQueue &instance()
  {
    static InputQueue q;
    return q;
  }

  void push(int key)
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(key);
    }
    cv_.notify_one();
  }

  // Blocks until a key is available or shutdown() is called.
  // Throws ShutdownException on shutdown with an empty queue — this propagates
  // through all nested input calls, unwinding the game thread cleanly.
  int pop()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
    if(shutdown_ && queue_.empty())
      throw ShutdownException{};
    int key = queue_.front();
    queue_.pop();
    return key;
  }

  // Unblocks any waiting pop() calls so the game thread can exit cleanly.
  void shutdown()
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
    }
    cv_.notify_all();
  }

  void reset()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_ = {};
    shutdown_ = false;
  }

private:
  InputQueue() = default;
  std::queue<int> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool shutdown_ = false;
};

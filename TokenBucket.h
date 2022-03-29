/*
Copyright (c) 2017 Erik Rigtorp <erik@rigtorp.se>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#pragma once

#include <atomic>
#include <chrono>

class TokenBucket {
public:
  TokenBucket() {}

  /* rate: average number of tokens per second */
  TokenBucket(const uint64_t rate, const uint64_t burstSize) {
    timePerToken_ = 1000000 / rate;
    timePerBurst_ = burstSize * timePerToken_;
  }

  TokenBucket(const TokenBucket &other) {
    timePerToken_ = other.timePerToken_.load();
    timePerBurst_ = other.timePerBurst_.load();
  }

  TokenBucket &operator=(const TokenBucket &other) {
    timePerToken_ = other.timePerToken_.load();
    timePerBurst_ = other.timePerBurst_.load();
    return *this;
  }

  bool consume(const uint64_t tokens) {
    // get current uptime (in microseconds)
    const uint64_t now =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    // calculate the average time needed for the requested number of tokens
    const uint64_t timeNeeded =
        tokens * timePerToken_.load(std::memory_order_relaxed);

    // minTime limits the passed time according to the burst size
    const uint64_t minTime =
        now - timePerBurst_.load(std::memory_order_relaxed);

    // get the stored time (from the last consume)
    uint64_t oldTime = time_.load(std::memory_order_relaxed);
    uint64_t newTime = oldTime;

    if (minTime > oldTime) {  // make sure not to exceed the burst size
      newTime = minTime;
    }

    for (;;) {  // loop until either successful exchange of the stored time or failure
      newTime += timeNeeded;
      if (newTime > now) {  // not enough tokens in the bucket
        return false;
      }
      // enough tokens - try to exchange the stored time
      if (time_.compare_exchange_weak(oldTime, newTime,
                                      std::memory_order_relaxed,
                                      std::memory_order_relaxed)) {
        return true;
      }
      // some other thread has received tokens, we have to retry
      newTime = oldTime;
    }

    // this should never be reached
    return false;
  }

private:
  std::atomic<uint64_t> time_ = {0};
  std::atomic<uint64_t> timePerToken_ = {0};
  std::atomic<uint64_t> timePerBurst_ = {0};
};

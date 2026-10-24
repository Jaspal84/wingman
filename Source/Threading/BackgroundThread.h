#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <utility>
namespace wingman {
class CancellationToken {
    std::shared_ptr<std::atomic<bool>> flag;
public:
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state):flag(std::move(state)){}
    bool stop_requested()const noexcept{return flag&&flag->load(std::memory_order_acquire);}
};
// C++11 thread/atomic primitives avoid std::jthread/stop_token availability gaps in
// Apple libc++ SDKs. Only control/background threads may start, stop or join it.
class BackgroundThread {
    std::shared_ptr<std::atomic<bool>> cancellation;
    std::thread thread;
public:
    BackgroundThread()=default;
    BackgroundThread(const BackgroundThread&)=delete;
    BackgroundThread& operator=(const BackgroundThread&)=delete;
    ~BackgroundThread(){request_stop();join();}
    void request_stop()noexcept{if(cancellation)cancellation->store(true,std::memory_order_release);}
    bool joinable()const noexcept{return thread.joinable();}
    void join(){if(thread.joinable())thread.join();}
    template<class Action>void start(Action&& action){
        request_stop();join();cancellation=std::make_shared<std::atomic<bool>>(false);
        thread=std::thread([state=cancellation,task=std::forward<Action>(action)]()mutable{task(CancellationToken(state));});
    }
};
}

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

namespace lsm
{

    class ThreadPool
    {
    public:
        explicit ThreadPool(size_t num_threads);
        ~ThreadPool();

        // Deleted copy/move constructors and assignment operators
        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;
        ThreadPool(ThreadPool &&) = delete;
        ThreadPool &operator=(ThreadPool &&) = delete;

        // Add a task to the thread pool
        template <class F, class... Args>
        auto enqueue(F &&f, Args &&...args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    private:
        // Worker thread function
        void worker_thread();

        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;

        // Synchronization
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop;
    };

    // Template method implementation
    template <class F, class... Args>
    auto ThreadPool::enqueue(F &&f, Args &&...args)
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {

        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Don't allow enqueueing after stopping the pool
            if (stop)
            {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([task]()
                          { (*task)(); });
        }

        condition.notify_one();
        return result;
    }

} // namespace lsm

#endif // THREAD_POOL_H
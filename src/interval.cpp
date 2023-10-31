#include <queue>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <iostream>

class Task {
public:
    std::function<void()> func;
    std::chrono::steady_clock::time_point time;

    Task(std::function<void()> f, std::chrono::steady_clock::time_point t)
        : func(f), time(t) {}

    bool operator<(const Task& other) const {
        return time > other.time;
    }
};

class EventLoop {
private:
    std::priority_queue<Task> task_queue;
    std::mutex queue_mutex;
    bool running;

public:
    EventLoop() : running(true) {}

    void add_task(const Task& task) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.push(task);
    }

    void run() {
        while (running) {
            try {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (!task_queue.empty()) {
                    auto now = std::chrono::steady_clock::now();
                    auto task = task_queue.top();
                    if (now >= task.time) {
                        task_queue.pop();
                        lock.unlock();
                        task.func();
                    } else {
                        lock.unlock();
                        std::this_thread::sleep_for(task.time - now);
                    }
                } else {
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown error" << std::endl;
            }
        }
    }

    void stop() {
        running = false;
    }
};

// ------------------------------
#include <unordered_map>

class TaskManager {
private:
    EventLoop event_loop;
    std::unordered_map<int, Task> recurring_tasks;
    int next_task_id;

public:
    TaskManager() : next_task_id(0) {}

    int set_interval(std::function<void()> func, int interval) {
        int task_id = next_task_id++;
        auto now = std::chrono::steady_clock::now();
        auto time = now + std::chrono::milliseconds(interval);
        Task task(func, time);
        recurring_tasks[task_id] = task;
        event_loop.add_task(task);
        return task_id;
    }

    void clear_interval(int task_id) {
        recurring_tasks.erase(task_id);
    }

    void set_timeout(std::function<void()> func, int timeout) {
        auto now = std::chrono::steady_clock::now();
        auto time = now + std::chrono::milliseconds(timeout);
        Task task(func, time);
        event_loop.add_task(task);
    }
};

TaskManager task_manager;

// ------------------------------
#include <Rcpp.h>

// [[Rcpp::export]]
int set_interval(SEXP func, int interval) {
    std::function<void()> f = [&]() { Rcpp::Function(func)(); };
    return task_manager.set_interval(f, interval);
}

// [[Rcpp::export]]
void clear_interval(int task_id) {
    task_manager.clear_interval(task_id);
}

// [[Rcpp::export]]
void set_timeout(SEXP func, int timeout) {
    std::function<void()> f = [&]() { Rcpp::Function(func)(); };
    task_manager.set_timeout(f, timeout);
}


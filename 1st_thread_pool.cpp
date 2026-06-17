#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <functional>
class ThreadPool {
private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::condition_variable cv,done_cv;
  bool stop = false;
  std::mutex task_mutex;

public:
  // constructor makes a thread pool object with n threads
  ThreadPool(int n) {
    for (int i = 0; i < n; i++) {
      workers.push_back(std::thread(&ThreadPool::thread_work, this, i));
    }
  }

  void done_with_tasks(){
  {
    std::unique_lock<std::mutex> mut_lock{task_mutex, std::defer_lock};
    mut_lock.lock();
    done_cv.wait(mut_lock, [this] {return tasks.empty();});
   stop= true;
  }
  cv.notify_all();
  }

  void thread_work(int x) {

    std::unique_lock<std::mutex> mut_lock{task_mutex, std::defer_lock};
    while (true) {
      mut_lock.lock();
      cv.wait(mut_lock, [this] { return !tasks.empty() || stop; });
      if (stop && tasks.empty()) {
        return;
      };
      auto task = tasks.front();
      tasks.pop();
      mut_lock.unlock();
      
      //the actual thread work that will be done by the thread
      task();
      done_cv.notify_all();
    }
  }

  void add_task(std::function<void()> task) {

    std::unique_lock<std::mutex> mut_lock{task_mutex, std::defer_lock};
    mut_lock.lock();
    tasks.push(task);
    mut_lock.unlock();
    cv.notify_one();
  }


  ~ThreadPool() {
    {
    std::unique_lock<std::mutex> mut_lock{task_mutex, std::defer_lock};
    mut_lock.lock();
    done_cv.wait(mut_lock, [this] {return tasks.empty() && stop;});
    }
    for (auto& w: workers) {
        w.join();
    }
  }
};


void print_num(int num){
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout<<num<<std::endl;
}

int main() {

  ThreadPool obj(2);
  obj.add_task([]{print_num(1);}); 
  obj.add_task([]{print_num(1);}); 
  obj.add_task([]{print_num(1);}); 

  std::cout<<"Done!"<<std::endl;
  obj.done_with_tasks();
}

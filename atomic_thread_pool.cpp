#include <atomic>
#include <iostream>
#include <vector>
#include <thread>
#include <functional>


struct WorkerQueue{
    std::atomic<int> head{0};
    std::atomic<int> tail{0};

    std::function<void()> worker_queue[1024];

    WorkerQueue(){
        head.store(0,std::memory_order_release);
        tail.store(0,std::memory_order_release);
    }

    void add_task_from_tail(std::function<void()> task){
        while ((tail.load(std::memory_order_acquire)-head.load(std::memory_order_acquire)==1024)) {}

        worker_queue[tail%1024] = task;
        tail.store(tail+1,std::memeory_order_release);
    }
                    
    std::optional<std::function<void()>> get_head_and_pop(){
        int expected = head.load(std::memory_order_acquire);
        while(!(head.compare_exchange_weak(expected,expected+1,std::memory_order_acq_rel,std::memory_order_relaxed) {

            if (head.load(std::memory_order_acquire)==tail.load(std::memory_order_acquire)){

               return std::nullopt;
            }
        }

        auto task = worker_queue[(expected)%1024];

        return task;
    }


    bool is_empty(){
        if (head.load(std::memory_order_acquire)==tail.load(std::memory_order_acquire)){
            return true;
        }

        return false;
    }
};

class threadpool{

private:
    std::vector<std::unique_ptr<WorkerQueue>> queues;
    //represents the task queue of each thread that other threads can see and steal from 
    std::vector<std::thread> workers;
    //just the workers 

    int task_index=0;
    int total_num_of_threads;


    void thread_work(int index, WorkerQueue* thread_queue){
    //index represents the index of the thread's worker queue in queues
    //WorkerQueue thread_queue = queues[index];

    if (!(thread_queue->is_empty())){//if there are tasks in the work queue

        auto task = (thread_queue)->get_head_and_pop();
        task();
    }
    }

public:
    threadpool(int n){
        queues.reserve(n);
        total_num_of_threads = n;
        for (int i=0;i<n;i++){
            std::unique_ptr<WorkerQueue> ptr = std::make_unique<WorkerQueue>();
            queues.push_back(std::move(ptr));
            //making the relevant task queue for each thread

            workers.push_back(std::thread(&threadpool::thread_work,this,i,queues[i].get()));
            //initialize workers while also passing their queue index
        }

    }

    
    void add_task(std::function<void()> task){

        //owner thread pushes with a round robin style to a thread's workerQueues end
        queues[task_index]->add_task_from_tail(task);

        //changing task_index
        if (++task_index == total_num_of_threads){
            task_index=0;
        }
    }

    void kill_tp(){
        for (int i=0;i<workers.size();i++){
            workers[i].join();
        }
    }

};



void foo(){
    int s = 4;
    for (int i=0;i<1000;i++){
        s++;
    }

    std::cout<<s<<std::endl;
}

int main(){

    threadpool obj(2);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.kill_tp();

}

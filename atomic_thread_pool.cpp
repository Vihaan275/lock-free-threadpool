#include <atomic>
#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <optional>


struct WorkerQueue{
    std::atomic<int> head{0};
    std::atomic<int> tail{0};

    std::function<void()> worker_queue[1024];

    WorkerQueue(){
        head.store(0,std::memory_order_release);
        tail.store(0,std::memory_order_release);
    }

    void add_task_from_tail(std::function<void()> task){
        while ((tail.load(std::memory_order_relaxed)-head.load(std::memory_order_acquire)==1024)) {}

        worker_queue[tail%1024] = task;
        tail.fetch_add(1,std::memory_order_release);
    }
                    
    std::optional<std::function<void()>> get_head_and_pop(){
        if (head.load(std::memory_order_acquire)==tail.load(std::memory_order_acquire)){
            return std::nullopt;

        }
        int expected = head.load(std::memory_order_acquire);
        while(!(head.compare_exchange_weak(expected,expected+1,std::memory_order_acq_rel,std::memory_order_relaxed))) {

            if (head.load(std::memory_order_acquire)==tail.load(std::memory_order_acquire)){

               return std::nullopt;
            }
        }

        auto task = std::move(worker_queue[(expected)%1024]);

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
    std::mutex mute;
    std::condition_variable cv;

    std::atomic<bool> stop{false};


    void thread_work(int index, WorkerQueue* thread_queue){
    //index represents the index of the thread's worker queue in queues
    //WorkerQueue thread_queue = queues[index];
        while(true){

        if(!(thread_queue->is_empty())){
        auto task = (thread_queue->get_head_and_pop());
        if (task){
            (*task)();
        }
        }
        else{// if we dont have tasks in our queue


            int count = (index+1)%total_num_of_threads;
            while(count%total_num_of_threads !=index){


                        WorkerQueue* new_queue = queues[count%total_num_of_threads].get();
                        while (!(new_queue->is_empty())){
                            auto task = new_queue->get_head_and_pop();
                            if (task){
                                (*task)();
                            }
                            else{
                                break;
                            }
                        }

                count++;
                }

            
            //all threads basically try to finish all other tasks till they reach back to their own thread, and then they wait on the condition variable. 

            std::unique_lock<std::mutex> mut_lock(mute);
            //CV is only basically to let a thread wait if there are no tasks
            cv.wait(mut_lock, [&] {return !thread_queue->is_empty() || stop.load(std::memory_order_acquire);});
            mut_lock.unlock();

            if (stop && thread_queue->is_empty()){
                return;
            }

            continue;
            
        }
        }
    }

    //if done with your own work queue, randomly choose another queue and try to steal its work 

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

        cv.notify_all();

        //changing task_index
        if (++task_index == total_num_of_threads){
            task_index=0;
        }
    }

    void kill_tp(){
        int count=0;
        int joined_threads=0;

        stop.store(true,std::memory_order_release);
        cv.notify_all();
        while(joined_threads<total_num_of_threads){
            if (queues[count%total_num_of_threads]->is_empty()){
                if(workers[count%total_num_of_threads].joinable()){
                workers[count%total_num_of_threads].join();
                joined_threads++;
                }
            count++;
    }
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

    threadpool obj(4);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.add_task(foo);
    obj.kill_tp();

}

#include "..\lab_sync\lab_sync.h"
#include <iostream>

#define IDLE_TIME rand() % 100 //number from 0 to 99
#define IDLE_SALT rand() % 10 //number from 0 to 9
#define THR_CNT 4

//some structures for thread control
struct thread_control
{
	bool terminate;
};

class task
{
private:
	std::string description;
public:
	task(const std::string &desc) : description(desc)
	{
	}
	virtual ~task()
	{
	}
	std::string get_description() 
	{
		return description;
	}
	virtual void operator()(thread_control &thr_c)
	{
	}
};

class terminate : public task
{
public:
	terminate() : task("terminator")
	{
	}
	void operator()(thread_control &thr_c)
	{
		thr_c.terminate = true;
	}
};

class work : public task
{
private:
	int sleep_time_ms;
public:
	work(const int sleep) : task("work work"), sleep_time_ms(sleep)
	{
	}
	void operator()(thread_control &thr)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms));
	}
};


//lock for cout not to overlap
std::mutex glob_lock;

void q_producer(lab_queue<task*> *command_in, std::vector<lab_queue<task*>> *consumers)
{
	try
	{
		task *element = nullptr;
		int curr_size;
		int rand_wait;
		int rand_salt;
		int control_wait = 0;
		thread_control my_thread;
		std::vector<lab_queue<task*>>::iterator cons_it;
		queue_status my_stat = { 0, 0, 0, 0 };
		while (!my_thread.terminate)
		{
			if (command_in->pop_front(*&element, curr_size, control_wait)) //if the control queue times out, do your production duty
			{
				rand_wait = IDLE_TIME;
				rand_salt = IDLE_SALT;
				//get the thread with lowest work load
				cons_it = std::min_element(consumers->begin(), consumers->end(), [](lab_queue<task*> *el1, lab_queue<task*> *el2)->bool
				{
					return (el1->get_status().size < el2->get_status().size);
				});
				if (cons_it->get_status().size > 50) //if the thread with leats work has over 50 tasks, just skip sending data
				{
					std::unique_lock<std::mutex> lck(glob_lock);
					std::cout << "all workers too busy, skipping data!" << std::endl;
				}
				else //workers can still work
				{
					cons_it->push_back(new work(rand_wait * THR_CNT + rand_salt), -1); //give it work that takes (thread_count) longer than the producer will wait
					//figure out how long to wait on the control queue before actually doing something, 
					//we wait a little less to fill the consumers and actually start skipping data
					control_wait = rand_wait - rand_salt; 
				}
			}
			else
			{
				element->operator()(my_thread);
				delete element;
			}
		}
	}
	catch (const std::exception &e)
	{
		std::unique_lock<std::mutex> lck(glob_lock);
		std::cout << e.what() << std::endl;
	}
}

void q_consumer( )
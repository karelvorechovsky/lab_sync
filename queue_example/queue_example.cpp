#include "..\lab_sync\lab_sync.h"
#include <iostream>
#include <conio.h>
#include <forward_list>

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

class terminate_t : public task
{
public:
	terminate_t() : task("terminator")
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

void q_producer(lab_queue<task*> *command_in, std::forward_list<lab_queue<task*>> *consumers)
{
	try
	{
		task *element = nullptr;
		int curr_size;
		int rand_wait;
		int rand_salt;
		int control_wait = 0;
		thread_control my_thread;
		queue_status my_stat = { 0, 0, 0, 0 };
		std::vector<int> lengths;
		std::vector<int>::iterator l_it;
		lengths.reserve(THR_CNT);
		while (!my_thread.terminate)
		{
			if (command_in->pop_front(*&element, curr_size, control_wait)) //if the control queue times out, do your production duty
			{
				rand_wait = IDLE_TIME;
				rand_salt = IDLE_SALT;
				//get the thread with lowest work load
				lengths.clear();
				std::for_each(consumers->begin(), consumers->end(), [&](lab_queue<task*> &curr)
				{
					lengths.push_back(curr.get_status().size);
				});
				l_it = std::min_element(lengths.begin(), lengths.end(), [](int &left, int &right)->bool
				{
					return (left < right);
				});

				if (consumers->at(*l_it).get_status().size > 50) //if the thread with leats work has over 50 tasks, just skip sending data
				{
					std::unique_lock<std::mutex> lck(glob_lock);
					std::cout << "all workers too busy, skipping data!" << std::endl;
				}
				else //workers can still work
				{
					consumers->at(*l_it).push_back(new work(rand_wait * THR_CNT + rand_salt), -1); //give it work that takes (thread_count) longer than the producer will wait
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
		//once the for loop is over, notify all consumers to terminate
		std::for_each(consumers->begin(), consumers->end(), [](lab_queue<task*> &curr)
		{
			curr.lossy_push_front(new terminate_t());
		});
	}
	catch (const std::exception &e)
	{
		std::unique_lock<std::mutex> lck(glob_lock);
		std::cout << e.what() << std::endl;
	}
}

void q_consumer(lab_queue<task*> *command_in)
{
	try
	{
		task *element = nullptr;
		int curr_size;
		thread_control my_thread;
		queue_status my_stat = { 0, 0, 0, 0 };
		while (!my_thread.terminate)
		{
			command_in->pop_front(*&element, curr_size, -1); //wait for work
			element->operator()(my_thread);
			delete element;
		}
	}
	catch (const std::exception &e)
	{
		std::unique_lock<std::mutex> lck(glob_lock);
		std::cout << e.what() << std::endl;
	}
}

int main()
{
	try
	{
		//init the command queue to producer
		lab_queue<task*> command_q(-1);
		std::forward_list<lab_queue<task*>> consumer_end;
		std::vector<std::thread> consumers;
		//init the queues to consumers
		for (int i = 0; i < THR_CNT; i++)
		{
			consumer_end.emplace_front(-1);
		}
		//start consumers
		consumers.reserve(THR_CNT);
		std::for_each(consumer_end.begin(), consumer_end.end(), [&](lab_queue<task*> &curr)
		{
			consumers.push_back(std::thread(q_consumer, &curr));
		});
		//start producer
		std::thread producer_thr(q_producer, &command_q, &consumer_end);
		
		//wait for user to hit a key
		int i = _getch();

		//after he pressed key, notify producer the program should end
		command_q.push_back(new terminate_t(), -1);
		//wait for the producer to join
		producer_thr.join();
		//and wait for the consumers to end as well
		std::for_each(consumers.begin(), consumers.end(), [](std::thread &curr)
		{
			curr.join();
		});
		return 0;
	}
	catch (const std::exception &e)
	{
		std::unique_lock<std::mutex> lck(glob_lock);
		std::cout << e.what() << std::endl;
	}
}
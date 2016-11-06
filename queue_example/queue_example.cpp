#include "..\lab_sync\lab_sync.h"
#include <iostream>
#include <conio.h>
#include <forward_list>
#include <thread>

#define PROD_TIMEOUT 50
#define IDLE_TIME rand() % 100 //number from 0 to 99
#define IDLE_SALT rand() % 10 //number from 0 to 9
#define THR_CNT 4 

//lock for cout not to overlap
std::mutex glob_lock;

int check_zero(const int &num)
{
	if (num < 0)
		return 0;
	return num;
}

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
		{
			//std::unique_lock<std::mutex> lck(glob_lock);
			//std::cout << "thread " << std::this_thread::get_id() << " will work for " << sleep_time_ms << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms));
	}
};

void q_producer(lab_queue<task*> *command_in, std::forward_list<lab_queue<task*>> *consumers)
{
	try
	{
		task *element = nullptr;
		task *cons_work = nullptr;
		int curr_size;
		int rand_wait;
		int rand_salt;
		int control_wait = 0;
		thread_control my_thread = { false };
		queue_status my_stat = { 0, 0, 0, 0 };
		std::forward_list<lab_queue<task*>>::iterator cons_it = consumers->begin();
		while (!my_thread.terminate)
		{
			if (command_in->pop_front(*&element, curr_size, control_wait)) //if the control queue times out, do your production duty
			{
				rand_wait = IDLE_TIME;
				rand_salt = IDLE_SALT;
				//do circular work dispatch
				cons_work = new work(check_zero(rand_wait * THR_CNT));
				if (cons_it->push_back(cons_work, PROD_TIMEOUT))
				{
					delete cons_work;
					control_wait = 0;
					std::unique_lock<std::mutex> lck(glob_lock);
					std::cout << std::endl << "producer timed out!" << std::endl << std::endl;
				}
				else
				{
					++cons_it;
					control_wait = check_zero(rand_wait - rand_salt);
				}
				if (cons_it == consumers->end())
					cons_it = consumers->begin();
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
		thread_control my_thread = { false };
		queue_status my_stat = { 0, 0, 0, 0 };
		const int max_counter = 10;
		int counter = 0;
		while (!my_thread.terminate)
		{
			if (!command_in->pop_front(*&element, curr_size, -1)) //wait for work
			{
				element->operator()(my_thread);
				delete element;
				if (counter == max_counter)
				{
					std::unique_lock<std::mutex> lck(glob_lock);
					//spit some stats
					std::cout 
						<< std::endl 
						<< "thread:" << std::this_thread::get_id() << " has " << curr_size - 1 << " pending." << std::endl 
						<< std::endl;
					counter = 0;
				}
				++counter;
			}
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
			consumer_end.emplace_front(10);
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
		std::for_each(consumer_end.begin(), consumer_end.end(), [](lab_queue<task*> &curr)
		{
			task *element = nullptr;
			int curr_size = 0;
			while (!curr.pop_front(element, curr_size, 0))
			{
				//delete all elements in queue
				delete element;
			}
		});
		return 0;
	}
	catch (const std::exception &e)
	{
		std::unique_lock<std::mutex> lck(glob_lock);
		std::cout << e.what() << std::endl;
	}
}
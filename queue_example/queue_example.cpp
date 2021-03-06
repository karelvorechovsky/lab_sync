#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "..\lab_sync\lab_sync.h"
#include <iostream>
#include <conio.h>
#include <forward_list>
#include <thread>

#define PROD_TIMEOUT 150
#define IDLE_TIME rand() % 100 //number from 0 to 99, the thread can now wait (0 - 99) * 4
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
		std::vector<int> sizes;
		std::vector<int>::iterator it_sizes;
		std::forward_list<lab_queue<task*>>::iterator it_consumers;
		sizes.reserve(THR_CNT);
		while (!my_thread.terminate)
		{
			if (command_in->pop_front(*&element, curr_size, control_wait)) //if the control queue times out, do your production duty
			{

				rand_wait = IDLE_TIME;
				rand_salt = IDLE_SALT;
				sizes.clear();
				for (auto& curr : *consumers)
				{
					sizes.emplace_back(curr.get_status().size);
				}
				it_sizes = std::min_element(sizes.begin(), sizes.end());
				int steps_2_walk = it_sizes - sizes.begin();

				it_consumers = std::next(consumers->begin(), steps_2_walk);
				cons_work = new work(check_zero(rand_wait * THR_CNT));
				if (it_consumers->push_back(cons_work, PROD_TIMEOUT))
				{
					std::unique_lock<std::mutex> lck(glob_lock);
					std::cout << "producer timed out!" << std::endl;
					delete cons_work;
				}

				control_wait = check_zero(rand_wait - rand_salt);
			}
			else
			{
				element->operator()(my_thread);
				delete element;
			}
		}
		//once the for loop is over, notify all consumers to terminate
		for (auto &curr : *consumers)
		{
			//curr.lossy_push_front(new terminate_t()); would terminate the consumers instantly if they had work
			curr.push_back(new terminate_t(), -1);
		}
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
					std::cout << "thread:" << std::this_thread::get_id() << " has " << curr_size << " tasks pending." << std::endl;

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
		std::cout 
			<< "hit key to stop" 
			<< std::endl
			<< "starting " 
			<< THR_CNT 
			<< " consumers"
			<< std::endl;
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
		for(auto &curr : consumer_end)
		{
			consumers.push_back(std::thread(q_consumer, &curr));
		}
		//start producer
		std::thread producer_thr(q_producer, &command_q, &consumer_end);
		
		//wait for user to hit a key
		int i = _getch();

		//after he pressed key, notify producer the program should end
		command_q.push_back(new terminate_t(), -1);
		//wait for the producer to join
		producer_thr.join();
		//and wait for the consumers to end as well
		for(auto &curr : consumers)
		{
			curr.join();
		}
		for(auto &curr : consumer_end)
		{
			task *element = nullptr;
			int curr_size = 0;
			while (!curr.pop_front(element, curr_size, 0))
			{
				//delete all elements in queue
				delete element;
			}
		}
	}
	catch (const std::exception &e)
	{
		std::unique_lock<std::mutex> lck(glob_lock);
		std::cout << e.what() << std::endl;
	}
	_CrtDumpMemoryLeaks();
	return 0;
}
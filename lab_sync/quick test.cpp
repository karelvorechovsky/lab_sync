//#include "lab_sync.h"
#include <iostream>

#define IDLE_TIME rand() % 100

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
	~task()
	{
	}
	virtual std::string get_description()
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

void q_producer(lab_queue<int> *command_in, std::vector<lab_queue<int>> *consumers)
{ 
	int element = 0;
	while (element != -1)
	{
		if (q->push_front(my_in, 50))
		{
			std::unique_lock<std::mutex> lck(glob_lock);
			std::cout << "producer timed out" << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		--my_in;
	}
	q->push_back(-1, 500);
}

void q_consumer(lab_queue<int> *q)
{
	int i = 0;
	int q_stat = 0;
	while (i != -1)
	{
		if (q->pop_front(i, q_stat, 30))
		{
			std::unique_lock<std::mutex> lck(glob_lock);
			std::cout << "consumer timed out!" << std::endl;
		}
		std::unique_lock<std::mutex> lck(glob_lock);
		std::cout << i << "queue size : " << q_stat << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

int main()
{
	try
	{
		lab_event<int> my_event;
		lab_event<int> my_event2;
		lab_register<int> my_register;

		my_register.register_event(&my_event);
		my_register.register_event(&my_event2);

		my_event.generate_event(5);

		int test = 0;
		bool result = my_register.wait(test, 0);
		my_register.unregister_event(&my_event);
		//my_register.unregister_event(&my_event2);

		my_event2.generate_event(25);
		result = my_register.wait(test, 0);

 		lab_queue<int> my_q(1);

		std::thread con(q_consumer, &my_q);
		std::thread pro(q_producer, &my_q, 20);

		con.join();
		pro.join();

		std::deque<int> my_mq;

		my_mq = my_q.flush();

		my_q.push_back(25, 0);
	}
	catch (std::exception &e)
	{
		std::cout << e.what();
	}
}
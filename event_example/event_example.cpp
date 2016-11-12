#include "..\lab_sync\lab_sync.h"
#include <iostream>
#include <sstream>
#include <conio.h>
#include <forward_list>
#include <thread>

#define THR_CNT 4
#define EVENT_CNT 4
std::mutex glob_lock;


class my_event
{
public:
	virtual bool operator()()
	{
		return false;
	}
	virtual ~my_event()
	{
	}
};

class terminate_event : public my_event
{
public:
	bool operator()()
	{
		return true;
	}
};

class data_event : public my_event
{
private:
	std::vector<double> data;
public:
	data_event(std::vector<double> data) : data(data)
	{
	}
};



int main()
{
	//sync pbjects
	std::vector<std::shared_ptr<lab_event<my_event>>> signal_events;
	std::shared_ptr<lab_event<my_event>> control_event;
	//sync objects initiation 
	control_event = std::make_shared<lab_event<my_event>>("control_event");
	
	std::ostringstream s;
	for (int i = 0; i < EVENT_CNT; ++i)
	{
		s.str("");
		s.clear();
		s << i;
		signal_events.push_back(std::make_shared<lab_event<my_event>>(s.str()));
	}

	return 0;
}


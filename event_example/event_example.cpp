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
	std::vector<double> get_data()
	{
		return data;
	}
};

struct user_input
{
	enum command
	{
		toggle_reg,
		fire_event,
		exit
	} user_command;
	int toggle_thread;
	int toggle_event;
	int event_fired;
};

std::vector<std::string> split(const std::string& str, const std::string& delim)
{
	std::vector<std::string> tokens;
	size_t prev = 0, pos = 0;
	do
	{
		pos = str.find(delim, prev);
		if (pos == std::string::npos) pos = str.length();
		std::string token = str.substr(prev, pos - prev);
		if (!token.empty()) tokens.push_back(token);
		prev = pos + delim.length();
	} while (pos < str.length() && prev < str.length());
	return tokens;
}

user_input parse_command(std::string &command)
{
	user_input input = user_input();

	if (command == "exit")
	{
		input.user_command = user_input::command::exit;
		return input;
	}
	size_t pos = 0;
	std::vector<std::string> tokens = split(command, " ");
	switch (tokens[0][0])
	{
	case 'f':
		input.user_command = user_input::command::fire_event;
		input.event_fired = std::stoi(tokens[1]);
		break;
	case 't':
		input.user_command = user_input::command::toggle_reg;
		input.toggle_event = std::stoi(tokens[1]);
		input.toggle_thread = std::stoi(tokens[2]);
		break;
	}
	return input;
}

int main()
{
	//sync objects
	std::vector<std::shared_ptr<lab_event<std::shared_ptr<my_event>>>> signal_events;
	std::vector<std::shared_ptr<lab_register<std::shared_ptr<my_event>>>> thread_registers;
	std::shared_ptr<lab_event<std::shared_ptr<my_event>>> control_event;
	//sync objects initiation 
	control_event = std::make_shared<lab_event<std::shared_ptr<my_event>>>("control_event");
	std::ostringstream s;
	//create signal events
	for (int i = 0; i < EVENT_CNT; ++i)
	{
		s.str("");
		s.clear();
		s << i;
		signal_events.push_back(std::make_shared<lab_event<std::shared_ptr<my_event>>>(s.str()));
	}
	//for all threads register all events + control event
	for (int i = 0; i < THR_CNT; ++i)
	{
		std::shared_ptr<lab_register<std::shared_ptr<my_event>>> curr_register = std::make_shared<lab_register<std::shared_ptr<my_event>>>();
		//register control event
		curr_register->register_event(control_event.get());
		std::for_each(signal_events.begin(), signal_events.end(), [&](const std::shared_ptr<lab_event<std::shared_ptr<my_event>>> &curr_event)
		{
			//and register all events for all threads
			curr_register->register_event(curr_event.get());
		});
		thread_registers.push_back(curr_register);
	}
	//at this point all sync objects are done and all registers are registered to all events
	//firing any event will notify all threads
	std::string command;
	user_input input = user_input();
	std::cout
		<< "program does no parameter check" << std::endl
		<< "type \"f 2\" to fire event 2" << std::endl
		<< "type \"t 2 3\" to toggle event 2 registration to thread 3" << std::endl
		<< "type \"exit\" to quit" << std::endl << std::endl;
	while (input.user_command != user_input::command::exit)
	{
		std::getline(std::cin, command);
		input = parse_command(command);
		switch (input.user_command)
		{
		case user_input::command::fire_event:
			signal_events[input.event_fired].get()->generate_event(std::make_shared<data_event>(data_event({ 1, 2, 3 })));
			break;
		case user_input::command::toggle_reg:
			break;
		default:
			break;
		}
	}
	return 0;
}


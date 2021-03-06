#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <algorithm>
#include <set>

#ifndef _LAB_SYNC_
#define _LAB_SYNC_

//**********************************************************QUEUES****************************************************************// 


struct queue_status
{
	int size;
	int p_read; 
	int p_write; 
	int max_size;
};

template<typename T>
class lab_queue
{
private:
	//queue data
	std::deque<T> _data;
	//data mutex to protect the conditional_variable and the data itself
	std::mutex _data_lock;
	//conditional variable used to halt threads if no data or too much data
	std::condition_variable _wait;
	//queue maximum size, if the queue is full, the writers have to wait
	int _max_size;
	//number of readers waiting to pop element from queue (they are blocked because the queue is empty)
	int _p_read;
	//number of writers waiting to insert into queue (they are blocked because the queue has reached maximum size)
	int _p_write;
	//helper class to make _p_read and _p_write variables modification exception safe
	/// Private copy constructor.
	lab_queue(const lab_queue&);
	/// Private assignment operator.
	lab_queue& operator=(const lab_queue&);
	//exception safe status incrementing
	class _p_inc
	{
	private:
		int &val;
	public:
		_p_inc(int& in) : val(in) { ++val; }
		~_p_inc() { --val; }
	};
public:
	//max_size <= 0 means unlimited
	lab_queue(int max_size = -1) : _max_size(max_size)
	{
	}
	~lab_queue()
	{
	}
	//lab_queue(const lab_queue& other) = delete;
	//get the queue status to see how many elements are in and how many readers & writers are pending
	queue_status get_status()
	{
		std::unique_lock<std::mutex> data_lock(_data_lock);//lock data
		queue_status stat;
		stat.size = _data.size();
		stat.p_read = _p_read;
		stat.p_write = _p_write;
		stat.max_size = _max_size;
		return stat;
	}
	//push element into the very front of queue, so it gets dequeued first unless another element is pushed front
	//return true on timeout, false otherwise
	//params
	//element, the data to push
	//timeout, timeout in milliseconds, timeout < 0 means wait indefinitely
	bool push_front(const T& element, const int timeout) //returns true if timed out
	{
		std::unique_lock<std::mutex> data_lock(_data_lock); //lock data
		_p_inc wrt_inc(_p_write); //increment the pending pointer
		if (_max_size > 0)
		{
			while (_data.size() == _max_size)
			{
				if (timeout < 0)
				{
					_wait.wait(data_lock);
				}
				else
				{
					if (std::_Cv_status::timeout == _wait.wait_for(data_lock, std::chrono::milliseconds(timeout)))
						return true; //the push operation has timed out

				}
			}
		}
		//data is locked at this point
		_data.push_front(element); //push new data
		if (_data.size() == 1) //if data was empty
		{
			_wait.notify_one(); //notify 
		}
		return false;
	}
	//push element into the very back of queue
	//return true on timeout, false otherwise
	//params
	//element, the data to push
	//timeout, timeout in milliseconds, timeout < 0 means wait indefinitely
	bool push_back(const T& element, const int timeout) //returns true if timed out
	{
		std::unique_lock<std::mutex> data_lock(_data_lock); //lock data

		_p_inc wrt_inc(_p_write); //increment the pending variable

		while (_data.size() == _max_size)
		{
			if (timeout < 0)
			{
				_wait.wait(data_lock);
			}
			else
			{
				if (std::_Cv_status::timeout == _wait.wait_for(data_lock, std::chrono::milliseconds(timeout)))
					return true;
			}
		}
		_data.push_back(element); //push new data
		if (_data.size() == 1) //if data was empty
		{
			_wait.notify_one(); //notify 
		}
		return false;
	}
	//push element into the very back of queue, if the queue is full, remove element from the back of the queue
	//returns nothing
	//params
	//element, the data to push
	void lossy_push_back(const T& element)
	{
		std::unique_lock<std::mutex> data_lock(_data_lock); //lock data

		_p_inc wrt_inc(_p_write); //increment the pending variable

		if (_data.size() == _max_size)
		{
			//in case the queue is full, discard the oldest
			_data.pop_back();
		}
		_data.push_back(element); //push new data
		if (_data.size() == 1) //if data was empty
		{
			_wait.notify_one(); //notify 
		}
		return false;
	}
	//push element into the very front of queue, if the queue is full, remove element from the back of the queue
	//returns nothing
	//params
	//element, the data to push
	void lossy_push_front(const T& element)
	{
		std::unique_lock<std::mutex> data_lock(_data_lock); //lock data

		_p_inc wrt_inc(_p_write); //increment the pending variable

		if (_data.size() == _max_size)
		{
			//in case the queue is full, discard the oldest
			_data.pop_back();
		}
		_data.push_front(element); //push new data
		if (_data.size() == 1) //if data was empty
		{
			_wait.notify_one(); //notify 
		}
	}
	//clears the queue data
	//returns data back through std::move
	std::deque<T> &&flush()
	{
		std::unique_lock<std::mutex> lck(_data_lock);
		return std::move(_data);
	}
	//dequeue element from the front of the queue
	//return true on timeout, false otherwise
	//params
	//element, the data to push
	//current queue size without the dequeued element
	//timeout, timeout in milliseconds, timeout < 0 means wait indefinitely
	bool pop_front(T& element, int &q_size, const int timeout) //returns true if timed out
	{
		std::unique_lock<std::mutex> data_lock(_data_lock); //lock data

		_p_inc rd_inc(_p_read); //increment the pending pointer

		while (!_data.size())
		{
			if (timeout < 0)
			{
				_wait.wait(data_lock);
			}
			else
			{
				if (std::_Cv_status::timeout == _wait.wait_for(data_lock, std::chrono::milliseconds(timeout))) //wait till data comes in
					return true;
			}
		}
		element = _data.front(); //get data
		_data.pop_front(); //pop the queue
		q_size = _data.size();
		if (_data.size() == _max_size - 1) //if queue was full, report it is fedable again
		{
			_wait.notify_one(); //notify the writer
		}
		return false;
	}
};

//**********************************************************EVENTS****************************************************************//
//forward declaration of register
template<typename T>
class lab_register;

//the event object, 
//for every lab_register it has been registered to, it maintains the register queue to notify registers and send them data
template<typename T>
class lab_event
{
private:
	std::string name;
	//the bunch of registers the event is registered to
	std::deque<lab_register<T>*> registers;
	//register access guard
	std::mutex registers_guard;
public:
	lab_event(const std::string &name = "") : name(name)
	{
	}
	~lab_event()
	{
		std::for_each(registers.begin(), registers.end(), [&](lab_register<T> *&curr_register)
		{
			curr_register->remove_event(this);
		});
	}
	//add new registration queue to the event
	void add_register(lab_register<T>* new_r_q)
	{
		std::unique_lock<std::mutex> lck(registers_guard);
		registers.push_back(new_r_q);
	}
	//remove register from the event
	bool remove_register(lab_register<T>* removed) //returns true if the event had the searched register
	{
		std::unique_lock<std::mutex> lck(registers_guard);
		bool found = false;
		typename std::deque<lab_register<T>*>::iterator it = registers.begin();
		do 
		{
			if ((*it) == removed)
			{
				it = registers.erase(it);
				found = true;
				break;
			}
			it++;
		} while (it != registers.end());
		return found;
	}
	//fire user event, in case there are registers waiting, they become notified the event has been fired
	void generate_event(const T& element)
	{
		std::unique_lock<std::mutex> lck(registers_guard);
		std::for_each(registers.begin(), registers.end(), [&](lab_register<T> *&curr_register)
		{
			std::unique_lock<std::mutex> lock(curr_register->r_queue.mtx);
			curr_register->r_queue.data.push_back(element);
			if (curr_register->r_queue.data.size() == 1) //if the queue was empty
				curr_register->r_queue.cnd.notify_one(); //notify the waiting register, every register has it's own cnd, no cnd's are shared among multiple waiters
		});
	}
};

//the register object, 
//it has it's own registration_queue, that every registered event gets pointer to, so it can notify the waiting register whenever new data is available
//registration_queue is shared among all events that are registered to such register
//for every lab_register exists one event queue, where events are enqueued
//the queues are conceptually non-blocking for writers, 
//so they cannot have maximum size that would block the generating event from returning
template<typename T>
struct registration_queue
{
	std::mutex mtx;
	std::condition_variable cnd;
	std::deque<T> data;
};
template<typename T>
class lab_register
{
private:
	//this is necessary for lab_register destructor so it removes queue to this register from all registered events
	//also allows events unregistering programatically
	struct event_ptr_comparator
	{
		bool operator()(lab_event<T> *lhs, lab_event<T> *rhs)
		{
			return lhs < rhs;
		}
	};
	std::set<lab_event<T>*, event_ptr_comparator> registered_events;
	//controls access to registered_events
	std::mutex events_guard;
public:
	//the register's queue
	registration_queue<T> r_queue;

	lab_register()
	{
	}
	~lab_register()
	{
		//upon destruction, remove all registered events
		//this removes the data queue associated with this register
		std::unique_lock<std::mutex> lck(events_guard);
		std::for_each(registered_events.begin(), registered_events.end(), [&](lab_event<T>* curr_event)
		{
			curr_event->remove_register(this);
		});
	}
	//register event, assigns the internal registration_queue to the registered event
	//returns nothing
	//params
	//new_event, event to register
	void register_event(lab_event<T>* new_event)
	{
		std::unique_lock<std::mutex> lck(events_guard);
		new_event->add_register(this);
		registered_events.insert(new_event);
	}
	//wait for event
	//returns true on timeout, false otherwise
	//params
	//element, the dequed data ref
	//timeout, time to wait in milliseconds, timeout < 0 means wait indefinitely
	bool wait(T& element, const int timeout)
	{
		std::unique_lock<std::mutex> data_lock(r_queue.mtx);
		while (!r_queue.data.size())
		{
			if (timeout < 0)
			{
				//if the timeout is less than zero, wait indefinitely
				r_queue.cnd.wait(data_lock);
			}
			else
			{
				//if the cnd times out, return
				if (std::_Cv_status::timeout == r_queue.cnd.wait_for(data_lock, std::chrono::milliseconds(timeout))) //wait till data comes in
					return true;
			}
		}
		element = r_queue.data.front(); //get data
		r_queue.data.pop_front(); //pop the queue
		return false;
	}
	//unregister from the given event
	//returns nothing
	//new_event the event to unregister
	void unregister_event(lab_event<T>* new_event)
	{
		typename std::set<lab_event<T>*, event_ptr_comparator>::iterator it;
		std::unique_lock<std::mutex> lck(events_guard);
		for (it = registered_events.begin(); it != registered_events.end();)
		{
			if (*it == new_event)
			{
				(*it)->remove_register(this);
				it = registered_events.erase(it);
				break;
			}
			else
				++it;
		}
	}
	void remove_event(lab_event<T>* new_event)
	{
		std::unique_lock<std::mutex> lck(events_guard);
		typename std::set<lab_event<T>*, event_ptr_comparator>::iterator it;
		it = registered_events.find(new_event);
		registered_events.erase(it);
	}
};

#endif //_LAB_SYNC_

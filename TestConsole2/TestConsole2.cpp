// TestConsole2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
//#include<iostream>
//#include <zmq.h>

#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <iostream>
#include <chrono>

int main()
{
	using namespace boost::interprocess;

	using ShmemAllocator = allocator<interprocess_semaphore, managed_shared_memory::segment_manager>;

	managed_shared_memory segment(open_only, "LockChecks");
	interprocess_semaphore *sem = segment.find<interprocess_semaphore>("SEM").first;
	sem->wait();
	auto t2 = std::chrono::high_resolution_clock::now();
	std::cout << t2.time_since_epoch().count() << std::endl;
    return 0;
}


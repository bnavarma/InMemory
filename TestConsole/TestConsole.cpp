// TestConsole.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <iostream>
#include <chrono>
#include <string>

#include "btreetest.h"
#include "signal_handler.h"

constexpr int child_proc_num = 100;

int main(int argc, char *argv[])
{
	namespace fs = boost::filesystem;
	fs::path full_path(fs::initial_path<fs::path>());
	full_path = fs::system_complete(fs::path(argv[0]));
	std::string path_string = full_path.generic_string();

	using namespace boost::interprocess;
	using lockfree_managed_shared_memory = basic_managed_shared_memory<char, rbtree_best_fit<null_mutex_family>, iset_index>;
	using sem_sig_handler = signal_handler<interprocess_semaphore, lockfree_managed_shared_memory, allocator, lockfree_managed_shared_memory::segment_manager>;
	using void_allocator = allocator<void, lockfree_managed_shared_memory::segment_manager>;

	
	if (argc < 2) {
		struct shm_remove {
			shm_remove() { shared_memory_object::remove("SignalHandler"); }
			~shm_remove() { shared_memory_object::remove("SignalHandler"); }
		} handler_remover;

		lockfree_managed_shared_memory segment(create_only, "SignalHandler", 1000000);
		void_allocator void_alloc(segment.get_segment_manager());
		sem_sig_handler *sig_handler = segment.construct<sem_sig_handler>(unique_instance)(segment);

		btreetest_main_proc<interprocess_semaphore, lockfree_managed_shared_memory, allocator, lockfree_managed_shared_memory::segment_manager>(child_proc_num, sig_handler, path_string);
		auto temp = GetCurrentProcessId();
	}
	else {
		lockfree_managed_shared_memory segment(open_only, "SignalHandler");
		sem_sig_handler *sig_handler = segment.find<sem_sig_handler>(unique_instance).first;
		btreetest_child_proc(std::stoi(argv[1]),sig_handler);
		std::vector<int> asdf;
	}

	return 0;
}


#pragma once

#include <Windows.h>

#include "shared_btree.h"
#include "signal_handler.h"
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <algorithm>
#include <random>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

struct Data {
	int x;
	int y;
	int z;
	int s;
};
//using lockfree_managed_shared_memory = basic_managed_shared_memory<char, simple_seq_fit<null_mutex_family>, iset_index>;
//signal_handler<interprocess_semaphore, lockfree_managed_shared_memory, allocator, lockfree_managed_shared_memory::segment_manager>* g_sig_hand;

template<typename sem_class, typename shm_class, template <class, class...> class allocator, class seg_manager, class... Args>
void btreetest_main_proc(int child_num, signal_handler<sem_class, shm_class, allocator, seg_manager, Args...>* sig_handler, std::string exe_path) {
	using namespace boost::interprocess;
	using lockfree_managed_shared_memory = basic_managed_shared_memory<char, rbtree_best_fit<null_mutex_family>, iset_index>;
	using shm_void_allocator = allocator<void, lockfree_managed_shared_memory::segment_manager>;
	using btree_type = shared_btree<int, Data, shm_class, allocator, seg_manager>;

	struct shm_remove {
		shm_remove() { shared_memory_object::remove("BTreeTest"); }
		~shm_remove() { shared_memory_object::remove("BTreeTest"); }
	} remover;

	auto proc_id = GetCurrentProcessId();

	sig_handler->create_subscriber(proc_id);
	sig_handler->register_signal("WaitToEnd");
	sig_handler->register_subscriber("WaitToEnd", proc_id);
	auto proc_sem = sig_handler->get_subscriber_semaphore(proc_id);

	shm_class segment(create_only, "BTreeTest", 2684354560);
	shm_void_allocator void_alloc(segment.get_segment_manager());
	btree_type* btree_ptr = segment.construct<btree_type>(unique_instance)(&segment, void_alloc);
	std::cout << segment.get_free_memory() << std::endl;
	/*for (auto i = 1; i < 1000001; i++) {
		Data temp_dat;
		temp_dat.x = i;
		temp_dat.y = 0;
		temp_dat.z = 0;
		temp_dat.s = 0;
		btree_ptr->insert(i, temp_dat);
	}*/
	//std::cout << segment.get_free_memory() << std::endl;
	//btree_ptr->traverse();
	//std::cout << &btree_type::LeafNode::~LeafNode << std::endl;
	//auto temp = btree_ptr->find(10);
	//for (auto i = 2; i < 999990; i++) {
	//	btree_ptr->delete_data(i);
	//	//btree_ptr->traverse();
	//}
	//std::cout << segment.get_free_memory() << std::endl;
	//auto x = 0;
	//for (auto i = 1; i < 1000001; i++) {
	//	Data temp_dat;
	//	temp_dat.x = i;
	//	temp_dat.y = 0;
	//	temp_dat.z = 0;
	//	temp_dat.s = 0;
	//	btree_ptr->insert(i, temp_dat);
	//	//btree_ptr->traverse();
	//}
	std::cout << segment.get_free_memory() << std::endl;
	//btree_ptr->traverse();
	std::cout << btree_ptr->root->isLeaf << std::endl;
	auto loop_start = (0 * 10000) + 1;
	for (auto i = loop_start; i < loop_start + 10000000; i++) {
		Data temp_dat;
		temp_dat.x = i;
		temp_dat.y = 0;
		temp_dat.z = 0;
		temp_dat.s = 0;
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			btree_ptr->insert(i, temp_dat);
		}
	}
	auto success = true;
	for (auto i = loop_start; i < loop_start + 10000000; i++) {
		std::unique_ptr<Data> temp_dat;
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			auto temp = btree_ptr->find(i);
			temp_dat = std::move(temp);
		}
		if (temp_dat->x != i) {
			success = false;
			std::cout << "Failed in child_proc " << child_num << "\t" << temp_dat->x << "\t" << i << std::endl;
		}
	}
	if (success) {
		std::cout << "Avinash is K" << std::endl;
	}
	for (auto i = loop_start; i < loop_start + 10000000; i++) {
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			btree_ptr->delete_data(i);
		}
	}
	for (auto i = loop_start; i < loop_start + 10000000; i++) {
		Data temp_dat;
		temp_dat.x = i;
		temp_dat.y = 0;
		temp_dat.z = 0;
		temp_dat.s = 0;
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			btree_ptr->insert(i, temp_dat);
		}
	}
	/*std::vector<int> rand_idxs;
	for (auto i = 1; i < 90; i++) {
		rand_idxs.push_back(i);
	}
	auto engine = std::default_random_engine{};
	std::shuffle(rand_idxs.begin(), rand_idxs.end(), engine);
	for (auto i : rand_idxs) {
		btree_ptr->delete_data(i);
		btree_ptr->traverse();
	}*/
	//}

	/*struct TempData{
		int c;
		signal_handler<sem_class, shm_class, allocator, seg_manager, Args...>& sig_handler;
	};*/
	std::vector<std::thread> pool;
	//std::vector<DWORD> idarray;
	//g_sig_hand = sig_handler;
	for (auto i = 0; i < child_proc_num; i++) {
		std::replace(exe_path.begin(), exe_path.end(), '/', '\\');
		std::string sys_string = "\"" + exe_path + "\" " + std::to_string(i)/*+"\""*/;
		//std::system(sys_string.c_str());

		//pool.push_back(std::thread(btreetest_child_proc_temp,i));

		/*auto tempData = (TempData*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TempData));
		if (tempData == NULL) {
			std::cout << "Fuck this shit" << std::endl;
			continue;
		}
		DWORD tempID;
		auto tempHandle = CreateThread(NULL, 0, btreetest_child_proc, tempData, 0, &tempID);
		if (tempHandle == NULL) {
			std::cout << "Fuck again" << std::endl;
			continue;
		}*/
	}

	for (auto i = 0; i < child_proc_num; i++) {
		//proc_sem->wait();
		std::cout << "Man something is happening" << "\t" << i << std::endl;
	}

	/*for (auto i = 2; i < 1000001; i++) {
		btree_ptr->delete_data(i);
	}*/

	std::cout << "Finally made it" << std::endl;
	//btree_ptr->traverse();
	//for (auto i = 0; i < child_proc_num; i++) {
	//	pool[i].join();
	//}
	//WaitForMultipleObjects(pool.size(), &pool[0], TRUE, INFINITE);
	sig_handler->unregister_subscriber("WaitToEnd", proc_id);
	sig_handler->unregister_signal("WaitToEnd");
	sig_handler->destroy_subscriber(proc_id);
};

template<typename sem_class, typename shm_class, template <class, class...> class allocator, class seg_manager, class... Args>
void btreetest_child_proc(int child_num, signal_handler<sem_class, shm_class, allocator, seg_manager, Args...>* sig_handler) {
	using namespace boost::interprocess;
	using lockfree_managed_shared_memory = basic_managed_shared_memory<char, rbtree_best_fit<null_mutex_family>, iset_index>;
	using shm_void_allocator = allocator<void, lockfree_managed_shared_memory::segment_manager>;
	using btree_type = shared_btree<int, Data, shm_class, allocator, seg_manager>;

	auto proc_id = GetCurrentProcessId();

	lockfree_managed_shared_memory segment(open_only, "BTreeTest");
	btree_type* btree_ptr = segment.find<btree_type>(unique_instance).first;

	std::ofstream out(std::to_string(proc_id) + ".txt");
	out << proc_id << ": Houston we are go for launch" << std::endl;
	out.flush();

	auto loop_start = (child_num * 10000) + 1;
	for (auto i = loop_start; i < loop_start + 10000; i++) {
		Data temp_dat;
		temp_dat.x = i;
		temp_dat.y = 0;
		temp_dat.z = 0;
		temp_dat.s = 0;
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			btree_ptr->insert(i, temp_dat);
		}
	}
	auto success = true;
	for (auto i = loop_start; i < loop_start + 10000; i++) {
		std::unique_ptr<Data> temp_dat;
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			auto temp = btree_ptr->find(i);
			temp_dat = std::move(temp);
		}
		if (temp_dat->x != i) {
			success = false;
			out << "Failed in child_proc " << child_num << "\t" << temp_dat->x << "\t" << i << std::endl;
		}
	}
	if (success) {
		out << "Avinash is K" << std::endl;
	}

	for (auto i = loop_start; i < loop_start + 9990; i++) {
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			btree_ptr->delete_data(i);
		}
	}
	sig_handler->post_signal("WaitToEnd");
	out << "Complete" << std::endl;
	out.close();
}

void btreetest_child_proc_temp(int child_num) {
	using namespace boost::interprocess;
	using lockfree_managed_shared_memory = basic_managed_shared_memory<char, rbtree_best_fit<null_mutex_family>, iset_index>;
	using shm_void_allocator = allocator<void, lockfree_managed_shared_memory::segment_manager>;
	using btree_type = shared_btree<int, Data, lockfree_managed_shared_memory, allocator, lockfree_managed_shared_memory::segment_manager>;

	auto proc_id = GetCurrentProcessId();

	lockfree_managed_shared_memory segment(open_only, "BTreeTest");
	btree_type* btree_ptr = segment.find<btree_type>(unique_instance).first;

	std::ofstream out(std::to_string(proc_id) + ".txt");
	out << proc_id << ": Houston we are go for launch" << std::endl;
	out.flush();

	auto loop_start = (child_num * 10000) + 1;
	for (auto i = loop_start; i < loop_start + 10000; i++) {
		Data temp_dat;
		temp_dat.x = i;
		temp_dat.y = 0;
		temp_dat.z = 0;
		temp_dat.s = 0;
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			btree_ptr->insert(i, temp_dat);
		}
	}
	auto success = true;
	for (auto i = loop_start; i < loop_start + 10000; i++) {
		std::unique_ptr<Data> temp_dat;
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			auto temp = btree_ptr->find(i);
			temp_dat = std::move(temp);
		}
		if (temp_dat->x != i) {
			success = false;
			std::cout << "Failed in child_proc " << child_num << "\t" << temp_dat->x << "\t" << i << std::endl;
		}
	}
	if (success) {
		//std::cout << "Avinash is K " << std::this_thread::get_id() << std::endl;
	}

	for (auto i = loop_start; i < loop_start + 9990; i++) {
		{
			scoped_lock<interprocess_mutex> lock(btree_ptr->btree_mutex);
			btree_ptr->delete_data(i);
		}
	}
	//g_sig_hand->post_signal("WaitToEnd");
	out << "Complete" << std::endl;
	out.close();
}
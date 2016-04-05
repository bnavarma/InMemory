#include "stdafx.h"
#include "shared_btree.h"
#include "signal_handler.h"
#include <iostream>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>

//struct Data {
//	int x;
//	int y;
//	int z;
//	int s;
//};
 
//template<typename sem_class,typename shm_class,template <class, class...> class allocator, class seg_manager, class... Args>
//void btreetest_main_proc(int child_num, signal_handler<sem_class, shm_class, allocator, seg_manager, Args...>& sig_handler) {
//	using namespace boost::interprocess;
//	using lockfree_managed_shared_memory = basic_managed_shared_memory<char, simple_seq_fit<null_mutex_family>, iset_index>;
//	using shm_void_allocator = allocator<void, lockfree_managed_shared_memory::segment_manager>;
//	using btree_type = shared_btree<int, Data, shm_class, seg_manager>;
//
//	struct shm_remove {
//		shm_remove() { shared_memory_object::remove("BTreeTest"); }
//		~shm_remove() { shared_memory_object::remove("BTreeTest"); }
//	} remover;
//	
//	shm_class segment(create_only, "BTreeTest", 268435456);
//	shm_void_allocator void_alloc(segment.get_segment_manager());
//	btree_type* btree_ptr = segment.construct<btree_type>(unique_instance)(&segment, void_alloc);
//	std::cout << segment.get_free_memory() << std::endl;
//	for (auto i = 1; i < 1000001; i++) {
//		Data temp_dat;
//		temp_dat.x = i;
//		temp_dat.y = 0;
//		temp_dat.z = 0;
//		temp_dat.s = 0;
//		btree_ptr->insert(i, temp_dat);
//	}
//	std::cout << segment.get_free_memory() << std::endl;
//	//btree_ptr->traverse();
//	//std::cout << &btree_type::LeafNode::~LeafNode << std::endl;
//	auto temp = btree_ptr->find(10);
//	for (auto i = 2; i < 1000001; i++) {
//		btree_ptr->delete_data(i);
//		//btree_ptr->traverse();
//	}
//	std::cout << segment.get_free_memory() << std::endl;
//	auto x = 0;
//	for (auto i = 1; i < 1000001; i++) {
//		Data temp_dat;
//		temp_dat.x = i;
//		temp_dat.y = 0;
//		temp_dat.z = 0;
//		temp_dat.s = 0;
//		btree_ptr->insert(i, temp_dat);
//		//btree_ptr->traverse();
//	}
//	std::cout << segment.get_free_memory() << std::endl;
//	//btree_ptr->traverse();
//	std::cout << btree_ptr->root->isLeaf << std::endl;
//}

//void btreetest_child_proc(int procnum) {
//	using namespace boost::interprocess;
//	using lockfree_managed_shared_memory = basic_managed_shared_memory<char, rbtree_best_fit<null_mutex_family>, iset_index>;
//	using shm_void_allocator = allocator<void, lockfree_managed_shared_memory::segment_manager>;
//	using btree_type = shared_btree<int, Data, lockfree_managed_shared_memory, allocator, lockfree_managed_shared_memory::segment_manager>;
//
//	lockfree_managed_shared_memory segment(open_only, "BTreeTest");
//	btree_type* btree_ptr = segment.find<btree_type>(unique_instance).first;
//}
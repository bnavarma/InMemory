#pragma once

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>
#include <boost/move/unique_ptr.hpp>
#include <type_traits>
#include <iostream>
#include <boost/type_index.hpp>
#include <atomic>
#include <boost/lockfree/queue.hpp>

using namespace boost::interprocess;
template <typename sem_class>
struct subscriber {
	unsigned long pid;
	sem_class sem;

	subscriber(unsigned long pd) :pid(pd), sem(0) {}
};

template <typename sema_class, typename void_allocator, typename sub_ptr_alloc>
struct signal {
	vector<offset_ptr<subscriber<sema_class>>, sub_ptr_alloc> subscribers;

	signal(const void_allocator &void_alloc) :subscribers(void_alloc) {}
};

template<typename sem_class, typename shm_class, template<class, class...> class allocator, class seg_manager, class... Args>
class signal_handler {
	using lockfree_managed_shared_memory = basic_managed_shared_memory<char, rbtree_best_fit<null_mutex_family>, iset_index>;
	using void_allocator = allocator<void, seg_manager, Args...>;
	using ulong_allocator = allocator<unsigned long, seg_manager, Args...>;
	using char_allocator = allocator<char, seg_manager, Args...>;
	using char_string = basic_string<char, std::char_traits<char>, char_allocator>;

	using subscriber_allocator = allocator<subscriber<sem_class>, seg_manager, Args...>;
	using subscriber_ptr_allocator = allocator<subscriber<sem_class>*, seg_manager, Args...>;
	using subscriber_offset_ptr_allocator = allocator<offset_ptr<subscriber<sem_class>>, seg_manager, Args...>;
	using signal_allocator = allocator<signal<sem_class, void_allocator, subscriber_offset_ptr_allocator>, seg_manager, Args...>;
	using map_value_type = std::pair<const char_string, signal<sem_class, void_allocator, subscriber_offset_ptr_allocator>>;
	using movable_map_value_type = std::pair<char_string, signal<sem_class, void_allocator, subscriber_offset_ptr_allocator>>;
	using map_value_type_allocator = allocator<map_value_type, seg_manager, Args...>;

	using subscriber_unique_ptr = boost::movelib::unique_ptr<subscriber<sem_class>, boost::interprocess::deleter<subscriber<sem_class>, seg_manager>>;
	using subscriber_unique_ptr_allocator = allocator<subscriber_unique_ptr, seg_manager, Args...>;

	typedef sem_class semaphore_class;
	typedef allocator semaphore_allocator;
	typedef shm_class shared_memory_class;

private:
	shm_class& segment;
	offset_ptr<void_allocator> void_alloc;
	interprocess_mutex handler_mutex;

	offset_ptr<vector<offset_ptr<subscriber<sem_class>>, subscriber_offset_ptr_allocator>> subscribers;
	offset_ptr<map<char_string, signal<sem_class, void_allocator, subscriber_offset_ptr_allocator>, std::less<char_string>, map_value_type_allocator>> signals_map;

	struct subscriber_comp {
		explicit subscriber_comp(offset_ptr<subscriber<sem_class>> temp_ptr) :r_ptr(temp_ptr) {}
		inline bool operator()(offset_ptr<subscriber<sem_class>> l_ptr) {
			if (l_ptr == r_ptr) {
				segment.destroy_ptr(l_ptr);
				return true;
			}
			return false;
		}
	private:
		subscriber<sem_class> r_ptr;
	};

public:
	signal_handler(shm_class& seg) :segment(seg) {
		static_assert(std::is_same<void_allocator::value_type, void>::value, "Need a void allocator in signal handler constructor");
		static_assert(std::is_same<void_allocator::segment_manager, shm_class::segment_manager>::value, "segment managers differ in signal handler constructor");
		void_alloc = seg.construct<void_allocator>(unique_instance)(seg.get_segment_manager());
		subscribers = seg.construct<vector<offset_ptr<subscriber<sem_class>>, subscriber_offset_ptr_allocator>>(unique_instance)(seg.get_segment_manager());
		signals_map = seg.construct<map<char_string, signal<sem_class, void_allocator, subscriber_offset_ptr_allocator>, std::less<char_string>, map_value_type_allocator>>(unique_instance)(seg.get_segment_manager());
	}

	void register_signal(const char* name) {
		char_string temp_name(name, *void_alloc);
		if (signals_map->count(temp_name) != 0) {
			throw;
		}
		signal<sem_class, void_allocator, subscriber_offset_ptr_allocator> temp_sig(*void_alloc);
		signals_map->insert(std::make_pair(temp_name, temp_sig));
	}

	void unregister_signal(const char* name) {
		char_string temp_name(name, *void_alloc);
		if (signals_map->count(temp_name) != 1) {
			throw;
		}
		auto iterpair = signals_map->equal_range(temp_name);
		for (auto it = iterpair.first; it != iterpair.second; ++it) {
			signals_map->erase(it);
			break;
		}
	}

	void post_signal(const char* name) {
		char_string temp_name(name, *void_alloc);
		auto it = signals_map->find(temp_name);

		if (it == signals_map->end()) {
			throw;
		}

		const auto& subs = *it;
		for (const auto& sub : subs.second.subscribers) {
			sub->sem.post();
		}
	}

	void create_subscriber(unsigned long pid) {
		// Check if it already exists
		if (std::find_if(subscribers->begin(), subscribers->end(), [&pid](const offset_ptr<subscriber<sem_class>> item) {
			return item->pid == pid;
		}) != subscribers->end()) {
			throw;
		}

		offset_ptr<subscriber<sem_class>> temp = segment.construct<subscriber<sem_class>>("Avinash")(pid);
		subscribers->push_back(temp);
	}

	sem_class* get_subscriber_semaphore(unsigned long pid) {
		auto it = std::find_if(subscribers->begin(), subscribers->end(), [&pid](const offset_ptr<subscriber<sem_class>> item) {
			return item->pid == pid;
		});
		if (it == subscribers->end()) {
			throw;
		}

		const auto& temp_sub = *it;
		return &temp_sub->sem;
	}

	void destroy_subscriber(unsigned long pid) {
		auto it = std::find_if(subscribers->begin(), subscribers->end(), [&pid](const offset_ptr<subscriber<sem_class>> item) {
			return item->pid == pid;
		});
		if (it == subscribers->end()) {
			throw;
		}

		auto& temp_ptr = *it;
		for (auto& kvp : *signals_map) {
			auto& temp_subvec = kvp.second.subscribers;
			for (auto it = temp_subvec.begin(); it != temp_subvec.end(); it++) {
				auto& l_ptr = *it;
				if (l_ptr.get() == temp_ptr.get()) {
					it = temp_subvec.erase(it);
					break;
				}
			}
		}

		subscribers->erase(std::remove_if(subscribers->begin(), subscribers->end(), [this, &pid](offset_ptr<subscriber<sem_class>> item) {
			if (item->pid == pid) {
				segment.destroy_ptr(item.get());
				return true;
			};
			return false;
		}), subscribers->end());
	}

	void register_subscriber(const char* sig_name, unsigned long pid) {
		char_string temp_name(sig_name, *void_alloc);
		auto map_iter = signals_map->find(temp_name);
		if (map_iter == signals_map->end()) {
			throw;
		}

		auto& map_entry = *map_iter;
		auto& map_vector = map_entry.second.subscribers;

		auto sub_iter = std::find_if(subscribers->begin(), subscribers->end(), [&pid](const offset_ptr<subscriber<sem_class>> item) {
			return item->pid == pid;
		});
		if (sub_iter == subscribers->end()) {
			throw;
		}
		map_vector.push_back(*sub_iter);
	}

	void unregister_subscriber(const char* sig_name, unsigned long pid) {
		char_string temp_name(sig_name, *void_alloc);
		auto map_iter = signals_map->find(temp_name);
		if (map_iter == signals_map->end()) {
			throw;
		}

		auto it = std::find_if(subscribers->begin(), subscribers->end(), [&pid](const offset_ptr<subscriber<sem_class>> item) {
			return item->pid == pid;
		});
		if (it == subscribers->end()) {
			throw;
		}

		auto temp_ptr = *it;
		auto& map_entry = *map_iter;
		auto& temp_subvec = map_entry.second.subscribers;
		std::cout << temp_subvec.size();
		for (auto it = temp_subvec.begin(); it != temp_subvec.end(); it++) {
			auto& l_ptr = *it;
			if (l_ptr.get() == temp_ptr.get()) {
				it = temp_subvec.erase(temp_subvec.begin());
				break;
			}
		}
	}
};
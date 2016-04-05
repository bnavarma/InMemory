// InMemory.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "shared_btree.h"
#include "signal_handler.h"
#include <iostream>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <array>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/allocators/node_allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>

//#include <boost/asio.hpp>

struct Data {
	unsigned x;
	unsigned y;
	unsigned z;
};

using namespace boost::interprocess;
using lockfree_managed_shared_memory = basic_managed_shared_memory<char, rbtree_best_fit<null_mutex_family>, iset_index>;
using ShmemAllocator = allocator<int, lockfree_managed_shared_memory::segment_manager>;
using MyVector = vector<int, ShmemAllocator>;

int main()
{
	struct shm_remove
	{
		shm_remove() { shared_memory_object::remove("MySharedMemory"); }
		~shm_remove() { shared_memory_object::remove("MySharedMemory"); }
	} remover;
	std::srand(unsigned(std::time(0)));
	//Create a new segment with given name and size
	lockfree_managed_shared_memory segment(create_only, "MySharedMemory", 536870912);
	managed_shared_memory asdf;
	//Initialize shared memory STL-compatible allocator
	ShmemAllocator alloc_inst(segment.get_segment_manager());
	using ShmemDataAllocator = node_allocator < Data, lockfree_managed_shared_memory::segment_manager>;
	using voidallocator = allocator<void, lockfree_managed_shared_memory::segment_manager>;
	voidallocator temp_void_alloc(segment.get_segment_manager());
	using DataOffsetPtr = offset_ptr<Data>;
	using ShmemDataVecAllocator = allocator<DataOffsetPtr, lockfree_managed_shared_memory::segment_manager>;
	const ShmemDataVecAllocator data_vec_alloc_inst(segment.get_segment_manager());
	ShmemDataAllocator data_alloc(segment.get_segment_manager());

	using ShmemSemaAllocator = allocator<interprocess_semaphore, lockfree_managed_shared_memory::segment_manager>;
	signal_handler < interprocess_semaphore, lockfree_managed_shared_memory, allocator ,lockfree_managed_shared_memory::segment_manager > tempasdf(segment/*, temp_void_alloc*/);
	std::array<int, 100> asdfjkl;
	tempasdf.register_signal("asdf");
	/*tempasdf.create_subscriber(123);
	tempasdf.register_subscriber("asdf", 123);
	tempasdf.get_subscriber_semaphore(123);
	tempasdf.post_signal("asdf");
	tempasdf.unregister_subscriber("asdf", 123);
	tempasdf.destroy_subscriber(123);*/
	using DataVec = vector < DataOffsetPtr, ShmemDataVecAllocator>;
	//Construct a vector named "MyVector" in shared memory with argument alloc_inst
	MyVector *myvector = segment.construct<MyVector>("MyVector")(alloc_inst);
	DataVec *data_vec = segment.construct<DataVec>("DataVec")(data_vec_alloc_inst);

	for (int i = 0; i < 100; ++i)  //Insert data in the vector
		myvector->push_back(i);
	auto x = 0;
	//auto inp = std::vector<Data*>();
	auto indexes = std::vector<int>();
	//for (auto i = 0; i < 10000000; i++) {
	//	auto temp = data_alloc.allocate_one();
	//	indexes.push_back(i);
	//	temp->x = i;
	//	temp->y = i + 1;
	//	temp->z = i + 2;
	//	data_vec->emplace_back(temp);
	//	//data_alloc.deallocate_one(temp_data);
	//}
	//std::random_shuffle(indexes.begin(), indexes.end());
	/*for (auto i = 0; i < 10000000; i++) {
		data_alloc.deallocate_one(inp.at(indexes.at(i)));
	}*/
	//Launch child process
	/*std::string s(argv[0]); s += " child ";
	if (0 != std::system(s.c_str()))
		return 1;*/

	//Check child has destroyed the vector
	/*auto temp = data_vec->back();
	std::cout << temp->x << temp->y << temp->z << std::endl;
	if (!segment.find<MyVector>("MyVector").first) {
		std::cout << "Destroyed" << std::endl;
		return 1;
	}*/
	/*auto inp = std::vector<Data*>();
	for (auto i = 0; i < 10000000; i++) {
		auto asdf = new Data;
		asdf->x = i + 1;
		inp.emplace_back(asdf);
	}*/
	auto asdfasdf = alloc_inst.allocate_one();
	auto tempadsf = asdfasdf;
	alloc_inst.deallocate_one(tempadsf);
	/*auto temp = shared_btree<unsigned long, Data,lockfree_managed_shared_memory,allocator,lockfree_managed_shared_memory::segment_manager>(&segment,temp_void_alloc);
	for (auto i = 0; i < inp.size(); i++) {
		temp.insert(i + 1, *inp[i]);
	}*/
	/*int x = 0;
	while (true) {
		std::cin >> x;
		if (x == 1) {
			break;
		}
	}*/
	/*try {
		boost::asio::io_service io_service;
		tcp_server server(io_service);
		io_service.run();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}*/
    return 0;
}
//#include <boost/asio/ip/tcp.hpp>
//using namespace boost::asio::ip;
//class tcp_server {
//public:
//	tcp_server(boost::asio::io_service& io_service) :acceptor_(io_service, tcp::endpoint(tcp::v4(), 5000)) {
//		start_accept();
//	}
//
//private:
//	void start_accept() {
//		tcp_connection::pointer new_connection = tcp_connection::create(acceptor_.io_service());
//		acceptor_.async_accept(new_connection->socket(), boost::bind(&tcp_server::handle_accept, this, new_connection, boost::asio::placeholders::error));
//	}
//
//	void handle_accept(tcp_connection::pointer new_connection, const boost::system::error_code& error) {
//		if (!error) {
//			new_connection->start();
//			start_accept();
//		}
//	}
//};
//
//class tcp_connection :public boost::enable_shared_from_this<tcp_connection> {
//public:
//	using pointer = boost::shared_ptr<tcp_connection>;
//	static pointer create(boost::asio::io_service& io_service) {
//		return pointer(new tcp_connection(io_service));
//	}
//
//	tcp::socket& socket() {
//		return socket_;
//	}
//
//	void start() {
//		message_ = "Avinash is King";
//		boost::asio::async_write(socket_, boost::asio::buffer(message_), boost::bind(&tcp_connection::handle_write, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
//	}
//
//private:
//	tcp_connection(boost::asio::io_service& io_service):socket_(io_service){}
//	void handle_write(const boost::system::error_code& error, size_t bytes_transferred){}
//
//	tcp::socket socket_;
//	std::string message_;
//};
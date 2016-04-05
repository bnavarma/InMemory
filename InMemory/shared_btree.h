#pragma once
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>
#include <boost/interprocess/smart_ptr/deleter.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

constexpr int BranchSize = 30;
constexpr int MaxSize = 100;
constexpr int DeleteIdx = (BranchSize / 2 - 1) > 0 ? BranchSize / 2 - 1 : 0;
constexpr bool UseDelete = DeleteIdx > 0;

using namespace boost::interprocess;
struct Node {
	bool isLeaf;
	virtual ~Node() {}

	Node() :isLeaf() {}
};

template<typename index_class>
struct IndexNode_ : public Node {
	offset_ptr<IndexNode_<index_class>> prev;
	offset_ptr<IndexNode_<index_class>> next;
	index_class keys[BranchSize];
	offset_ptr<Node> nodes[BranchSize + 1];

	IndexNode_() :prev(), next(), keys(), nodes() {}
	~IndexNode_() {}
};

template<typename index_class, typename data_class>
struct LeafNode_ : public Node {
	offset_ptr<LeafNode_<index_class, data_class>> prev;
	offset_ptr<LeafNode_<index_class, data_class>> next;
	index_class keys[BranchSize];
	offset_ptr<data_class> values[BranchSize];

	LeafNode_() :prev(), next(), keys(), values() {}
	~LeafNode_() {}
};

template<typename index_class>
struct key_node_pair {
	index_class key;
	Node* node;

	key_node_pair() :key(), node() {}
	~key_node_pair() {}
};

template<typename index_class, typename data_class, typename shm_class, template<class, class...> class allocator, class seg_manager, class... Args>
class shared_btree {
	using data_deleter = deleter<data_class, shm_class>;
	using data_shared_ptr = shared_ptr<data_class, shm_class, data_deleter>;
public:
	using IndexNode = IndexNode_<index_class>;
	using LeafNode = LeafNode_<index_class, data_class>;
	using void_allocator = allocator<void, seg_manager, Args...>;
	using data_allocator = allocator<data_class, seg_manager, Args...>;
	using tree_node_allocator = allocator<Node, seg_manager, Args...>;
	using idx_node_allocator = allocator<IndexNode, seg_manager, Args...>;
	using leaf_node_allocator = allocator<LeafNode, seg_manager, Args...>;

private:
	void_allocator& void_alloc;
	offset_ptr<idx_node_allocator> idx_alloc;
	offset_ptr<leaf_node_allocator> leaf_alloc;
	offset_ptr<data_allocator> data_alloc;

	offset_ptr<IndexNode> idx_alloc_construct() {
		auto ret = idx_alloc->allocate_one();
		new(ret.get()) IndexNode();
		//idx_alloc->construct(ret);
		return ret;
	}

	void idx_alloc_destroy(offset_ptr<IndexNode>& ptr) {
		auto temp = ptr.get();
		temp->IndexNode::~IndexNode_();
		idx_alloc->deallocate_one(ptr.get());
		//idx_alloc->destroy(ptr);
	}

	offset_ptr<LeafNode> leaf_alloc_construct() {
		auto ret = leaf_alloc->allocate_one();
		new(ret.get()) LeafNode();
		//leaf_alloc->construct(ret);
		return ret;
	}

	void leaf_alloc_destroy(offset_ptr<LeafNode>& ptr) {
		auto temp = ptr.get();
		temp->LeafNode::~LeafNode_();
		leaf_alloc->deallocate_one(temp);
		//leaf_alloc->destroy(ptr);
	}

	offset_ptr<data_class> data_alloc_construct() {
		auto ret = data_alloc->allocate_one();
		new(ret.get()) data_class();
		return ret;
	}

	void data_alloc_destroy(offset_ptr<data_class>& ptr) {
		auto temp = ptr.get();
		temp->data_class::~data_class();
		data_alloc->deallocate_one(ptr.get());
	}
public:
	interprocess_mutex btree_mutex;
	shm_class* segment;
	offset_ptr<Node> root;

	shared_btree(shm_class* seg, void_allocator& alloc) :segment(seg), void_alloc(alloc) {
		static_assert(std::is_same<void_allocator::value_type, void>::value, "Need a void allocator in signal handler constructor");
		static_assert(std::is_same<void_allocator::segment_manager, shm_class::segment_manager>::value, "segment managers differ in signal handler constructor");

		idx_alloc = segment->construct<idx_node_allocator>(anonymous_instance)(segment->get_segment_manager());
		leaf_alloc = segment->construct<leaf_node_allocator>(anonymous_instance)(segment->get_segment_manager());
		data_alloc = segment->construct<data_allocator>(anonymous_instance)(segment->get_segment_manager());
		//data_alloc = &segment->get_allocator<data_class>();
		root = leaf_alloc_construct();
		root->isLeaf = true;
	}

	void set_segment(shm_class* seg) {
		segment = seg;
	}

	void set_void_alloc(void_allocator& alloc) {
		void_alloc = alloc;
	}

	int search(IndexNode* idx_node, index_class key) {
		for (auto i = 0; i < BranchSize; i++) {
			if (idx_node->keys[i] == 0) {
				return i;
			}
			if (key <= idx_node->keys[i]) {
				return i;
			}
			else if (i == BranchSize - 1) {
				return BranchSize;
			}
		}
		throw;
	}

	int search(LeafNode* leaf_node, index_class key) {
		for (auto i = 0; i < BranchSize; i++) {
			if (leaf_node->keys[i] == 0) {
				return i;
			}
			if (key <= leaf_node->keys[i]) {
				return i;
			}
		}
		throw;
	}

	index_class node_split(IndexNode* idx_node, IndexNode* temp, index_class key, Node* data_node) {
		temp->isLeaf = false;
		temp->prev = idx_node;
		temp->next = idx_node->next;
		idx_node->next = temp;
		index_class ret = index_class();

		if (key < idx_node->keys[BranchSize / 2]) {
			for (auto i = BranchSize / 2; i < BranchSize; i++) {
				auto temp_node_idx = i - BranchSize / 2;
				if (i == BranchSize - 1) {
					temp->nodes[temp_node_idx + 1] = idx_node->nodes[i + 1].get();
					idx_node->nodes[i + 1] = nullptr;
				}
				temp->keys[temp_node_idx] = idx_node->keys[i];
				temp->nodes[temp_node_idx] = idx_node->nodes[i].get();
				idx_node->keys[i] = NULL;
				idx_node->nodes[i] = nullptr;
			}
			if (key > idx_node->keys[(BranchSize / 2) - 1]) {
				ret = key;
				idx_node->nodes[BranchSize / 2] = temp->nodes[0].get();
				temp->nodes[0] = data_node;
			}
			else {
				ret = idx_node->keys[(BranchSize / 2) - 1];
				idx_node->keys[(BranchSize / 2) - 1] = NULL;
				insert_non_split(idx_node, key, data_node);
			}
		}
		else {
			for (auto i = (BranchSize / 2) + 1; i <= BranchSize; i++) {
				auto temp_node_idx = i - (BranchSize / 2) - 1;
				if (i == BranchSize) {
					temp->nodes[temp_node_idx] = idx_node->nodes[i].get();
					idx_node->nodes[i] = nullptr;
				}
				else {
					temp->keys[temp_node_idx] = idx_node->keys[i];
					temp->nodes[temp_node_idx] = idx_node->nodes[i].get();
					idx_node->keys[i] = NULL;
					idx_node->nodes[i] = nullptr;
				}
			}
			ret = idx_node->keys[(BranchSize / 2)];
			idx_node->keys[(BranchSize / 2)] = NULL;
			insert_non_split(temp, key, data_node);
		}

		return ret;
	}

	index_class node_split(LeafNode* leaf_node, index_class key, data_class* value) {
		auto temp = leaf_alloc_construct().get();
		temp->isLeaf = true;
		temp->prev = leaf_node;
		temp->next = leaf_node->next;
		leaf_node->next = temp;
		index_class ret = index_class();

		if (key < leaf_node->keys[(BranchSize / 2)]) {
			for (auto i = BranchSize / 2; i < BranchSize; i++) {
				auto temp_node_idx = i - (BranchSize / 2);
				temp->keys[temp_node_idx] = leaf_node->keys[i];
				temp->values[temp_node_idx] = leaf_node->values[i];
				leaf_node->keys[i] = NULL;
				leaf_node->values[i] = nullptr;
			}
			insert_non_split(leaf_node, key, value);
			ret = leaf_node->keys[BranchSize / 2];
		}
		else {
			for (auto i = BranchSize / 2 + 1; i < BranchSize; i++) {
				auto temp_node_idx = i - (BranchSize / 2) - 1;
				temp->keys[temp_node_idx] = leaf_node->keys[i];
				temp->values[temp_node_idx] = leaf_node->values[i];
				leaf_node->keys[i] = NULL;
				leaf_node->values[i] = nullptr;
			}
			insert_non_split(temp, key, value);
			ret = leaf_node->keys[BranchSize / 2];
		}

		return ret;
	}

	void insert_non_split(IndexNode* idx_node, index_class key, Node* data_node) {
		auto idx = search(idx_node, key);

		idx_node->nodes[BranchSize] = idx_node->nodes[BranchSize - 1].get();
		for (auto i = BranchSize - 1; i > idx; i--) {
			idx_node->keys[i] = idx_node->keys[i - 1];
			idx_node->nodes[i] = idx_node->nodes[i - 1].get();
		}

		idx_node->keys[idx] = key;
		idx_node->nodes[idx + 1] = data_node;
	}

	void insert_non_split(LeafNode* leaf_node, index_class key, data_class* value) {
		auto idx = search(leaf_node, key);
		if (leaf_node->keys[idx] == key) {
			leaf_node->values[idx] = value;
			return;
		}

		for (auto i = BranchSize - 1; i > idx; i--) {
			leaf_node->keys[i] = leaf_node->keys[i - 1];
			leaf_node->values[i] = leaf_node->values[i - 1];
		}

		leaf_node->keys[idx] = key;
		leaf_node->values[idx] = value;
	}

	int insert_rec(IndexNode* idx_node, index_class key, data_class* value, key_node_pair<index_class>* kn_pair) {
		auto idx = search(idx_node, key);
		auto child_node = idx_node->nodes[idx];

		if (child_node->isLeaf) {
			auto leaf_node = static_cast<LeafNode*>(child_node.get());
			if (leaf_node->keys[BranchSize - 1] != NULL) {
				auto split_idx = node_split(leaf_node, key, value);
				if (idx_node->keys[BranchSize - 1] != NULL) {
					auto temp = idx_alloc_construct().get();
					temp->isLeaf = false;
					kn_pair->key = node_split(idx_node, temp, split_idx, leaf_node->next.get());
					kn_pair->node = temp;
					return 1;
				}
				else {
					insert_non_split(idx_node, split_idx, leaf_node->next.get());
					return NULL;
				}
			}
			else {
				insert_non_split(leaf_node, key, value);
				return NULL;
			}
		}
		else {
			auto idx_child_node = static_cast<IndexNode*>(child_node.get());
			auto temp_kn_pair = key_node_pair<index_class>();
			auto rec_value = insert_rec(idx_child_node, key, value, &temp_kn_pair);
			if (rec_value != NULL) {
				if (idx_node->keys[BranchSize - 1] != NULL) {
					auto temp = idx_alloc_construct().get();
					new(temp) IndexNode();
					temp->isLeaf = false;
					kn_pair->key = node_split(idx_node, temp, temp_kn_pair.key, temp_kn_pair.node);
					kn_pair->node = temp;
					return 1;
				}
				else {
					insert_non_split(idx_node, temp_kn_pair.key, temp_kn_pair.node);
					return NULL;
				}
			}
			return NULL;
		}
	}

	void insert(index_class key, const data_class& value_ptr) {
		auto value = data_alloc_construct().get();
		*value = value_ptr;
		if (root->isLeaf) {
			auto leaf_node = static_cast<LeafNode*>(root.get());
			if (leaf_node->keys[BranchSize - 1] != NULL) {
				auto temp = idx_alloc_construct().get();
				new(temp) IndexNode();
				temp->isLeaf = false;
				temp->keys[0] = node_split(leaf_node, key, value);
				temp->nodes[0] = leaf_node;
				temp->nodes[1] = leaf_node->next;
				root = temp;
				return;
			}
			else {
				insert_non_split(leaf_node, key, value);
				return;
			}
		}

		auto idx_node = static_cast<IndexNode*>(root.get());
		auto idx = search(idx_node, key);
		auto idx_child_node = static_cast<IndexNode*>(idx_node->nodes[idx].get());

		auto temp_kn_pair = key_node_pair<index_class>();
		auto rec_value = insert_rec(idx_node, key, value, &temp_kn_pair);
		if (rec_value != NULL) {
			auto temp_root = idx_alloc_construct().get();
			temp_root->isLeaf = false;
			temp_root->nodes[0] = idx_node;
			insert_non_split(temp_root, temp_kn_pair.key, temp_kn_pair.node);
			root = temp_root;
		}
	}

	std::unique_ptr<data_class> find(index_class key) {
		std::unique_ptr<data_class> ret{ nullptr };
		auto temp = root;
		while (!temp->isLeaf) {
			auto temp_idx_node = static_cast<IndexNode*>(temp.get());
			auto idx = search(temp_idx_node, key);
			temp = temp_idx_node->nodes[idx];
		}

		auto child_idx_node = static_cast<LeafNode*>(temp.get());
		auto idx = search(child_idx_node, key);
		if (child_idx_node->keys[idx] == key) {
			ret = std::make_unique<data_class>();
			*ret = *child_idx_node->values[idx].get();
			return std::move(ret);
		}

		return std::move(ret);
	}

	void traverse() {
		if (!root->isLeaf) {
			auto temp = static_cast<IndexNode*>(root.get());
			while (1) {
				auto temp_next = temp;
				while (temp_next != nullptr) {
					for (auto i = 0; i < BranchSize; i++) {
						if (temp_next->keys[i] == NULL) {
							break;
						}
						std::cout << temp_next->keys[i] << " ";
					}
					std::cout << "\t";
					temp_next = temp_next->next.get();
				}
				std::cout << std::endl << std::endl;
				if (temp->nodes[0]->isLeaf) {
					break;
				}
				else {
					temp = static_cast<IndexNode*>(temp->nodes[0].get());
				}
			}
			auto first_leaf = static_cast<LeafNode*>(temp->nodes[0].get());
			while (first_leaf->next != NULL) {
				for (auto i = 0; i < BranchSize; i++) {
					if (first_leaf->keys[i] != NULL) {
						std::cout << first_leaf->keys[i] << " ";
					}
					else {
						std::cout << "\t";
						break;
					}
				}
				first_leaf = first_leaf->next.get();
			}
			for (auto i = 0; i < BranchSize; i++) {
				if (first_leaf->keys[i] != NULL) {
					std::cout << first_leaf->keys[i] << " ";
				}
				else {
					std::cout << "\t";
					break;
				}
			}
		}
		else {
			auto first_leaf = static_cast<LeafNode*>(root.get());
			for (auto i = 0; i < BranchSize; i++) {
				if (first_leaf->keys[i] != NULL) {
					std::cout << first_leaf->keys[i] << " ";
				}
				else {
					std::cout << "\t";
					break;
				}
			}
		}
		std::cout << std::endl << std::endl;
	}

	struct delete_check_struct {
		static const bool value = UseDelete;
	};

	void del_compact(IndexNode* idx_node, int idx) {
		for (auto i = idx; i < BranchSize - 1; i++) {
			idx_node->keys[i] = idx_node->keys[i + 1];
			idx_node->nodes[i + 1] = idx_node->nodes[i + 2];
		}
		idx_node->keys[BranchSize - 1] = NULL;
		idx_node->nodes[BranchSize] = nullptr;
	}

	void del_compact(LeafNode* leaf_node, int idx) {
		data_alloc_destroy(leaf_node->values[idx]);

		for (auto i = idx; i < BranchSize - 1; i++) {
			leaf_node->keys[i] = leaf_node->keys[i + 1];
			leaf_node->values[i] = leaf_node->values[i + 1];
		}
		leaf_node->keys[BranchSize - 1] = NULL;
		leaf_node->values[BranchSize - 1] = nullptr;
	}

	template<typename U = index_class>
	U delete_rec(U key, IndexNode* idx_node, typename std::enable_if_t<delete_check_struct::value && std::is_same<U, index_class>::value>* = nullptr) {
		auto idx = search(idx_node, key);
		auto child_node = idx_node->nodes[idx];
		auto ret = U{};

		if (child_node->isLeaf) {
			auto leaf_node = static_cast<LeafNode*>(child_node.get());
			auto leaf_idx = search(leaf_node, key);
			if (leaf_node->keys[leaf_idx] != key) {
				return NULL;
			}
			ret = leaf_node->keys[leaf_idx - 1];
			del_compact(leaf_node, leaf_idx);
			if (key == idx_node->keys[idx]) {
				idx_node->keys[idx] = ret;
				ret = NULL;
			}
			if (leaf_node->keys[DeleteIdx] == NULL) {
				auto idx_root = static_cast<IndexNode*>(root.get());
				if (idx_root == idx_node && idx_root->keys[1] == NULL) {
					auto lf_node1 = static_cast<LeafNode*>(idx_root->nodes[0].get());
					auto lf_node2 = static_cast<LeafNode*>(idx_root->nodes[1].get());
					auto node1_endidx = 0, node2_endidx = 0;
					for (auto i = 0; i < BranchSize; i++) {
						if (lf_node1->keys[i] == NULL && node1_endidx == 0) {
							node1_endidx = i;
						}
						if (lf_node2->keys[i] == NULL && node2_endidx == 0) {
							node2_endidx = i;
						}
						if (i == BranchSize - 1) {
							if (node1_endidx == 0) {
								node1_endidx = BranchSize;
							}
							if (node2_endidx == 0) {
								node2_endidx = BranchSize;
							}
						}
					}
					if (node1_endidx + node2_endidx <= BranchSize) {
						for (auto i = node1_endidx; i < node1_endidx + node2_endidx; i++) {
							lf_node1->keys[i] = lf_node2->keys[i - node1_endidx];
							lf_node1->values[i] = lf_node2->values[i - node1_endidx];
						}
						lf_node1->next = nullptr;
						offset_ptr<LeafNode> temp_leaf_offptr = lf_node2;
						leaf_alloc_destroy(temp_leaf_offptr);
						root = idx_root->nodes[0];
						offset_ptr<IndexNode> temp_idx_offptr = idx_root;
						idx_alloc_destroy(temp_idx_offptr);
						return false;
					}
				}
				auto node1_idx = idx == 0 ? 0 : idx - 1;
				auto node2_idx = idx == 0 ? 1 : idx;
				auto lf_node1 = static_cast<LeafNode*>(idx_node->nodes[node1_idx].get());
				auto lf_node2 = static_cast<LeafNode*>(idx_node->nodes[node2_idx].get());
				if (lf_node2 == nullptr) {
					// Last remaining 
					return NULL;
				}
				auto node1_endidx = 0, node2_endidx = 0;
				for (auto i = 0; i < BranchSize; i++) {
					if (lf_node1->keys[i] == NULL && node1_endidx == 0) {
						node1_endidx = i;
					}
					if (lf_node2->keys[i] == NULL && node2_endidx == 0) {
						node2_endidx = i;
					}
					if (i == BranchSize - 1) {
						if (node1_endidx == 0) {
							node1_endidx = BranchSize;
						}
						if (node2_endidx == 0) {
							node2_endidx = BranchSize;
						}
					}
				}
				if (node1_endidx + node2_endidx > BranchSize) {
					if (node1_endidx > BranchSize / 2) {
						for (auto i = node1_endidx; i > 0; i--) {
							lf_node2->keys[i] = lf_node2->keys[i - 1];
							lf_node2->values[i] = lf_node2->values[i - 1];
						}
						for (auto i = BranchSize / 2; i < node1_endidx; i++) {
							lf_node2->keys[i] = lf_node1->keys[i];
							lf_node2->values[i] = lf_node1->values[i];
							lf_node1->keys[i] = NULL;
							lf_node1->values[i] = NULL;
						}
					}
					else {
						auto temp_gap = (BranchSize / 2) - node1_endidx;
						for (auto i = node1_endidx; i < BranchSize / 2; i++) {
							lf_node1->keys[i] = lf_node2->keys[i - node1_endidx];
							lf_node1->values[i] = lf_node2->values[i - node1_endidx];
						}
						for (auto i = 0; i < node2_endidx - temp_gap; i++) {
							lf_node2->keys[i] = lf_node2->keys[i + temp_gap];
							lf_node2->values[i] = lf_node2->values[i + temp_gap];
						}
						for (auto i = node2_endidx - temp_gap; i < node2_endidx; i++) {
							lf_node2->keys[i] = NULL;
							lf_node2->values[i] = NULL;
						}
					}
					idx_node->keys[node1_idx] = lf_node1->keys[BranchSize / 2 - 1];
					return NULL;
				}
				else {
					for (auto i = node1_endidx; i < node1_endidx + node2_endidx; i++) {
						lf_node1->keys[i] = lf_node2->keys[i - node1_endidx];
						lf_node1->values[i] = lf_node2->values[i - node1_endidx];
					}
					lf_node1->next = lf_node2->next;
					if (lf_node1->next != nullptr) {
						lf_node1->next->prev = lf_node1;
					}
					/*segment->destroy_ptr(lf_node2);*/
					offset_ptr<LeafNode> temp = lf_node2;
					leaf_alloc_destroy(temp);
					del_compact(idx_node, node1_idx);
					return ret;
				}
			}
			return ret;
		}
		else {
			auto child_idx_node = static_cast<IndexNode*>(child_node.get());
			auto del_status = delete_rec(key, child_idx_node);
			if (key == idx_node->keys[idx]) {
				idx_node->keys[idx] = del_status;
			}
			if (child_idx_node->keys[DeleteIdx] == NULL) {
				auto idx_root = static_cast<IndexNode*>(root.get());
				if (idx_root == idx_node && idx_root->keys[1] == NULL) {
					auto idx_node1 = static_cast<IndexNode*>(idx_root->nodes[0].get());
					auto idx_node2 = static_cast<IndexNode*>(idx_root->nodes[1].get());
					auto node1_endidx = 0, node2_endidx = 0;
					for (auto i = 0; i < BranchSize; i++) {
						if (idx_node1->keys[i] == NULL && node1_endidx == 0) {
							node1_endidx = i;
						}
						if (idx_node2->keys[i] == NULL && node2_endidx == 0) {
							node2_endidx = i;
						}
						if (i == BranchSize - 1) {
							if (node1_endidx == 0) {
								node1_endidx = BranchSize;
							}
							if (node2_endidx == 0) {
								node2_endidx = BranchSize;
							}
						}
					}
					if (node1_endidx + node2_endidx < BranchSize) {
						idx_node1->keys[node1_endidx] = idx_root->keys[0];
						for (auto i = node1_endidx + 1; i <= node1_endidx + node2_endidx; i++) {
							idx_node1->keys[i] = idx_node2->keys[i - node1_endidx - 1];
							idx_node1->nodes[i] = idx_node2->nodes[i - node1_endidx - 1];
						}
						idx_node1->nodes[node1_endidx + node2_endidx + 1] = idx_node2->nodes[node2_endidx];
						idx_node1->next = nullptr;
						offset_ptr<IndexNode> temp_idx2_offptr = idx_node2;
						idx_alloc_destroy(temp_idx2_offptr);
						root = idx_root->nodes[0];
						offset_ptr<IndexNode> temp_root_offptr = idx_root;
						idx_alloc_destroy(temp_root_offptr);
						return del_status;
					}
				}
				auto node1_idx = idx == 0 ? 0 : idx - 1;
				auto node2_idx = idx == 0 ? 1 : idx;
				auto idx_node1 = static_cast<IndexNode*>(idx_node->nodes[node1_idx].get());
				auto idx_node2 = static_cast<IndexNode*>(idx_node->nodes[node2_idx].get());
				if (idx_node2 == nullptr) {
					return del_status;
				}
				auto node1_endidx = 0, node2_endidx = 0;
				for (auto i = 0; i < BranchSize; i++) {
					if (idx_node1->keys[i] == NULL && node1_endidx == 0) {
						node1_endidx = i;
					}
					if (idx_node2->keys[i] == NULL && node2_endidx == 0) {
						node2_endidx = i;
					}
					if (i == BranchSize - 1) {
						if (node1_endidx == 0) {
							node1_endidx = BranchSize;
						}
						if (node2_endidx == 0) {
							node2_endidx = BranchSize;
						}
					}
				}
				if (node1_endidx + node2_endidx >= BranchSize) {
					if (node1_endidx > BranchSize / 2) {
						auto temp_gap = node1_endidx - (BranchSize / 2);
						idx_node2->nodes[BranchSize] = idx_node2->nodes[BranchSize - temp_gap];
						for (auto i = BranchSize - 1; i >= temp_gap; i--) {
							idx_node2->keys[i] = idx_node2->keys[i - temp_gap];
							idx_node2->nodes[i] = idx_node2->nodes[i - temp_gap];
						}
						idx_node2->keys[temp_gap - 1] = idx_node->keys[node1_idx];
						idx_node2->nodes[temp_gap - 1] = idx_node1->nodes[node1_endidx];
						idx_node1->nodes[node1_endidx] = nullptr;
						for (auto i = (BranchSize / 2) + 1; i < node1_endidx; i++) {
							idx_node2->keys[i - (BranchSize / 2)-1] = idx_node1->keys[i];
							idx_node2->nodes[i - (BranchSize / 2)-1] = idx_node1->nodes[i];
							idx_node1->keys[i] = NULL;
							idx_node1->nodes[i] = nullptr;
						}
						idx_node->keys[node1_idx] = idx_node1->keys[BranchSize / 2];
						idx_node1->keys[BranchSize / 2] = NULL;
					}
					else {
						idx_node1->keys[node1_endidx] = idx_node->keys[node1_idx];
						auto temp_gap = (BranchSize / 2) - node1_endidx;
						for (auto i = node1_endidx + 1; i < (BranchSize / 2) + 1; i++) {
							auto temp_ridx = i - node1_endidx - 1;
							idx_node1->keys[i] = idx_node2->keys[temp_ridx];
							idx_node1->nodes[i] = idx_node2->nodes[temp_ridx];
						}
						for (auto i = 0; i < BranchSize - temp_gap; i++) {
							idx_node2->keys[i] = idx_node2->keys[i + temp_gap];
							idx_node2->nodes[i] = idx_node2->nodes[i + temp_gap];
						}
						idx_node2->nodes[BranchSize - temp_gap] = idx_node2->nodes[BranchSize];
						for (auto i = BranchSize - temp_gap; i < BranchSize; i++) {
							idx_node2->keys[i] = NULL;
							idx_node2->nodes[i + 1] = nullptr;
						}
						idx_node->keys[node1_idx] = idx_node1->keys[(BranchSize / 2)];
						idx_node1->keys[(BranchSize / 2)] = NULL;
					}
					return del_status;
				}
				else {
					idx_node1->keys[node1_endidx] = idx_node->keys[node1_idx];
					for (auto i = node1_endidx + 1; i <= node1_endidx + node2_endidx; i++) {
						idx_node1->keys[i] = idx_node2->keys[i - node1_endidx - 1];
						idx_node1->nodes[i] = idx_node2->nodes[i - node1_endidx - 1];
					}
					idx_node1->nodes[node1_endidx + node2_endidx + 1] = idx_node2->nodes[node2_endidx];
					idx_node1->next = idx_node2->next;
					if (idx_node1->next != nullptr) {
						idx_node1->next->prev = idx_node1;
					}
					offset_ptr<IndexNode> temp = idx_node2;
					idx_alloc_destroy(temp);
					del_compact(idx_node, node1_idx);
					return del_status;
				}
			}
			return del_status;
		}

		return NULL;
	}

	template<typename U = index_class>
	bool delete_rec(U key, typename std::enable_if_t<!delete_check_struct::value && std::is_same<U, index_class>::value>* = nullptr) {

	}

	template<typename U = index_class>
	void delete_data(U key, typename std::enable_if_t<delete_check_struct::value && std::is_same<U, index_class>::value>* = nullptr) {
		if (root->isLeaf) {
			auto lf_root = static_cast<LeafNode*>(root.get());
			auto idx = search(lf_root, key);
			if (key != lf_root->keys[idx]) {
				return;
			}
			del_compact(lf_root, idx);
			return;
		}

		auto idx_root = static_cast<IndexNode*>(root.get());
		auto del_status = delete_rec(key, idx_root);
		return;
	}

	template<typename U = index_class>
	void delete_data(U key, typename std::enable_if_t<!delete_check_struct::value && std::is_same<U, index_class>::value>* = nullptr) {
		std::cout << "Not in the shit" << std::endl;
	}
};
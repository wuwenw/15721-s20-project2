#include <map>
#include <vector>
#include "index.h"

namespace terrier::storage::index {
template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class InnerList {
 public:
  KeyType key_;
  ValueType value_;
  InnerList *prev_;
  InnerList *next_;
  InnerList *dup_next_;  // singly linked list for storing duplicated key

  InnerList(KeyType key, ValueType val, InnerList *prev = nullptr, InnerList *next = nullptr) {
    *key_ = key;
    *value_ = val;
    prev_ = prev;
    next_ = next;
    dup_next_ = nullptr;
  }

  InnerList(InnerList *reference) {
    *key_ = reference->key_;
    *value_ = reference->value_;
    prev_ = reference->prev_;
    next_ = reference->next_;
    dup_next_ = reference->dup_next_;
  }

  // insert at the fron of the current value node
  void InsertFront(InnerList *new_value) {
    InnerList *cur_value = this;
    InnerList *prev = cur_value->prev_;
    if (prev != nullptr) {
      prev->next_ = new_value;
    }
    new_value->prev_ = prev;
    new_value->next_ = cur_value;
    cur_value->prev_ = new_value;
  }

  void InsertBack(InnerList *new_value) {
    InnerList *cur_value = this;
    InnerList *next = cur_value->next_;
    if (next != nullptr) {
      next->prev_ = new_value;
    }
    new_value->next_ = next;
    cur_value->next = new_value;
    new_value->prev_ = cur_value;
  }

  bool isInDupChain() { return dup_next == nullptr; }

  void InsertDup(InnerList *new_value) {
    InnerList *cur = this;
    InnerList *next = dup_next_;
    if (next == nullptr) {
      dup_next = new_value;
      return;
    }
    while (next != nullptr) {
      cur = next;
      next = cur->dup_next_;
    }
    cur->dup_next_ = new_value;
  }
  // pop the current node from linked list
  // set poped prev next to null
  // re link the linked list
  // return the poped linked list
  InnerList *PopListHere() {
    InnerList *cur_node = this;
    InnerList *prev = cur_node->prev_;
    InnerList next = cur_node->next_;
    cur_node->prev_ = nullptr;
    cur_node->next_ = nullptr;
    prev->next_ = next;
    next->prev_ = prev;
    return cur_node;
  }
  // detach the linked list from current node left
  // return the ptr to the new right side
  InnerList *DetachListHere() {
    InnerList *cur_node = this;
    InnerList right_start = cur_node;
    if (cur_node->prev_ != nullptr) {
      InnerList *left = cur_node->prev_;
      left->next_ = nullptr;
    }
    cur_node->prev_ = nullptr;
    return cur_node;
  }
};

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class TreeNode {
 public:
  size_t size;
  InnerList *value_list_;              // list of value points
  InnerList *value_list_end_;          // Only leaf node, mark the end of the current node value list
  std::vector<TreeNode *> *ptr_list_;  // list of pointers to the next treeNode
  TreeNode *parent_;

  TreeNode(TreeNode *parent, value_list = nullptr, ptr_list = nullptr, parent = nullptr; sibling = nullptr) {
    value_list_ = value_list;
    ptr_list_ = ptr_list;
    parent_ = parent;
    sibling_ = sibling;
    size = 0;
    while (value_list != nullptr) {
      size++;
      value_list = valie_list->next_;
    }
    if (ptr_list_ == null_ptr) {
      ptr_list_ = new vector<InnerList *>();
    }
  }
  ~TreeNode() {}

  typedef struct SplitReturn {
    InnerList *split_value;
    TreeNode *parent;
    TreeNode *left_child;
    TreeNode *right_child;
  } SplitReturn;

  bool isLeaf() { return ptr_list_ == nullptr; }
  bool ShouldSplit(size_t order) { return size > order; }
  /**
   * Recursively find the position to perform insert.
   * Terminate if reach the child node, insert into it
   * otherwise find the best child node and recursively perform insertion
   **/
  TreeNode *Insert(InnerList *new_value) {
    bool result = nullptr;
    if (isLeaf()) {
      result = insertAtLeafNode(new_value);
    } else {
      TreeNode *child_node = findBestFitChild(new_value->key_);
      result = child_node->Insert(new_value);
    }
    return result;
  }

  TreeNode *InsertUnique(InnerList *new_value) {
    // TODO: implement unique insertion
    return Insert(new_value);
  }
  TreeNode *Delete(KeyType key, ValueType value) { return nullptr; }

  // going from leaf to root, recursively split child, insert a new node at parent
  // in case split fails at current level, return nullptr, restore  tree from failed node
  TreeNode *Split(TreeNode *cur_node, TreeNode *root_node, size_t order, vector<InnerList *> restore_stack,
                  InnerList *split_value_list = nullptr, TreeNode *left_child = nullptr,
                  TreeNode *right_child = nullptr) {
    restore_stack.push_back(split_value_list);
    // base case: root is split, create a new root and return it
    if (cur_node == nullptr) {
      TreeNode *new_root = new TreeNode(nullptr);
      // if the creation fails
      if (new_root == nullptr) {
        return RestoreTreeFromNode(left_child, right_child, restore_stack);
      }
      new_root->ConfigureNewSplitNode(split_value_list, left_child, right_child);
      return new_root
    }

    if (cur_node->isLeaf()) {
      // base case: leaf node and no need to split
      if (!ShouldSplit(order)) return root_node;
    } else {
      // insert the new node in current
      cur_node->ConfigureNewSplitNode(split_value_list, left_child, right_child);
    }
    // check if need to split
    if (!ShouldSplit(order)) {
      return root_node;
    }
    // otherwise split the current node
    SplitReturn *split_res = SplitNode(cur_node);
    // fail to split into two nodes
    if (split_res == nullptr) {
      return RestoreTreeFromNode(left_node, right_node, restore_stack);
    }
    return Split(split_res->parent, root_node, order, restore_stack, split_res->split_value, split_res->left_child,
                 split_res->right_child);
  }

 private:
  // assuming this is a leafNode
  TreeNode *insertAtLeafNode(InnerList *new_list) {
    KeyType key = new_list->key_;
    ValueType val = new_list->value_;
    TreeNode *result = nullptr;
    InnerList *cur = value_list_;
    InnerList *next = nullptr;
    while (cur != nullptr) {
      // if contains key just append in the key linked list
      if (cur->key_ == key) {
        bool sub_res = cur->InsertDup(new_list);
        result = this;
      } else {
        // else, find best fit point to insert a new node
        next = cur->next_;
        if (next != nullptr && next->key_ <= key)
          cur = cur->next_;
        else {
          // if should insert at the front
          if (cur->key_ > key) {
            cur->InsertFront(new_list);
            if (cur == value_list_) {
              value_list_ = new_list;
            }
            size++;
            break;
          }
          // if place between cur and next
          else {
            cur->InsertBack(new_list);
            if (cur == value_list_end_) {
              value_list_end_ = new_list;
            }
            size++;
            break;
          }
        }
      }
    }
    return this;
  }
  /**
   * Assuming that current node is not leaf node
   **/
  TreeNode *findBestFitChild(KeyType key) {
    InnerList *cur_val = value_list_;
    InnerList *next_val;
    std::list<TreeNode *>::iterator ptr_iter = ptr_list_.begin(); // left side of the ptr list
    while (cur_val != nullptr) {
      if (cur_val->key_ > key) {
        return *ptr_iter;
      }
      ++ptr_iter;
      next_val = cur_val->next_;
      if (next_val == nullptr)
        return *ptr_iter;
      else if (next_val->key_ > key)
        return *ptr_iter;
      else
        cur_val = next_val;
    }
  }
  // return the top node and recursively restore child according to restoring stack
  TreeNode *RestoreTreeFromNode(TreeNode *left_node, TreeNode *right_node, vector<InnerList *> restore_stack);
  // configure a new node, insert split value, left child, right child
  // return the finished node
  void ConfigureNewSplitNode(InnerList *split_value_list, TreeNode *left_child,
                             TreeNode *right_child) {
    // case 1, the node to config is an empty node
    if (this->size == 0) {
      this->value_list_ = split_value_list;
      this->ptr_list_.push_back(left_child);
      this->ptr_list_.push_back(right_child);
    }
    // case 2, the node to config is in a parent level
    else {
      // find a position to insert value into
      InnerList *cur_value = value_list_;
      std::vector<TreeNode*>::iterator ptr_list_iter = this->ptr_list_->begin();
      // lterate untill theoriginal ptr position using left node as original node
      while ((*ptr_list_iter) != left_node) {
        next_value = cur_value->next_;
        ++ptr_list_iter;
      }
      // insert right ptr at the right of current left ptr
      ++ptr_list_iter;
      // insert the new value into the value list
      // if at the end of the value list insert at the back of the value list
      if (cur_value == nullptr) {
        TERRIER_ASSERT(ptr_list_iter == this->ptr_list_.end(), "insert should at the ptr end when cur_value is null");
        InnerList *end = this->value_list_end_;
        end->InsertBack(split_value_list);
        this->value_list_end_ = end->next_;
      }
      // if at the start of the value_list insert at the very front
      else if (cur_value->prev_ == nullptr) {
        TERRIER_ASSERT(ptr_list_iter == this->ptr_list_.begin(), "insert should at the ptr start when cur_value->prev_ is null");
        TERRIER_ASSERT(cur_value == this->value_list_, "cur_value should also point to value_list_ start");
        this->value_list_->InsertFront(split_value_list);
        this->value_list_ = this->value_list_->prev_;
      }
      // if at the middle of the value_list, insert at the front 
      // as the ptr_list is the left of the real value
      else{
        cur_value->InsertFront(split_value_list);
      }
      // insert the new right side pointer
      this->ptr_list_.insert(ptr_list_iter, right_child);
    }
    // increase the current node size as we insert a new value
    this.size ++;
  }

  // assume the node exceeds capacity
  // split the current node into two for left and non-leaf
  SplitReturn *SplitNode(TreeNode node) {
    SplitReturn result;
    size_t split_index = size / 2;
    size_t cur_index = 0;
    InnerList split_list = value_list_;
    // get the split_list location
    while (cur_index != split_index) {
      split_list = split->next_;
      cur_index++;
    }
    // create a new TreeNode that has the same parent
    TreeNode *right_tree_node = new TreeNode(node->parent_);
    TreeNode left_tree_node = node;
    if (right_tree_node == nullptr) return nullptr;
    result.parent = left_tree_node->parent_;
    result.left_child = left_tree_node;
    result.right_child = right_tree_node;

    if (node->isLeaf()) {
      InnerList *split_value = new InnerList(split_list);
      if (split_value == nullptr) return nullptr;
      split_value->dup_list_ = nullptr;  // value to be inserted to parent node, only key matters
      // configure value list
      right_tree_node->value_list_ = split_list;
      right_tree_node->value_list_end = left_tree_node->value_list_end_;
      left_tree_node->value_list_end_ = split_list->prev_;
      // configure size
      right_tree_node->size = left_tree_node->size - cur_index;
      left_tree_node->size = cur_index;
      result.split_value = split_value;
    } else {
      // if none leaf node
      // configure the value list pop the value out of the value list
      right_tree_node->value_list_ = split_list->next_;
      right_tree_node->value_list_end_ = left_tree_node->value_list_end_;
      left_tree_node->value_list_end = split_list->prev_;
      // configure ptr_list
      // cur_index is the left ptr in the poped node
      // should remain at the left side, right node keeps everything from cur_index + 1
      size_t popping_index = left_tree_node->size;  // ptr lise has size + 1 ptrs
      InnerList *tmp_ptr;
      while (popping_index > cur_index) {
        tmp_ptr = left_tree_node->ptr_list_.pop_back();
        right_tree_node->ptr_list_->insert(right_tree_node->ptr_list_->begin(), tmp_ptr);
        popping_index--;
      }
      // configure size
      right_tree_node->size = left_tree_node->size - cur_index - 1;  // middle value is poped out
      left_tree_node->size = cur_index;
      result.split_value = split_list->DetachListHere();
    }
    return &result;
  }
};

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class BPlusTree {
 public:
  TreeNode *root;  // with parent node as empty for root
  size_t order_;
  BPlusTree(size_t order_) {
    order_ = order;
    root = new TreeNode(nullptr);
  }

  bool Insert(KeyType key, ValueType value) {
    bool result = false;
    TreeNode *new_root = nullptr;
    InnerList *new_value = new InnerList(key, value);
    if (new_value == nullptr) return false;
    root->Insert(key, new_value);
    new_root = RebalanceTree(result, key, value);
    if (new_root == nullptr) return false;
    root = new_root;
    return true;
  }

 private:
  InnerList *RebalanceTree(TreeNode *leaf_node) {
    // wrapper for recursively rebalance the tree
    // if size exceeds, pop the middle element going from child to parent
    vector<InnerList *> restore_stack;
    return Split(left_node, root, order *, restore_stack);
  }
};
}  // namespace terrier::storage::index
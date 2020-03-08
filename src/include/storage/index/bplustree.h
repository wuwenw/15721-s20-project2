#include <vector>
#include "storage/index/index.h"

namespace terrier::storage::index {
    
template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class BPlusTree {
 public:

  class TreeNode {
   public:

    class InnerList {
     public:
      KeyType key_;
      ValueType value_;
      InnerList *prev_;
      InnerList *next_;
      std::vector<ValueType> same_key_values_;
      // standard constructor using key and value
      InnerList(KeyType key, ValueType val, InnerList *prev = nullptr, InnerList *next = nullptr) {
        key_ = key;
        value_ = val;
        prev_ = prev;
        next_ = next;
        // push the first value into the same_key_values_ as well
        // TODO: either having a better way to manage the repeated key and value or get rid of value_ field
        same_key_values_.push_back(val);
      }
      // copy constructor to construct a InnerList from reference
      InnerList(InnerList *reference) {
        key_ = reference->key_;
        value_ = reference->value_;
        prev_ = reference->prev_;
        next_ = reference->next_;
        same_key_values_.insert(reference->same_key_values_.begin(), reference->same_key_values_.end());
      }
      ~InnerList() {
        InnerList *next;
        // TODO: Do we need to delete every element in the same key value?
        // delete same_key_values_;
        // TODO: do we need to delete the key and value?
        /*
        delete key_;
        delete value_;
        */
      }
      std::vector<ValueType> GetAllValues() { return same_key_values_; }

      // insert at the fron of the current value node
      void InsertFront(InnerList *new_value) {
        TERRIER_ASSERT(new_value != nullptr,
                       "insertFront do not accept null InnerList to be inserted");
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
        TERRIER_ASSERT(new_value != nullptr,
                       "insertBack do not accept null InnerList to be inserted");
        InnerList *cur_value = this;
        InnerList *next = cur_value->next_;
        if (next != nullptr) {
          next->prev_ = new_value;
        }
        new_value->next_ = next;
        cur_value->next_ = new_value;
        new_value->prev_ = cur_value;
      }

      void InsertDup(InnerList *new_value) {
        TERRIER_ASSERT(new_value != nullptr,
                       "insertDup do not accept null InnerList to be inserted");
        TERRIER_ASSERT(new_value->key_ == this->key_,
                       "insertDup should insert at Innerlist with same key as the new_value");
        same_key_values_.push_back((new_value->value_));
        // void the value_field to prevent deletion
        new_value->value_ = nullptr;
        delete new_value;
      }
      // pop the current node from linked list
      // set poped prev next to null
      // re link the linked list
      // return the poped linked list
      InnerList *PopListHere() {
        InnerList *cur_node = this;
        InnerList *prev = cur_node->prev_;
        InnerList *next = cur_node->next_;
        cur_node->prev_ = nullptr;
        cur_node->next_ = nullptr;
        if (prev != nullptr) prev->next_ = next;
        if (next != nullptr) next->prev_ = prev;
        return cur_node;
      }
    };  // end class InnerList
    size_t size;
    InnerList *value_list_;              // list of value points, point at the start
    std::vector<TreeNode *> ptr_list_;  // list of pointers to the next treeNode, left node have size zero
    TreeNode *parent_;
    TreeNode *left_sibling_;
    TreeNode *right_sibling_;  // only leaf node has siblings
    TreeNode(TreeNode *parent, InnerList *value_list = nullptr, std::vector<TreeNode *> ptr_list = nullptr,
             TreeNode *left_sibling = nullptr, TreeNode *right_sibling = nullptr) {
      value_list_ = value_list;
      ptr_list_ = ptr_list;
      parent_ = parent;
      left_sibling_ = left_sibling;
      right_sibling_ = right_sibling;
      size = 0;
      while (value_list != nullptr) {
        size++;
        value_list = value_list->next_;
      }
    }
    ~TreeNode() {
      InnerList *tmp_list;
      while (value_list_ != nullptr) {
        tmp_list = *value_list_;
        value_list_ = value_list_->next_;
        delete tmp_list;
      }
      for (int i = 0; i < ptr_list_.size(); i++) {
        delete ptr_list_[i];
      }
    }

    typedef struct SplitReturn {
      InnerList *split_value;
      TreeNode *parent;
      TreeNode *left_child;
      TreeNode *right_child;
    } SplitReturn;

    bool IsLeaf() { return ptr_list_.size() == 0; }
    bool ShouldSplit(size_t order) { return size > order; }
    /**
     * Recursively find the position to perform insert.
     * Terminate if reach the child node, insert into it
     * otherwise find the best child node and recursively perform insertion
     **/
    TreeNode *Insert(KeyType key, ValueType val, bool allow_dup = true) {
      TreeNode *result = nullptr;
      InnerList *new_value = new InnerList(key, val);
      if (IsLeaf()) {
        result = insertAtLeafNode(new_value, allow_dup);
      } else {
        TreeNode *child_node = findBestFitChild(new_value->key_);
        result = child_node->Insert(new_value, allow_dup);
      }
      return result;
    }

    // TODO: implement delete
    TreeNode *Delete(KeyType key, ValueType value) { return nullptr; }
    TreeNode *SplitWrapper(TreeNode *cur_node, TreeNode *root_node, size_t order) {
      std::vector<TreeNode::InnerList *> restore_stack;
      return Split(cur_node, root_node, order, restore_stack);
    }

    // going from leaf to root, recursively split child, insert a new node at parent
    // in case split fails at current level, return nullptr, restore  tree from failed node
    TreeNode *Split(TreeNode *cur_node, TreeNode *root_node, size_t order, std::vector<InnerList *> restore_stack,
                    InnerList *split_value_list = nullptr, TreeNode *left_child = nullptr,
                    TreeNode *right_child = nullptr) {
      restore_stack.push_back(split_value_list);
      // base case: root is split, create a new root and return it
      if (cur_node == nullptr) {
        TreeNode *new_root = new TreeNode(nullptr);
        // if the creation fails
//        if (new_root == nullptr) {
//          return this->RestoreTreeFromNode(left_child, right_child, restore_stack);
//        }
        new_root->ConfigureNewSplitNode(split_value_list, left_child, right_child);
        return new_root;
      }

      if (cur_node->IsLeaf()) {
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
        return this->RestoreTreeFromNode(left_child, right_child, restore_stack);
      }
      return Split(split_res->parent, root_node, order, restore_stack, split_res->split_value, split_res->left_child,
                   split_res->right_child);
    }

    TreeNode *GetNodeRecursive(TreeNode *node, KeyType index_key) {
      if (IsLeaf()) {
        return node;
      } else {
        TreeNode *child_node = findBestFitChild(index_key);
        GetNodeRecursive(child_node, index_key);
      }
    }

   private:
    InnerList *GetEndValue() {
      InnerList *cur = value_list_;
      InnerList *next = value_list_->next_;
      while (next != nullptr) {
        cur = next;
        next = next->next_;
      }
      return cur;
    }
    // assuming this is a leafNode
    TreeNode *insertAtLeafNode(InnerList *new_list, bool allow_dup = true) {
      TERRIER_ASSERT(this->IsLeaf(),
                     "insertAtLeafNode should be called from leaf node");
      KeyType key = new_list->key_;
      ValueType val = new_list->value_;
      InnerList *cur = value_list_;
      InnerList *next = nullptr;
      while (cur != nullptr) {
        // if contains key just append in the key linked list
        if (cur->key_ == key) {
          if (allow_dup || (!ContainDupValue(val))) {
            cur->InsertDup(new_list);
          } else {
            return nullptr;
          }
        } else {
          // else, find best fit point to insert a new node
          next = cur->next_;
          // case should advance
          if (next != nullptr && next->key_ <= key) {
            cur = cur->next_;
            continue;
          }else {
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
              size++;
              break;
            }
          }
        }
      }
      return this;
    }
    bool ContainDupValue(ValueType value) {
      for (int i = 0; i < this->same_key_values_.size(); i++) {
        if ((this->same_key_values_)[i] == value) return true;
      }
      return false;
    }
    /**
     * recursively find the best fit child tree node from here to leaf, return leaf
     **/
    TreeNode *findBestFitChild(KeyType key) {
      TERRIER_ASSERT(!this->IsLeaf(), "findBestFitChild should be called from non-leaf node");
      InnerList *cur_val = value_list_;
      InnerList *next_val;
      auto ptr_iter = ptr_list_.begin();  // left side of the ptr list
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
    TreeNode *RestoreTreeFromNode(TreeNode *left_node, TreeNode *right_node, std::vector<InnerList *> restore_stack) {
      // pop the restoring value
      InnerList *split_list = restore_stack.pop_back();

      // delete the split value in current node if there is any
      InnerList *cur_val = this->value_list_;
      while (cur_val != nullptr) {
        if (cur_val->key_ == split_list->key_) {
          TERRIER_ASSERT(cur_val == split_list,
                         "the value targeting to be removed should be the same as the one in the restore stack");
          cur_val->PopListHere();
          break;
        }
        cur_val = cur_val->next_;
      }
      // if not leaf node
      // delete the right node from ptr list if there is any
      if (!this->IsLeaf()) {
        TERRIER_ASSERT(this->ptr_list != nullptr, "When it is no leaf ptr_list should not be null");
        auto ptr_iter = this->ptr_list_.begin();
        while (ptr_iter != this->ptr_list_.end()) {
          if (*ptr_iter == right_node) {
            this->ptr_list_.erase(ptr_iter);
            break;
          }
          ++ptr_iter;
        }
      }

      // merge left node with right node:
      merge(left_node, right_node);
      // merge values together
      // if not leaf node, merge ptr list

      // recursively call left child to restore
      TreeNode child_left = nullptr;
      TreeNode child_right = nullptr;
      int index = 0;
      if (!left_node->IsLeaf()) {
        InnerList *child_split_list = restore_stack.back();
        while (left_node->value_list_[index]->key_ != child_split_list->key_) {
          index++;
        }
        child_left = left_node->ptr_list_[index];
        child_right = left_node->ptr_list_[index + 1];
      }
      left_node->RestoreTreeFromNode(child_left, child_right, restore_stack);
      // return restored top node which is this
      return this;
      ;
    }
    // merge right into left, delete right treenode
    void merge(TreeNode *left_node, TreeNode *right_node) {
      TERRIER_ASSERT(left_node->IsLeaf() == right_node->IsLeaf(),
                     "THe merging two nodes should be both leaf or both non-leaf");

      InnerList *left_end_value = left_node->GetEndValue();
      left_end_value->InsertBack(right_node->value_list_);
      right_node->value_list_ = nullptr;  // TODO: prevent deletion of values

      // if non-leaf, merge the ptr list
      if (!left_node->IsLeaf()) {
        left_node->ptr_list_.insert(left_node->ptr_list_.end(), right_node->ptr_list_.begin(),
                                     right_node->ptr_list_.end());
        // TODO: not sure should invalidate the right ptr list in order to prevent its inner element currently in left
        // gets invalidated
      }
      // if is leaf, merge the sibling
      else {
        left_node->right_sibling_ = right_node->right_sibling_;
      }
      delete right_node;
    }
    // configure a new node, insert split value, left child, right child
    // return the finished node
    void ConfigureNewSplitNode(InnerList *split_value_list, TreeNode *left_child, TreeNode *right_child) {
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
        auto ptr_list_iter = this->ptr_list_.begin(); // left side of the value
        // lterate untill theoriginal ptr position using left node as original node
        while ((*ptr_list_iter) != left_child) {
          cur_value = cur_value->next_;
          ++ptr_list_iter;
        }
        // insert right ptr at the right of current left ptr
        ++ptr_list_iter;
        // insert the new value into the value list
        // if at the end of the value list insert at the back of the value list
        if (cur_value == nullptr) {
          TERRIER_ASSERT(ptr_list_iter == this->ptr_list_.end(), "insert should at the ptr end when cur_value is null");
          InnerList *end = this->GetEndValue();
          end->InsertBack(split_value_list);
        }
        // if at the start of the value_list insert at the very front
        else if (cur_value->prev_ == nullptr) {
          TERRIER_ASSERT(ptr_list_iter == this->ptr_list_.begin(),
                         "insert should at the ptr start when cur_value->prev_ is null");
          TERRIER_ASSERT(cur_value == this->value_list_, "cur_value should also point to value_list_ start");
          this->value_list_->InsertFront(split_value_list);
          this->value_list_ = this->value_list_->prev_;
        }
        // if at the middle of the value_list, insert at the front
        // as the ptr_list is the left of the real value
        else {
          cur_value->InsertFront(split_value_list);
        }
        // insert the new right side pointer
        this->ptr_list_.insert(ptr_list_iter, right_child);
      }
      // increase the current node size as we insert a new value
      this->size++;
    }

    // assume the node exceeds capacity
    // split the current node into two for left and non-leaf
    SplitReturn *SplitNode(TreeNode *node) {
      SplitReturn result;
      size_t split_index = node->size / 2;
      size_t cur_index = 0;
      InnerList split_list = value_list_;
      // get the split_list location
      while (cur_index != split_index) {
        split_list = split_list->next_;
        cur_index++;
      }
      // create a new TreeNode that has the same parent
      TreeNode *right_tree_node = new TreeNode(node->parent_);
      TreeNode left_tree_node = node;
      // if (right_tree_node == nullptr) return nullptr;
      result.parent = left_tree_node->parent_;
      result.left_child = left_tree_node;
      result.right_child = right_tree_node;

      if (node->IsLeaf()) {
        InnerList *split_value = new InnerList(split_list);
        // if (split_value == nullptr) return nullptr;
        // configure value list, break the linkedlist into two
        right_tree_node->value_list_ = split_list;
        split_list->prev_->next_ = nullptr;
        split_list->prev_ = nullptr;
        // configure size
        right_tree_node->size = left_tree_node->size - cur_index;
        left_tree_node->size = cur_index;
        result.split_value = split_value;
        // configure sibling
        right_tree_node->right_sibling_ = left_tree_node->right_sibling_;
        left_tree_node->right_sibling_ = right_tree_node;
        right_tree_node->left_sibling_ = left_tree_node;
      } else {
        // if none leaf node
        // configure the value list pop the value out of the value list
        right_tree_node->value_list_ = split_list->next_;
        split_list->next_->prev_ = nullptr;
        split_list->prev_->next_ = nullptr;
        split_list->prev_ = nullptr;
        split_list->next_ = nullptr;
        // configure ptr_list
        // cur_index is the left ptr in the poped node
        // should remain at the left side, right node keeps everything from cur_index + 1
        size_t popping_index = left_tree_node->size;  // ptr lise has size + 1 ptrs
        InnerList *tmp_ptr;
        while (popping_index > cur_index) {
          tmp_ptr = left_tree_node->ptr_list_.back();
          left_tree_node->ptr_list_.pop_back();
          right_tree_node->ptr_list_.insert(right_tree_node->ptr_list_.begin(), tmp_ptr);
          popping_index--;
        }
        // configure size
        right_tree_node->size = left_tree_node->size - cur_index - 1;  // middle value is poped out
        left_tree_node->size = cur_index;
        result.split_value = split_list;
      }
      return &result;
    }
  };  // end TreeNode

  TreeNode *root;  // with parent node as empty for root
  size_t order_; // split when node > order_

  BPlusTree(size_t order) {
    order_ = order;
    root = new TreeNode(nullptr);
  }
  ~BPlusTree() { delete root; }

  bool Insert(KeyType key, ValueType value, bool allow_dup = true) {
    bool result = false;
    TreeNode *new_root = nullptr;
    root->Insert(key, value, allow_dup);
    new_root = RebalanceTree(result, key, value);
    if (new_root == nullptr) return false;
    root = new_root;
    return true;
  }
  bool InsertUnique(KeyType key, ValueType value) { return Insert(key, value, false); }
  bool Delete(KeyType key, ValueType value) { return true; }
  void GetValue(KeyType index_key, std::vector<ValueType> &results) {
    TreeNode *target_node = GetNodeRecursive(root, index_key);
    auto *cur = target_node->value_list_;
    while (cur != nullptr) {
      if (index_key == cur->key_) {
        results = cur->GetAllValues();
        break;
      }
      cur = cur->next_;
    }

  }



  void GetValueDescending(KeyType index_low_key, KeyType index_high_key, std::vector<ValueType> &results) {
    TreeNode *cur_node = GetNodeRecursive(root, index_high_key);
    while (cur_node != nullptr) {
      auto cur = cur_node->value_list_;
      while (cur != nullptr) {
        if (cur->key_ < index_low_key) return;
        if (cur->key_ <= index_high_key) {
          results.reserve(results.size() + cur->GetAllValues().size());
          results.insert(results.end(), cur->GetAllValues().begin(), cur->GetAllValues().end());
        }
        cur = cur->next_;
      }
      cur_node = cur_node->left_sibling_;
    }
  }

  void GetValueDescendingLimited(KeyType index_low_key, KeyType index_high_key, std::vector<ValueType> &results, uint32_t limit) {
    if (limit == 0) return;
    uint32_t count = 0;
    TreeNode *cur_node = GetNodeRecursive(root, index_high_key);
    while (cur_node != nullptr) {
      auto cur = cur_node->value_list_;
      while (cur != nullptr) {
        if (cur->key_ < index_low_key) return;
        if (cur->key_ <= index_high_key) {
          results.reserve(results.size() + cur->GetAllValues().size());
          results.insert(results.end(), cur->GetAllValues().begin(), cur->GetAllValues().end());
          ++count;
          if (count == limit) return;
        }
        cur = cur->next_;
      }
      cur_node = cur_node->left_sibling_;
    }
  }

 private:
  mutable common::SpinLatch latch_;
  TreeNode *RebalanceTree(TreeNode *leaf_node) {
    // wrapper for recursively rebalance the tree
    // if size exceeds, pop the middle element going from child to parent
    return SplitWrapper(leaf_node, root, order_);
  }
};
}  // namespace terrier::storage::index
#include <functional>
#include <queue>
#include <vector>
#include "storage/index/index.h"

namespace terrier::storage::index {

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class BPlusTree {
 public:
  class BaseOp {
   public:
    // Key comparator
    const KeyComparator key_cmp_obj_;
    // Raw key eq checker
    const KeyEqualityChecker key_eq_obj_;
    // Raw key hasher
    //    const KeyHashFunc key_hash_obj_;
    // Check whether values are equivalent
    const ValueEqualityChecker value_eq_obj_;

    bool KeyCmpLess(const KeyType &key1, const KeyType &key2) const { return key_cmp_obj_(key1, key2); }
    bool KeyCmpEqual(const KeyType &key1, const KeyType &key2) const { return key_eq_obj_(key1, key2); }
    //    bool KeyCmpGreaterEqual(const KeyType &key1, const KeyType &key2) const { return !KeyCmpLess(key1, key2); }
    bool KeyCmpGreater(const KeyType &key1, const KeyType &key2) const { return KeyCmpLess(key2, key1); }
    bool KeyCmpLessEqual(const KeyType &key1, const KeyType &key2) const { return !KeyCmpGreater(key1, key2); }
    //    size_t KeyHash(const KeyType &key) const { return key_hash_obj_(key); }
    bool ValueCmpEqual(const ValueType &val1, const ValueType &val2) const { return value_eq_obj_(val1, val2); }
  };
  class InnerList : public BaseOp {
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
      same_key_values_.push_back(val);
    }
    // copy constructor to construct a InnerList from reference
    explicit InnerList(InnerList *reference) {
      key_ = reference->key_;
      value_ = reference->value_;
      prev_ = nullptr;
      next_ = nullptr;
    }
    std::vector<ValueType> GetAllValues() { return same_key_values_; }

    // insert at the fron of the current value node
    void InsertFront(InnerList *new_value) {
      TERRIER_ASSERT(new_value != nullptr, "insertFront do not accept null InnerList to be inserted");
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
      TERRIER_ASSERT(new_value != nullptr, "insertBack do not accept null InnerList to be inserted");
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
      TERRIER_ASSERT(new_value != nullptr, "insertDup do not accept null InnerList to be inserted");
      TERRIER_ASSERT(this->KeyCmpEqual(new_value->key_, this->key_),
                     "insertDup should insert at Innerlist with same key as the new_value");
      same_key_values_.push_back((new_value->value_));
      // void the value_field to prevent deletion
      //      new_value->value_ = nullptr;
      delete new_value;
    }
    bool ContainDupValue(ValueType value) {
      for (size_t i = 0; i < this->same_key_values_.size(); i++) {
        if (this->ValueCmpEqual((this->same_key_values_)[i], value)) return true;
      }
      return false;
    }
    // remove all value frm current innerList
    // return false if current innerlist does not have this value
    InnerList *RemoveValue(ValueType value) {
      InnerList *res = nullptr;
      std::vector<ValueType>::iterator iter = same_key_values_.end();
      --iter;
      while (iter != same_key_values_.begin()) {
        if (*iter == value) {
          same_key_values_.erase(iter);
          res = this;
        }
        --iter;
      }
      if (*iter == value) {
        same_key_values.erase(iter);
        res = this;
      }
      return res;
    }

    bool IsEmpty() { return same_key_values_.size() == 0; }
  };  // end class InnerList
  class TreeNode : public BaseOp {
   public:
    size_t size_;
    InnerList *value_list_;             // list of value points, point at the start
    std::vector<TreeNode *> ptr_list_;  // list of pointers to the next treeNode, left node have size zero
    TreeNode *parent_;
    TreeNode *left_sibling_;
    TreeNode *right_sibling_;  // only leaf node has siblings
    explicit TreeNode(TreeNode *parent, InnerList *value_list = nullptr) {
      value_list_ = value_list;
      parent_ = parent;
      left_sibling_ = nullptr;
      right_sibling_ = nullptr;
      size_ = 0;
      while (value_list != nullptr) {
        size_++;
        value_list = value_list->next_;
      }
    }
    ~TreeNode() {
      InnerList *tmp_list;
      while (value_list_ != nullptr) {
        tmp_list = value_list_;
        value_list_ = value_list_->next_;
        delete tmp_list;
      }
      for (size_t i = 0; i < ptr_list_.size(); i++) {
        delete ptr_list_[i];
      }
    }

    using SplitReturn = struct SplitReturn {
      InnerList *split_value_;
      TreeNode *parent_;
      TreeNode *left_child_;
      TreeNode *right_child_;
      size_t parent_index_;
    };

    bool IsLeaf() { return ptr_list_.empty(); }
    bool ShouldSplit(size_t order) { return size_ > order; }
    /**
     * Recursively find the position to perform insert.
     * Terminate if reach the child node, insert into it
     * otherwise find the best child node and recursively perform insertion
     **/
    TreeNode *Insert(KeyType key, ValueType val, bool allow_dup = true) {
      TreeNode *result = nullptr;
      if (IsLeaf()) {
        auto *new_value = new InnerList(key, val);
        result = InsertAtLeafNode(new_value, allow_dup);
        if (result == nullptr) delete new_value;
      } else {
        TreeNode *child_node = FindBestFitChild(key);
        result = child_node->Insert(key, val, allow_dup);
      }
      return result;
    }

    TreeNode *SplitWrapper(TreeNode *cur_node, TreeNode *root_node, size_t order) {
      std::vector<InnerList *> restore_stack;
      return Split(cur_node, root_node, order, restore_stack);
    }

    // going from leaf to root, recursively split child, insert a new node at parent
    // in case split fails at current level, return nullptr, restore  tree from failed node
    TreeNode *Split(TreeNode *cur_node, TreeNode *root_node, size_t order, std::vector<InnerList *> restore_stack,
                    InnerList *split_value_list = nullptr, TreeNode *left_child = nullptr,
                    TreeNode *right_child = nullptr, size_t parent_index = 0) {
      restore_stack.push_back(split_value_list);
      // base case: root is split, create a new root and return it
      if (cur_node == nullptr) {
        auto *new_root = new TreeNode(nullptr);

        new_root->ConfigureNewSplitNode(split_value_list, left_child, right_child, parent_index);
        return new_root;
      }

      if (cur_node->IsLeaf()) {
        if (!cur_node->ShouldSplit(order)) {
          return root_node;
        }
      } else {
        // insert the new node in current
        cur_node->ConfigureNewSplitNode(split_value_list, left_child, right_child, parent_index);
      }
      // check if need to split
      if (!cur_node->ShouldSplit(order)) {
        return root_node;
      }
      // otherwise split the current node
      SplitReturn split_res = SplitNode(cur_node);

      return Split(split_res.parent_, root_node, order, restore_stack, split_res.split_value_, split_res.left_child_,
                   split_res.right_child_, split_res.parent_index_);
    }

    TreeNode *GetNodeRecursive(KeyType index_key) {
      if (IsLeaf()) {
        return node;
      }
      TreeNode *child_node = FindBestFitChild(index_key);
      return child_node->GetNodeRecursive(index_key);
    }

    // assume the node exceeds capacity
    // split the current node into two for left and non-leaf
    SplitReturn SplitNode(TreeNode *node) {
      SplitReturn result;
      size_t split_index = node->size_ / 2;
      size_t cur_index = 0;
      InnerList *split_list = node->value_list_;
      // get the split_list location
      while (cur_index != split_index) {
        split_list = split_list->next_;
        cur_index++;
      }
      // create a new TreeNode that has the same parent
      auto *right_tree_node = new TreeNode(node->parent_);
      TreeNode *left_tree_node = node;
      // if (right_tree_node == nullptr) return nullptr;
      result.parent_ = left_tree_node->parent_;
      result.left_child_ = left_tree_node;
      result.right_child_ = right_tree_node;
      result.parent_index_ = 0;
      if (node->parent_ != nullptr) {
        size_t index = 0;
        TreeNode *pi_node = node->parent_->ptr_list_[index];
        while (pi_node != node) {
          index++;
          pi_node = node->parent_->ptr_list_[index];
        }
        result.parent_index_ = index;
      }

      if (node->IsLeaf()) {
        auto *split_value = new InnerList(split_list->key_, split_list->value_);
        // if (split_value == nullptr) return nullptr;
        // configure value list, break the linkedlist into two
        right_tree_node->value_list_ = split_list;
        split_list->prev_->next_ = nullptr;
        split_list->prev_ = nullptr;
        // configure size
        right_tree_node->size_ = left_tree_node->size_ - cur_index;
        left_tree_node->size_ = cur_index;
        result.split_value_ = split_value;
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
        size_t popping_index = left_tree_node->size_;  // ptr lise has size + 1 ptrs
        TreeNode *tmp_ptr;
        while (popping_index > cur_index) {
          tmp_ptr = left_tree_node->ptr_list_.back();
          left_tree_node->ptr_list_.pop_back();
          right_tree_node->ptr_list_.insert(right_tree_node->ptr_list_.begin(), tmp_ptr);
          tmp_ptr->parent_ = right_tree_node;
          popping_index--;
        }
        // configure size
        right_tree_node->size_ = left_tree_node->size_ - cur_index - 1;  // middle value is poped out
        left_tree_node->size_ = cur_index;
        result.split_value_ = split_list;
      }
      return result;
    }
    // remove a key value pair from leaf
    // return nullptr if current value or key does not exist in leaf
    TreeNode *DeleteValueFromLeaf(KeyType key, ValueType value) {
      // find the proper value list
      TERRIER_ASSERT(this->IsLeaf(), "DeleteValueFromLeaf should be called from leaf node");
      InnerList *cur_value = value_list_;
      while (cur_value ! + nullptr && cur_value->key_ != key) {
        cur_value = cur_value->next_;
      }
      if (cur_value == nullptr) return nullptr;
      InnerList *remove_res = cur_value->RemoveValue(value);
      // fail to delete any
      if (remove_res == nullptr) return nullptr;
      // check if the innerlist needs to be remove
      if (remove_res->IsEmpty()) {
        if (remove_res == value_list_) {
          value_list_ = value_list->next_;
          if (value_list_ != nullptr) value_list_->prev_ = nullptr;
        } else {
          remove_res->prev_->next_ = remove_res->next_;
          remove_res->next_->prev_ = remove_res->prev_;
        }
        delete remove_res;
      }
      return this;
    }

    bool CurNodeValid(TreeNode *cur_node) { return cur_node->value_list_ != nullptr; }

    TreeNode *MergeFromLeaf(TreeNode *leaf_node, TreeNode *root, KeyType key) {
      // check if leaf id empty after deletion, if yes then needs to merge
      if (CurNodeValid(leaf_node)) return root;
      TreeNode *parent, *cur_node, *left_child, *right_child, *merged_node;
      InnerList *separation_value;
      size_t separation_index = 1;  // index of right hand side
      cur_node = leaf_node;
      while (!CurNodeValid(cur_node)) {
        if (cur_node == root) {
          TERRIER_ASSERT(root->size_ == 0 && root->ptr_list_.size() == 1,
                         "if roots needs to tear down to child, its ptr side should be 1");
          merged_node = root;
          root = root->ptr_list_[0];
          delete merged_node;

        } else {
          // find parent
          parent = cur_node->parent_;
          // find its left child ,if not find its right child
          // find parent's value that separatesthe two children
          // find parent's curresponding pointers two the two children
          if (cur_node == parent->ptr_list_[0]) {
            left_child = cur_node;
            right_child = ptr_list[1];
            separation_value = parent->value_list_;
          } else {
            separation_value = parent->value_list_;
            while (parent->ptr_list_[separation_index] != cur_node) {
              separation_value = separation_value->next_;
              separation_index++;
            }
            TERRIER_ASSERT(separation_index <= parent->size_, "Finding separation index goes beyond parent size");
            left_child = parent->ptr_list_[separation_index - 1];
            right_child = cur_node
          }
          // merge the two children as a new one
          Merge(left_child, right_child);
          merged_node = left_child;
          // delete the separation value in parent set to point to new children
          parent->ptr_list_.erase(parent->ptr_list_.begin() + separation_index);
          if (separation_value == parent->value_list_) {
            parent->value_list_ = parent->value_list_->next_;
            if (parent->value_list_ != nullptr) parent->value_list_->prev_ = nullptr;
          } else {
            separation_value->prev_->next_ = separation_value->next_;
            if (separation_value->next_ != nullptr) separation_value->next_->prev_ = separation_value->prev_;
          }
          delete separation_value;
          cur_node = parent;
        }
      }
      return root;
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
    TreeNode *InsertAtLeafNode(InnerList *new_list, bool allow_dup = true) {
      TERRIER_ASSERT(this->IsLeaf(), "insertAtLeafNode should be called from leaf node");
      if (this->size_ == 0) {
        this->value_list_ = new_list;
        this->size_++;
        return this;
      }
      KeyType key = new_list->key_;
      ValueType val = new_list->value_;
      InnerList *cur = value_list_;
      InnerList *next = nullptr;
      while (cur != nullptr) {
        // if contains key just append in the key linked list
        if (cur->KeyCmpEqual(cur->key_, key)) {
          if (allow_dup || (!cur->ContainDupValue(val))) {
            cur->InsertDup(new_list);
            return this;
          }
          return nullptr;
        }
        // else, find best fit point to insert a new node
        next = cur->next_;
        // case should advance
        if (next != nullptr && this->KeyCmpLessEqual(next->key_, key)) {
          cur = cur->next_;
          continue;
        }
        // if should insert at the front
        if (cur->KeyCmpGreater(cur->key_, key)) {
          cur->InsertFront(new_list);
          if (cur == value_list_) {
            value_list_ = new_list;
          }
          size_++;
          break;
        }
        // if place between cur and next
        cur->InsertBack(new_list);
        size_++;
        break;
      }
      return this;
    }

    /**
     * recursively find the best fit child tree node from here to leaf, return leaf
     **/
    TreeNode *FindBestFitChild(KeyType key) {
      TERRIER_ASSERT(!this->IsLeaf(), "findBestFitChild should be called from non-leaf node");
      InnerList *cur_val = value_list_;
      InnerList *next_val;
      auto ptr_iter = ptr_list_.begin();  // left side of the ptr list
      while (cur_val != nullptr) {
        if (cur_val->KeyCmpGreater(cur_val->key_, key)) {
          return *ptr_iter;
        }
        next_val = cur_val->next_;
        ++ptr_iter;
        if (next_val == nullptr) return *ptr_iter;
        if (next_val->KeyCmpGreater(next_val->key_, key)) return *ptr_iter;
        cur_val = next_val;
      }
      return *ptr_iter;
    }
    // merge right into left, delete right treenode
    void Merge(TreeNode *left_node, TreeNode *right_node) {
      TERRIER_ASSERT(left_node->IsLeaf() == right_node->IsLeaf(),
                     "THe merging two nodes should be both leaf or both non-leaf");

      InnerList *left_end_value = left_node->GetEndValue();
      left_end_value->InsertBack(right_node->value_list_);
      right_node->value_list_ = nullptr;

      // if non-leaf, merge the ptr list
      if (!left_node->IsLeaf()) {
        left_node->ptr_list_.insert(left_node->ptr_list_.end(), right_node->ptr_list_.begin(),
                                    right_node->ptr_list_.end());
      } else {
        // if is leaf, merge the sibling
        left_node->right_sibling_ = right_node->right_sibling_;
      }
      delete right_node;
    }
    // configure a new node, insert split value, left child, right child
    // return the finished node
    void ConfigureNewSplitNode(InnerList *split_value_list, TreeNode *left_child, TreeNode *right_child,
                               size_t ref_index) {
      // case 1, the node to config is an empty node
      if (this->size_ == 0) {
        this->value_list_ = split_value_list;
        this->ptr_list_.push_back(left_child);
        this->ptr_list_.push_back(right_child);

      } else {
        // find a position to insert value into
        InnerList *cur_value = value_list_;
        auto ptr_list_iter = ptr_list_.begin();
        // lterate untill theoriginal ptr position using left node as original node
        size_t ptr_index = 0;
        while (ptr_index != ref_index) {
          cur_value = cur_value->next_;
          ++ptr_list_iter;
          ptr_index++;
        }
        // insert right ptr at the right of current left ptr
        ++ptr_list_iter;
        // insert the new value into the value list
        // if at the end of the value list insert at the back of the value list
        if (cur_value == nullptr) {
          TERRIER_ASSERT(ptr_list_iter == this->ptr_list_.end(), "insert should at the ptr end when cur_value is null");
          InnerList *end = this->GetEndValue();
          end->InsertBack(split_value_list);
        } else if (cur_value->prev_ == nullptr) {
          // if at the start of the value_list insert at the very front
          TERRIER_ASSERT(cur_value == this->value_list_, "cur_value should also point to value_list_ start");
          if (this->KeyCmpGreater(cur_value->key_, split_value_list->key_)) {
            this->value_list_->InsertFront(split_value_list);
            this->value_list_ = this->value_list_->prev_;
          } else {
            this->value_list_->InsertBack(split_value_list);
          }
        } else {
          // if at the middle of the value_list, insert at the front
          // as the ptr_list is the left of the real value
          if (this->KeyCmpGreater(cur_value->key_, split_value_list->key_)) {
            cur_value->InsertFront(split_value_list);
          } else {
            cur_value->InsertBack(split_value_list);
          }
        }
        // insert the new right side pointer
        this->ptr_list_.insert(ptr_list_iter, right_child);
      }
      // increase the current node size as we insert a new value
      this->size_++;
      left_child->parent_ = this;
      right_child->parent_ = this;
    }
  };  // end TreeNode

  TreeNode *root_;  // with parent node as empty for root
  size_t order_;    // split when node > order_

  explicit BPlusTree(size_t order = 2) {
    order_ = order;
    root_ = new TreeNode(nullptr);
  }
  ~BPlusTree() { delete root_; }

  bool Insert(KeyType key, ValueType value, bool allow_dup = true) {
    common::SpinLatch::ScopedSpinLatch guard(&latch_);
    TreeNode *new_root = nullptr;
    TreeNode *leaf_node = root_->Insert(key, value, allow_dup);
    if (leaf_node == nullptr) return false;
    new_root = RebalanceTree(leaf_node);
    if (new_root == nullptr) return false;
    root_ = new_root;
    return true;
  }
  bool InsertUnique(KeyType key, ValueType value) { return Insert(key, value, false); }

  void Delete(KeyType key, ValueType value) {
    common::SpinLatch::ScopedSpinLatch guard(&latch_);
    TreeNode *leaf_node = root_->GetNodeRecursive(key);
    leaf_node->DeleteValueFromLeaf(key, value);
    TreeNode *new_root = MergeFromLeaf(leaf_node, key);
    root_ = new_root;
    return true;
  }
  void GetValue(KeyType index_key, std::vector<ValueType> *results) {
    common::SpinLatch::ScopedSpinLatch guard(&latch_);
    TreeNode *target_node = root_->GetNodeRecursive(index_key);
    auto *cur = target_node->value_list_;
    while (cur != nullptr) {
      if (cur->KeyCmpEqual(index_key, cur->key_)) {
        *results = cur->GetAllValues();
        break;
      }
      cur = cur->next_;
    }
  }

  void GetValueDescending(KeyType index_low_key, KeyType index_high_key, std::vector<ValueType> *results) {
    common::SpinLatch::ScopedSpinLatch guard(&latch_);
    TreeNode *cur_node = root_->GetNodeRecursive(index_high_key);
    while (cur_node != nullptr) {
      auto cur = cur_node->value_list_;
      while (cur != nullptr) {
        if (cur->KeyCmpLess(cur->key_, index_low_key)) return;
        if (cur->KeyCmpLessEqual(cur->key_, index_high_key)) {
          (*results).reserve((*results).size() + cur->GetAllValues().size());
          (*results).insert((*results).end(), cur->GetAllValues().begin(), cur->GetAllValues().end());
        }
        cur = cur->next_;
      }
      cur_node = cur_node->left_sibling_;
    }
  }

  void GetValueDescendingLimited(KeyType index_low_key, KeyType index_high_key, std::vector<ValueType> *results,
                                 uint32_t limit) {
    common::SpinLatch::ScopedSpinLatch guard(&latch_);
    if (limit == 0) return;
    uint32_t count = 0;
    TreeNode *cur_node = root_->GetNodeRecursive(index_high_key);
    while (cur_node != nullptr) {
      auto cur = cur_node->value_list_;
      while (cur != nullptr) {
        if (cur->KeyCmpLess(cur->key_, index_low_key)) return;
        if (cur->KeyCmpLessEqual(cur->key_, index_high_key)) {
          (*results).reserve((*results).size() + cur->GetAllValues().size());
          (*results).insert((*results).end(), cur->GetAllValues().begin(), cur->GetAllValues().end());
          ++count;
          if (count == limit) return;
        }
        cur = cur->next_;
      }
      cur_node = cur_node->left_sibling_;
    }
  }

  size_t GetHeapUsage() const {
    size_t total_usage = 0;
    if (root_ == nullptr) return 0;
    std::queue<TreeNode *> q;
    q.push(root_);
    while (!q.empty()) {
      TreeNode *curr = q.front();
      q.pop();
      total_usage += GetNodeHeapUsage(curr);
      for (size_t i = 0; i < curr->ptr_list_.size(); i++) q.push(curr->ptr_list_[i]);
    }
    return total_usage;
  }

  size_t GetNodeHeapUsage(TreeNode *node) const {
    size_t count = 0;
    if (node == nullptr) return 0;
    // count heap usage for current node
    // TreeNode heap usage:
    //        size_t size;
    //        InnerList *value_list_;              // list of value points, point at the start
    //        std::vector<TreeNode *> *ptr_list_;  // list of pointers to the next treeNode, left node have size zero
    //        TreeNode *parent_;
    //        TreeNode *left_sibling_;
    //        TreeNode *right_sibling_;

    count += sizeof(size_t) + sizeof(TreeNode *) * (3 + node->ptr_list_.size()) + sizeof(InnerList *);

    InnerList *curr = node->value_list_;
    while (curr != nullptr) {
      // InnerList heap usage:
      //            KeyType key_;
      //            ValueType value_;
      //            InnerList *prev_;
      //            InnerList *next_;
      //            std::vector<ValueType> *same_key_values_;
      count += sizeof(KeyType) + sizeof(ValueType) * (1 + curr->same_key_values_.size()) + sizeof(InnerList *) * 2;
      curr = curr->next_;
    }
    return count;
  }

 private:
  mutable common::SpinLatch latch_;
  TreeNode *RebalanceTree(TreeNode *leaf_node) {
    // wrapper for recursively rebalance the tree
    // if size exceeds, pop the middle element going from child to parent
    return root_->SplitWrapper(leaf_node, root_, order_);
  }
};
}  // namespace terrier::storage::index
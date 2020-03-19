#include <functional>
#include <queue>
#include <vector>
#include "storage/index/index.h"

namespace terrier::storage::index {

/**
 * Base data structure of index, a standard bplus tree
 */
template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class BPlusTree {
 public:
  /**
   * Base operations of the class
   */
  class BaseOp {
   public:
    /// Key comparator
    KeyComparator key_cmp_obj_;
    /// Raw key eq checker
    KeyEqualityChecker key_eq_obj_;
    /// Check whether values are equivalent
    ValueEqualityChecker value_eq_obj_;
    /// operation <
    bool KeyCmpLess(const KeyType &key1, const KeyType &key2) const { return key_cmp_obj_(key1, key2); }
    /// operation ==
    bool KeyCmpEqual(const KeyType &key1, const KeyType &key2) const { return key_eq_obj_(key1, key2); }
    /// operation >
    bool KeyCmpGreater(const KeyType &key1, const KeyType &key2) const { return KeyCmpLess(key2, key1); }
    /// operation <=
    bool KeyCmpLessEqual(const KeyType &key1, const KeyType &key2) const { return !KeyCmpGreater(key1, key2); }
    /// operation == for value
    bool ValueCmpEqual(const ValueType &val1, const ValueType &val2) const { return value_eq_obj_(val1, val2); }
  };
  /**
   * InnerList contains key and corresponding values. It also has pointers to the prev and next one.
   */
  class InnerList : public BaseOp {
   public:
    /// key
    KeyType key_;
    /// value
    ValueType value_;
    /// prev pointer
    InnerList *prev_;
    /// next pointer
    InnerList *next_;
    /// underlying vector
    std::vector<ValueType> same_key_values_;
    /**
     * standard constructor using key and value
     * @param key key
     * @param val value
     * @param prev prev pointer
     * @param next next pointer
     */
    InnerList(KeyType key, ValueType val, InnerList *prev = nullptr, InnerList *next = nullptr) {
      key_ = key;
      value_ = val;
      prev_ = prev;
      next_ = next;
      // push the first value into the same_key_values_ as well
      same_key_values_.push_back(val);
    }
    /**
     * copy constructor to construct a InnerList from reference
     * @param reference reference of inner list
     */
    explicit InnerList(InnerList *reference) {
      key_ = reference->key_;
      value_ = reference->value_;
      prev_ = nullptr;
      next_ = nullptr;
    }
    /// get underlying vector
    std::vector<ValueType> GetAllValues() { return same_key_values_; }

    /**
     * insert at the front of the current value node
     * @param new_value value
     */
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

    /**
     * Insert at the back of list
     * @param new_value value
     */
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

    /**
     * append a new value at the end
     * @param new_value value
     */
    void AppendEnd(InnerList *new_value) {
      TERRIER_ASSERT(new_value != nullptr, "AppendEnd do not accept null InnerList to be inserted");
      InnerList *cur_value = this;
      cur_value->next_ = new_value;
      new_value->prev_ = cur_value;
    }

    /**
     * Insert to dup keys
     * @param new_value value
     */
    void InsertDup(InnerList *new_value) {
      TERRIER_ASSERT(new_value != nullptr, "insertDup do not accept null InnerList to be inserted");
      TERRIER_ASSERT(this->KeyCmpEqual(new_value->key_, this->key_),
                     "insertDup should insert at Innerlist with same key as the new_value");
      same_key_values_.push_back((new_value->value_));
      // void the value_field to prevent deletion
      //      new_value->value_ = nullptr;
      delete new_value;
    }
    /**
     * check if ContainDupValue
     * @param value value waited to be check
     * @return if dup is contained
     */
    bool ContainDupValue(ValueType value) {
      for (size_t i = 0; i < this->same_key_values_.size(); i++) {
        if (this->ValueCmpEqual((this->same_key_values_)[i], value)) return true;
      }
      return false;
    }
    /**
     * remove all values from current list
     * @param value value
     * @return false if failed
     */
    InnerList *RemoveValue(ValueType value) {
      InnerList *res = nullptr;
      auto iter = same_key_values_.end();
      bool deleted_one = false;
      --iter;
      while (iter != same_key_values_.begin()) {
        if (this->ValueCmpEqual(*iter, value)) {
          same_key_values_.erase(iter);
          res = this;
          deleted_one = true;
          break;
        }
        --iter;
      }
      if ((!deleted_one) && this->ValueCmpEqual(*iter, value)) {
        same_key_values_.erase(iter);
        res = this;
      }
      return res;
    }

    /**
     * check empty
     * @return if value list if empty
     */
    bool IsEmpty() { return same_key_values_.empty(); }

    /**
     * find list end
     * @return a pointer to the end of list
     */
    InnerList *FindListEnd() {
      InnerList *cur = this;
      InnerList *next = next_;
      while (next != nullptr) {
        cur = next;
        next = cur->next_;
      }
      return cur;
    }
  };  // end class InnerList
  /**
   * Bplus tree is composed of treenodes, which contains a parent, a value list and a pointer list if it is an insider,
   * or siblings if it is a leaf.
   */
  class TreeNode : public BaseOp {
   public:
    /// size
    size_t size_;
    /// list of value points, point at the start
    InnerList *value_list_;
    /// list of pointers to the next treeNode, left node have size zero
    std::vector<TreeNode *> ptr_list_;
    /// parent node
    TreeNode *parent_;
    /// left sibling
    TreeNode *left_sibling_;
    /// right sibling
    TreeNode *right_sibling_;  // only leaf node has siblings
    /// thread queue
    std::queue<size_t> thread_queue_;
    /// active reader
    size_t active_reader_;
    /// active writer
    size_t active_writer_;
    // spin latch
    mutable common::SpinLatch latch_;
    /**
     * Constructor
     * @param parent parent node
     * @param value_list value list
     */
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
    /**
     * destructor
     */
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
    /// return struct of split
    using SplitReturn = struct SplitReturn {
      /// split value
      InnerList *split_value_;
      /// parent
      TreeNode *parent_;
      /// left child
      TreeNode *left_child_;
      /// right child
      TreeNode *right_child_;
      /// parent index
      size_t parent_index_;
    };
    /**
     * Check if node is leaf
     * @return if node is leaf
     */
    bool IsLeaf() { return ptr_list_.empty(); }
    /**
     * Check if node should split
     * @return if node should split
     */
    bool ShouldSplit(size_t order) { return size_ > order; }

    /**
     * Acquire ID
     * @return write id
     */
    void PushWriteId(size_t cur_id) {
      latch_.Lock();
      thread_queue_.push(cur_id);
      latch_.Unlock();
    }

    /**
     * Writer loop
     * @param cur_id current id
     */
    void WriterWhileLoop(size_t cur_id) {
      while (true) {
        latch_.Lock();
        if (thread_queue_.front() == cur_id && active_reader_ == 0 && active_writer_ == 0) {
          thread_queue_.pop();
          active_writer_++;
          latch_.Unlock();
          break;
        }
        latch_.Unlock();
      }
    }

    /**
     * Reader loop
     * @param cur_id current id
     */
    void ReaderWhileLoop(size_t cur_id) {
      while (true) {
        latch_.Lock();
        if (thread_queue_.front() == cur_id && active_writer_ == 0) {
          thread_queue_.pop();
          active_reader_++;
          latch_.Unlock();
          break;
        }
        latch_.Unlock();
      }
    }

    /**
     * Writer Release lock
     */
    void WriterRelease() {
      latch_.Lock();
      active_writer_--;
      latch_.Unlock();
    }

    /**
     * Reader Release lock
     */
    void ReaderRelease() {
      latch_.Lock();
      active_reader_--;
      latch_.Unlock();
    }

    /**
     * Recursively find the position to perform insert.
     * Terminate if reach the child node, insert into it
     * otherwise find the best child node and recursively perform insertion
     * @param key key
     * @param val value
     * @param allow_dup if dups are allowed
     * @return inserted node
     */
    TreeNode *Insert(KeyType key, ValueType val, std::queue<common::SpinLatch *> *path_queue, bool allow_dup = true) {
      TreeNode *result = nullptr;
      TreeNode *leaf_node = FindBestFitChildForWrite(key, path_queue, 1);
      if (leaf_node == nullptr) return result;
      auto *new_value = new InnerList(key, val);
      result = leaf_node->InsertAtLeafNode(new_value, allow_dup);
      if (result == nullptr) delete new_value;
      return result;
    }

    /**
     * Wrapper function for split
     * @param cur_node current node
     * @param root_node root
     * @param order order
     * @return new node
     */
    TreeNode *SplitWrapper(TreeNode *cur_node, TreeNode *root_node, size_t order) {
      std::vector<InnerList *> restore_stack;
      return Split(cur_node, root_node, order, restore_stack);
    }

    /**
     * going from leaf to root, recursively split child, insert a new node at parent
     * in case split fails at current level, return nullptr, restore  tree from failed node
     * @param cur_node current node
     * @param root_node root
     * @param order order
     * @param restore_stack stack for restore purpose
     * @param split_value_list value list to split
     * @param left_child left child
     * @param right_child right child
     * @param parent_index parent index
     * @return new node
     */
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

    /**
     * Recursivly find a node
     * @param index_key index key to be found
     * @return target node
     */
    TreeNode *GetNodeRecursive(KeyType index_key) {
      if (IsLeaf()) {
        return this;
      }
      TreeNode *child_node = FindBestFitChild(index_key);
      return child_node->GetNodeRecursive(index_key);
    }

    /**
     * split the current node into two for left and non-leaf
     * @param node tree node to be splitted
     * @return SplitReturn struct
     */
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
    bool InsertOneStillValid(size_t order) { return this->size_ + 1 <= order; }
    /**
     * Check if node is valid
     * @param order order of a node
     * @return if node is valid
     */
    bool NodeValid(size_t order) { return this->size_ >= order / 2; }
    /**
     * Check if node is valid after removing one
     * @param order order of a node
     * @return if node is valid
     */
    bool RemoveOneStillValid(size_t order) { return this->size_ - 1 >= order / 2; }
    /**
     * Find the smallest key
     * @return the smallest key
     */
    KeyType FindSmallestKey() {
      if (IsLeaf()) return value_list_->key_;
      return ptr_list_[0]->FindSmallestKey();
    }

    /**
     * Remove value list from leaf
     * @param cur_value current value list
     */
    void RemoveValueListFromLeaf(InnerList *cur_value) {
      TERRIER_ASSERT(IsLeaf(), "RemoveValueListFromLeaf should be called from leaf node");
      if (cur_value->prev_ == nullptr)
        value_list_ = cur_value->next_;
      else
        cur_value->prev_->next_ = cur_value->next_;
      if (cur_value->next_ != nullptr) cur_value->next_->prev_ = cur_value->prev_;
      size_--;
      delete cur_value;
    }
    /**
     * Detach the given value list from parent, return it
     * @param separation_value the value to be separated
     */
    InnerList *DetachValueFromNode(InnerList *separation_value) {
      if (separation_value == value_list_) value_list_ = value_list_->next_;
      if (separation_value->prev_ != nullptr) separation_value->prev_->next_ = separation_value->next_;
      if (separation_value->next_ != nullptr) separation_value->next_->prev_ = separation_value->prev_;
      separation_value->prev_ = nullptr;
      separation_value->next_ = nullptr;
      return separation_value;
    }

    /**
     * Called from root to removed the key value pair from the leaf,
     * iterate backwards to maintain the tree invariance
     * @param leaf_node leaf node
     * @param key key
     * @param value value
     * @param order order
     * @return merged tree node
     */
    TreeNode *MergeFromLeaf(TreeNode *leaf_node, KeyType key, ValueType value, size_t order) {
      // delete value from leaf node
      // find the proper value list
      TERRIER_ASSERT(leaf_node->IsLeaf(), "MergeFromLeaf should be called from leaf node");
      InnerList *cur_value = leaf_node->value_list_;
      TreeNode *root = this;

      while (cur_value != nullptr && (!(cur_value->KeyCmpEqual(cur_value->key_, key)))) {
        cur_value = cur_value->next_;
      }
      // if the key ror value does not exists, return null as failed
      if (cur_value == nullptr) return nullptr;
      InnerList *removed_leaf = cur_value->RemoveValue(value);
      // fail to delete any value
      if (removed_leaf == nullptr) return nullptr;
      // check if the leaf innerlist needs to be remove
      if (removed_leaf->IsEmpty()) {
        TreeNode *cur_node = leaf_node;
        TreeNode *parent = nullptr;
        TreeNode *left_sib = nullptr;
        TreeNode *right_sib = nullptr;
        size_t right_ptr_index = 1, ptr_index, sib_index;  // index of right hand side
        InnerList *separation_value = nullptr;
        InnerList *left_sep_value = nullptr;
        InnerList *right_sep_value = nullptr;
        InnerList *borrowed_value = nullptr;
        KeyType tmp_key;
        while (cur_node != nullptr) {
          parent = cur_node->parent_;
          // find the node needed to be deleted, its parent,
          /////////////////////////// root case ////////////////////////////////////////
          if (cur_node == root) {
            // if root is empty, means that it children is merged and level shrinked
            if (cur_node->value_list_ == nullptr) {
              // if node is already leaf, just leave it like this
              // if root has children merged, shift root into its children and delete current root
              if (!cur_node->IsLeaf()) {
                root = cur_node->ptr_list_[0];
                cur_node->ptr_list_.pop_back();
                root->parent_ = nullptr;
                parent = nullptr;
                delete cur_node;
                cur_node = nullptr;
                break;
              }
            } else {
              // if root not empty, search if we still can find the value to delete
              cur_value = cur_node->value_list_;
              while (cur_value != nullptr) {
                // if find value to remove
                if (cur_value->KeyCmpEqual(cur_value->key_, key)) {
                  // if root is the leaf, then directly remove it
                  if (cur_node->IsLeaf()) {
                    cur_node->RemoveValueListFromLeaf(cur_value);
                  } else {
                    // if root has child, replace this value with the smallest key to its right
                    cur_value->key_ = cur_node->ptr_list_[right_ptr_index]->FindSmallestKey();
                  }
                  break;
                }
                cur_value = cur_value->next_;
                right_ptr_index++;
              }
            }
          } else {
            // end if cur_node == root
            // check to see if there is value needed to be removed
            right_ptr_index = 1;
            cur_value = cur_node->value_list_;
            while (cur_value != nullptr) {
              // if there is key needed to be removed
              if (cur_value->KeyCmpEqual(cur_value->key_, key)) {
                // if this is leaf, directly remove the value
                if (cur_node->IsLeaf()) {
                  cur_node->RemoveValueListFromLeaf(cur_value);
                } else {
                  // if non-leaf, replace it with the right hand side least key in leaf level
                  cur_value->key_ = cur_node->ptr_list_[right_ptr_index]->FindSmallestKey();
                }
                break;
              }
              cur_value = cur_value->next_;
              right_ptr_index++;
            }
            // after removal, check if current node needs to rebalance
            if (!cur_node->NodeValid(order)) {
              /*
              Figure out:
                left_sib: the left sibling of current node, null if cur_node is left most
                right_sib: the right sibling of current node, null if right most
                left_sep_value: parent separation value to the left of cur_node ptr, null if DCN
                right_sep_value: parent separation value to the right of the cur node ptr, null if DCN
                ptr_index: parent index in ptr list that points to cur_node
              */
              if (parent->ptr_list_[0] == cur_node) {
                left_sib = nullptr;
                right_sib = parent->ptr_list_[1];
                left_sep_value = nullptr;
                right_sep_value = parent->value_list_;
                ptr_index = 0;
              } else {
                left_sep_value = parent->value_list_;
                for (ptr_index = 1; ptr_index < parent->size_; ptr_index++) {
                  if (parent->ptr_list_[ptr_index] == cur_node) break;
                  left_sep_value = left_sep_value->next_;
                }
                right_sep_value = left_sep_value->next_;
                left_sib = parent->ptr_list_[ptr_index - 1];
                if (right_sep_value == nullptr)
                  right_sib = nullptr;
                else
                  right_sib = parent->ptr_list_[ptr_index + 1];
              }
              // try borrow from right sibling
              if (right_sib != nullptr && right_sib->RemoveOneStillValid(order)) {
                /*
                require : parent separation value
                          left most value from right sib
                          current right most value in current node
                if cur node is not leaf:
                          append parent separation value to back of the right most value in cur node
                          move left most value from right sib as new separation value at parent
                          detach the left most ptr list append it to the back of cur node ptr list
                if cur node is leaf: then parent separation value = left most value for right sib
                          detach the left most child, append to the cur_node back
                          update parent separation value as the new value list start on right sib
                */
                // non leaf case
                separation_value = right_sep_value;
                if (!cur_node->IsLeaf()) {
                  borrowed_value = right_sib->value_list_;
                  tmp_key = borrowed_value->key_;
                  borrowed_value->key_ = separation_value->key_;
                  right_sib->value_list_ = borrowed_value->next_;
                  right_sib->value_list_->prev_ = nullptr;
                  borrowed_value->next_ = nullptr;
                  if (cur_node->value_list_ == nullptr)
                    cur_node->value_list_ = borrowed_value;
                  else
                    cur_node->value_list_->FindListEnd()->InsertBack(borrowed_value);

                  separation_value->key_ = tmp_key;

                  cur_node->ptr_list_.push_back(right_sib->PopPtrListFront());
                } else {
                  borrowed_value = right_sib->value_list_;
                  right_sib->value_list_ = borrowed_value->next_;
                  right_sib->value_list_->prev_ = nullptr;
                  borrowed_value->next_ = nullptr;
                  if (cur_node->value_list_ == nullptr)
                    cur_node->value_list_ = borrowed_value;
                  else
                    cur_node->value_list_->FindListEnd()->InsertBack(borrowed_value);

                  separation_value->key_ = right_sib->FindSmallestKey();
                }
                right_sib->size_--;
                cur_node->size_++;
              } else if (left_sib != nullptr && left_sib->RemoveOneStillValid(order)) {
                /*
                require : parent separation value
                          right most value from left sib
                if cur node is not leaf:
                          append parent separation value to the front of cur node
                          move right most value from left sib as new separation value at parent
                          detach the right most ptr list append it to the front of cur node ptr list
                if cur node is leaf: then parent separation value = newly borrowed value from left
                          detach the left most child, append to the cur_node back
                          update parent separation value as the new value list start on right sib
                */
                // if not leaf
                separation_value = left_sep_value;
                if (!cur_node->IsLeaf()) {
                  borrowed_value = left_sib->value_list_->FindListEnd();
                  borrowed_value->prev_->next_ = nullptr;
                  borrowed_value->prev_ = nullptr;
                  tmp_key = borrowed_value->key_;
                  borrowed_value->key_ = separation_value->key_;
                  if (cur_node->value_list_ == nullptr) {
                    cur_node->value_list_ = borrowed_value;
                  } else {
                    cur_node->value_list_->InsertFront(borrowed_value);
                    cur_node->value_list_ = borrowed_value;
                    separation_value->key_ = cur_node->FindSmallestKey();
                  }
                  separation_value->key_ = tmp_key;
                  cur_node->InsertPtrFront(left_sib->ptr_list_.back());
                  left_sib->ptr_list_.pop_back();
                } else {
                  borrowed_value = left_sib->value_list_->FindListEnd();
                  borrowed_value->prev_->next_ = nullptr;
                  borrowed_value->prev_ = nullptr;
                  if (cur_node->value_list_ == nullptr) {
                    cur_node->value_list_ = borrowed_value;
                  } else {
                    cur_node->value_list_->InsertFront(borrowed_value);
                    cur_node->value_list_ = borrowed_value;
                    separation_value->key_ = cur_node->FindSmallestKey();
                  }
                }
                left_sib->size_--;
                cur_node->size_++;
              } else if (right_sib != nullptr) {
                // else try merge with right sibling if it is not the right most sibling
                /*
                require: right_sib,
                if cur_node is not leaf
                        detach the separation value from parent
                        delete the right ptr from, parent
                        merge the curnode, detached separation value, right_sib's value_list  from left to right
                        merge the ptr list from cur_node to the right sib
                if cur node is leaf
                        merge cur_node value_list, right sib value_list from left to right
                        delete the separation valie from parent
                        delete right sib ptr from parent
                */
                sib_index = ptr_index + 1;
                separation_value = right_sep_value;
                if (!cur_node->IsLeaf()) {
                  // detach
                  parent->DetachValueFromNode(separation_value);
                  parent->ptr_list_.erase(parent->ptr_list_.begin() + sib_index);
                  // use variable borrowed value as the innerlist performing insertion
                  if (cur_node->value_list_ == nullptr) {
                    cur_node->value_list_ = separation_value;
                  } else {
                    borrowed_value = cur_node->value_list_->FindListEnd();
                    borrowed_value->AppendEnd(separation_value);
                  }
                  borrowed_value = separation_value;
                  borrowed_value->AppendEnd(right_sib->value_list_);
                  // merge ptr list
                  while (!right_sib->ptr_list_.empty()) {
                    cur_node->ptr_list_.insert(cur_node->ptr_list_.begin() + cur_node->size_ + 1,
                                               right_sib->ptr_list_.back());
                    right_sib->ptr_list_.back()->parent_ = cur_node;
                    right_sib->ptr_list_.pop_back();
                  }

                  cur_node->size_ += (right_sib->size_ + 1);
                  parent->size_--;
                } else {
                  // merge value list
                  if (cur_node->value_list_ == nullptr)
                    cur_node->value_list_ = right_sib->value_list_;
                  else
                    cur_node->value_list_->FindListEnd()->AppendEnd(right_sib->value_list_);
                  // detach and delete the separation value from parent
                  parent->DetachValueFromNode(separation_value);
                  delete separation_value;
                  // delete right_sib ptr from parent
                  parent->ptr_list_.erase(parent->ptr_list_.begin() + sib_index);
                  // update cur_node right sib and right sib->right_sib->left_sib
                  cur_node->right_sibling_ = right_sib->right_sibling_;
                  if (right_sib->right_sibling_ != nullptr) right_sib->right_sibling_->left_sibling_ = cur_node;
                  // update size
                  parent->size_--;
                  cur_node->size_ += right_sib->size_;
                }
                right_sib->value_list_ = nullptr;  // set to nullptr to prevent value_list being deleted
                delete right_sib;
              } else {
                // else try merge with left sibling
                TERRIER_ASSERT(left_sib != nullptr, "left sibling should not be null otherwise tree not valid");
                /*
                require: left_sib,
                if cur_node is not leaf
                        detach the separation value from parent
                        delete the left ptr from, parent
                        merge the left value_list_, separation_value, cur_node value_list from left to right
                        set the cur_node value_list to the left value_list
                        merge the ptr list from leftNode ptr_list, cur_node ptr list from left to right
                if cur node is leaf
                        merge left_sib value_list, cur_node value_list from left to right
                        delete the separation valie from parent
                        delete left sib ptr from parent
                */
                sib_index = ptr_index - 1;
                separation_value = left_sep_value;
                if (!cur_node->IsLeaf()) {
                  // detach
                  parent->DetachValueFromNode(separation_value);
                  // delete
                  parent->ptr_list_.erase(parent->ptr_list_.begin() + sib_index);
                  // merge value list
                  left_sib->value_list_->FindListEnd()->AppendEnd(separation_value);
                  if (cur_node->value_list_ != nullptr) separation_value->AppendEnd(cur_node->value_list_);
                  cur_node->value_list_ = left_sib->value_list_;
                  while (!left_sib->ptr_list_.empty()) {
                    cur_node->ptr_list_.insert(cur_node->ptr_list_.begin(), left_sib->ptr_list_.back());
                    left_sib->ptr_list_.back()->parent_ = cur_node;
                    left_sib->ptr_list_.pop_back();
                  }

                  parent->size_--;
                  cur_node->size_ += (1 + left_sib->size_);
                } else {
                  // detach
                  parent->DetachValueFromNode(separation_value);
                  // delete
                  parent->ptr_list_.erase(parent->ptr_list_.begin() + sib_index);
                  delete separation_value;
                  if (cur_node->value_list_ != nullptr)
                    left_sib->value_list_->FindListEnd()->AppendEnd(cur_node->value_list_);
                  cur_node->value_list_ = left_sib->value_list_;

                  cur_node->left_sibling_ = left_sib->left_sibling_;
                  if (left_sib->left_sibling_ != nullptr) left_sib->left_sibling_->right_sibling_ = cur_node;
                  parent->size_--;
                  cur_node->size_ += (left_sib->size_);
                }
                left_sib->value_list_ = nullptr;
                delete left_sib;
              }
            }
          }                   // end else where cur node is not root
          cur_node = parent;  // proceed upward
        }                     // while cur node is not null
      }                       // if cur leaf val is empty
      return root;            // return the new root post merge
    }

    /**
     * recursively find the best fit child tree node from here to leaf, return leaf
     * @param key key
     */
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

    /**
     * Insert at the leaf
     * @param new_list value list
     * @param allow_dup if dups are allowed
     */
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

   private:
    /**
     * Get the last value
     * @return End value list
     */
    InnerList *GetEndValue() {
      InnerList *cur = value_list_;
      InnerList *next = value_list_->next_;
      while (next != nullptr) {
        cur = next;
        next = next->next_;
      }
      return cur;
    }
    /**
     * Pop front of ptr list
     * @return Front node of ptr list
     */
    TreeNode *PopPtrListFront() {
      TreeNode *result = *ptr_list_.begin();
      ptr_list_.erase(ptr_list_.begin());
      return result;
    }
    /**
     * Insert at the front of pointer list
     * @param node_ptr pointer to node
     */
    void InsertPtrFront(TreeNode *node_ptr) { ptr_list_.insert(ptr_list_.begin(), node_ptr); }

    /*
      mode:
          1: insert
          0: delete

    */
    // TreeNode *FindBestFitChildForWrite(KeyType key, std::queue<common::SpinLatch *> *path_queue, size_t mode) {
    //   InnerList *cur_val = value_list_;
    //   InnerList *next_val;
    //   /*********  Write latch stepwise operation ***********/
    //   path_queue->push(&write_latch_);
    //   write_latch_.Lock();
    //   if (mode == 1 && this->InsertOneStillValid()) {
    //     while (path_queue->size() > 1) {
    //       path_queue->front()->Unlock();
    //       path_queue->pop();
    //     }
    //   } else if (mode == 0 && (!this->IsLeaf) && (!(this->ptr_list_[0]->IsLeaf)) &&
    //              (this->RemoveOneStillValid())) { /* conditionals:
    //                                                 mode is deletion
    //                                                 this is not a leaf node
    //                                                 this is not a parent of a leaf node
    //                                                 this can satisfy delete one still valid
    //                                               */
    //     // delete has two cases, if the current non-leaf node's children are leaf, then it lock itself
    //     // otherwise perform the same
    //     while (path_queue->size() > 1) {
    //       path_queue->front()->Unlock();
    //       path_queue->pop();
    //     }
    //   }

    //   /*********  Write latch stepwise operation ***********/
    //   if (IsLeaf()) return this;
    //   auto ptr_iter = ptr_list_.begin();  // left side of the ptr list
    //   while (cur_val != nullptr) {
    //     if (cur_val->KeyCmpGreater(cur_val->key_, key)) {
    //       return (*ptr_iter)->FindBestFitChildForWrite(key, path_queue, mode);
    //     }
    //     next_val = cur_val->next_;
    //     ++ptr_iter;
    //     if (next_val == nullptr) return (*ptr_iter)->FindBestFitChildForWrite(key, path_queue, mode);
    //     if (next_val->KeyCmpGreater(next_val->key_, key))
    //       return (*ptr_iter)->FindBestFitChildForWrite(key, path_queue, mode);
    //     cur_val = next_val;
    //   }
    //   return (*ptr_iter)->FindBestFitChildForWrite(key, path_queue, mode);
    // }

    /**
     * merge two node
     * @param left_node left child
     * @param right_node right child
     */
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
    /**
     * configure a new node, insert split value, left child, right child
     * @param split_value_list value list to be splitted
     * @param left_child left child
     * @param right_child right child
     * @param ref_index reference index
     */
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
      if (left_child != nullptr) left_child->parent_ = this;
      if (right_child != nullptr) right_child->parent_ = this;
    }
  };  // end TreeNode

  /// thread queue
  std::queue<size_t> thread_queue_;
  /// active reader
  size_t active_reader_;
  /// active writer
  size_t active_writer_;
  /// current id
  size_t cur_id_;
  /// with parent node as empty for root
  TreeNode *root_;
  /// split when node > order_
  size_t order_;

  /**
   * Constructor
   * @param order
   */
  explicit BPlusTree(size_t order = 2) {
    order_ = order;
    cur_id_ = 0;
    active_reader_ = 0;
    active_writer_ = 0;
    root_ = new TreeNode(nullptr);
  }
  /**
   * destructor
   */
  ~BPlusTree() { delete root_; }

  /**
   * Acquire ID
   * @return write id
   */
  size_t AcquireWriteId() {
    size_t cur_id = 0;
    latch_.Lock();
    cur_id = cur_id_;
    cur_id_++;
    thread_queue_.push(cur_id);
    latch_.Unlock();
    return cur_id;
  }

  /**
   * Writer loop
   * @param cur_id current id
   */
  void WriterWhileLoop(size_t cur_id) {
    while (true) {
      latch_.Lock();
      if (thread_queue_.front() == cur_id && active_reader_ == 0 && active_writer_ == 0) {
        thread_queue_.pop();
        active_writer_++;
        latch_.Unlock();
        break;
      }
      latch_.Unlock();
    }
  }

  /**
   * Reader loop
   * @param cur_id current id
   */
  void ReaderWhileLoop(size_t cur_id) {
    while (true) {
      latch_.Lock();
      if (thread_queue_.front() == cur_id && active_writer_ == 0) {
        thread_queue_.pop();
        active_reader_++;
        latch_.Unlock();
        break;
      }
      latch_.Unlock();
    }
  }

  /**
   * Writer Release lock
   */
  void WriterRelease() {
    latch_.Lock();
    active_writer_--;
    latch_.Unlock();
  }

  /**
   * Reader Release lock
   */
  void ReaderRelease() {
    latch_.Lock();
    active_reader_--;
    latch_.Unlock();
  }

  /// return struct of of the encapsulation for both tree and node
  using TreeNodeUnion = struct TreeNodeUnion {
    union {
      TreeNode *tree_node_;
      BPlusTree *tree_;
    };
    bool is_tree_;
  };

  void UnlockQueue(std::queue<TreeNodeUnion *> *path_queue, bool is_read) {
    TreeNodeUnion *t_union;
    while (path_queue->size() != 0) {
      t_union = path_queue->front();
      if (t_union->is_tree_) {
        if (is_read)
          t_union->tree_->ReaderRelease();
        else
          t_union->tree_->WriterRelease();
      } else {
        if (is_read)
          t_union->tree_node_->ReaderRelease();
        else
          t_union->tree_node_->WriterRelease();
      }
      path_queue->pop();
      delete t_union;
    }
    delete path_queue;
  }

  void UnlockQueueTillNow(std::queue<TreeNodeUnion *> *path_queue, TreeNode *cur_node, bool is_read) {
    TreeNodeUnion *t_union;
    while (path_queue->size() != 0) {
      t_union = path_queue->front();
      if (t_union->is_tree_) {
        if (is_read)
          t_union->tree_->ReaderRelease();
        else
          t_union->tree_->WriterRelease();
      } else {
        if (t_union->tree_node_ == cur_node) return;
        if (is_read)
          t_union->tree_node_->ReaderRelease();
        else
          t_union->tree_node_->WriterRelease();
      }
      path_queue->pop();
      delete t_union;
    }
  }

  /**
   * Insert
   * @param key insert key
   * @param value insert value
   * @param allow_dup if dup is allowed
   * @return if the insertion succeeds
   */
  bool Insert(KeyType key, ValueType value, bool allow_dup = true) {
    TreeNode *new_root = nullptr;
    std::queue<TreeNodeUnion *> *path_queue = new std::queue<TreeNodeUnion *>();
    TreeNodeUnion *t_union;
    /******************* Concurrent node **********************/
    size_t cur_id = AcquireWriteId();
    t_union = new TreeNodeUnion();
    t_union->is_tree_ = true;
    t_union->tree_ = this;
    path_queue->push(t_union);
    /******************* Concurrent node **********************/
    TreeNode *result = nullptr;
    TreeNode *cur_node = root_;

    while (true) {
      cur_node->PushWriteId(cur_id);
      cur_node->WriterWhileLoop(cur_id);
      t_union = new TreeNodeUnion();
      t_union->tree_node_ = cur_node;
      t_union->is_tree_ = false;
      path_queue->push(t_union);
      if (cur_node->InsertOneStillValid(order_)) UnlockQueueTillNow(path_queue, cur_node, false);
      if (cur_node->IsLeaf()) {
        auto new_value = new InnerList(key, value);
        result = cur_node->InsertAtLeafNode(new_value, true);
        if (result == nullptr) delete new_value;
        break;
      } else {
        cur_node = cur_node->FindBestFitChild(key);
      }
    }

    if (result == nullptr) {
      UnlockQueue(path_queue, false);
      return false;
    }
    new_root = RebalanceTree(result);
    if (new_root == nullptr) {
      UnlockQueue(path_queue, false);
      return false;
    }
    root_ = new_root;
    UnlockQueue(path_queue, false);
    return true;
  }

  /**
   * InsertUnique
   * @param key insert key
   * @param value insert value
   * @param predicate predicate check
   * @param predicate_satisfied result of predicate check
   * @return if the insertion succeeds
   */
  bool InsertUnique(KeyType key, ValueType value, std::function<bool(const ValueType)> predicate,
                    bool *predicate_satisfied) {
    TreeNode *new_root = nullptr;
    std::queue<TreeNodeUnion *> *path_queue = new std::queue<TreeNodeUnion *>();
    TreeNodeUnion *t_union;
    /******************* Concurrent node **********************/
    size_t cur_id = AcquireWriteId();
    t_union = new TreeNodeUnion();
    t_union->is_tree_ = true;
    t_union->tree_ = this;
    path_queue->push(t_union);
    /******************* Concurrent node **********************/
    TreeNode *result = nullptr;
    TreeNode *cur_node = root_;

    while (true) {
      cur_node->PushWriteId(cur_id);
      cur_node->WriterWhileLoop(cur_id);
      t_union = new TreeNodeUnion();
      t_union->tree_node_ = cur_node;
      t_union->is_tree_ = false;
      path_queue->push(t_union);
      if (cur_node->InsertOneStillValid(order_)) UnlockQueueTillNow(path_queue, cur_node, false);

      if (cur_node->IsLeaf()) {
        std::vector<ValueType> *value_list = nullptr;
        auto *cur = cur_node->value_list_;
        while (cur != nullptr) {
          if (cur->KeyCmpEqual(key, cur->key_)) {
            *value_list = cur->GetAllValues();
            break;
          }
          cur = cur->next_;
        }
        if (cur == nullptr) {
          UnlockQueue(path_queue, false);
          return false;
        }
        for (auto &val : *value_list) {
          if (predicate(val)) {
            *predicate_satisfied = true;
            UnlockQueue(path_queue, false);
            return false;
          }
          if (this->root_->ValueCmpEqual(val, value)) {
            UnlockQueue(path_queue, false);
            return false;
          }
        }
        auto new_value = new InnerList(key, value);
        result = cur_node->InsertAtLeafNode(new_value, false);
        if (result == nullptr) delete new_value;
        break;
      } else {
        cur_node = cur_node->FindBestFitChild(key);
      }
    }

    if (result == nullptr) {
      UnlockQueue(path_queue, false);
      return false;
    }
    new_root = RebalanceTree(result);
    if (new_root == nullptr) {
      UnlockQueue(path_queue, false);
      return false;
    }
    root_ = new_root;
    UnlockQueue(path_queue, false);
    return true;
  }

  /**
   * Delete
   * @param key insert key
   * @param value insert value
   * @return if the deletion succeeds
   */
  bool Delete(KeyType key, ValueType value) {
    std::queue<TreeNodeUnion *> *path_queue = new std::queue<TreeNodeUnion *>();
    TreeNodeUnion *t_union;
    /******************* Concurrent node **********************/
    size_t cur_id = AcquireWriteId();
    t_union = new TreeNodeUnion();
    t_union->is_tree_ = true;
    t_union->tree_ = this;
    path_queue->push(t_union);
    /******************* Concurrent node **********************/
    TreeNode *result = nullptr;
    TreeNode *cur_node = root_;

    while (true) {
      cur_node->PushWriteId(cur_id);
      cur_node->WriterWhileLoop(cur_id);
      t_union = new TreeNodeUnion();
      t_union->tree_node_ = cur_node;
      t_union->is_tree_ = false;
      path_queue->push(t_union);
      if (cur_node->RemoveOneStillValid(order_)) UnlockQueueTillNow(path_queue, cur_node, false);
      if (cur_node->IsLeaf()) {
        result = cur_node;
        break;
      } else {
        cur_node = cur_node->FindBestFitChild(key);
      }
    }
    TreeNode *new_root = root_->MergeFromLeaf(result, key, value, order_);
    // case when did not find proper key value pair to delete
    if (new_root == nullptr) {
      UnlockQueue(path_queue, false);
      return false;
    }
    this->root_ = new_root;
    UnlockQueue(path_queue, false);
    return true;
  }

  /**
   * Get value
   * @param index_key scan key
   * @param results pointers to result vector
   */
  void GetValue(KeyType index_key, std::vector<ValueType> *results) {
    // common::SpinLatch::ScopedSpinLatch guard(&latch_);
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

  /**
   * Get values by ascending order
   * @param index_low_key lower scan key
   * @param index_high_key higher scan key
   * @param results pointers to result vector
   * @param limit limit number of result. 0 == unlimited.
   */
  void GetValueAscending(KeyType index_low_key, KeyType index_high_key, std::vector<ValueType> *results,
                         uint32_t limit) {
    size_t cur_id = AcquireWriteId();
    ReaderWhileLoop(cur_id);
    uint32_t count = 0;
    TreeNode *cur_node = root_->GetNodeRecursive(index_low_key);
    while (cur_node != nullptr) {
      auto cur = cur_node->value_list_;
      while (cur != nullptr) {
        if (cur->KeyCmpGreater(cur->key_, index_high_key)) {
          ReaderRelease();
          return;
        }
        if (cur->KeyCmpGreater(cur->key_, index_low_key) || cur->KeyCmpEqual(cur->key_, index_low_key)) {
          (*results).reserve((*results).size() + cur->GetAllValues().size());
          for (auto value : cur->GetAllValues()) {
            (*results).emplace_back(value);
          }
          ++count;
          if (count == limit) {
            ReaderRelease();
            return;
          }
        }
        cur = cur->next_;
      }
      cur_node = cur_node->right_sibling_;
    }
  }

  /**
   * Get values by descending order
   * @param index_low_key lower scan key
   * @param index_high_key higher scan key
   * @param results pointers to result vector
   * @param limit limit number of result. 0 == unlimited.
   */
  void GetValueDescendingLimited(KeyType index_low_key, KeyType index_high_key, std::vector<ValueType> *results,
                                 uint32_t limit) {
    size_t cur_id = AcquireWriteId();
    ReaderWhileLoop(cur_id);
    uint32_t count = 0;
    TreeNode *cur_node = root_->GetNodeRecursive(index_high_key);
    while (cur_node != nullptr) {
      auto cur = cur_node->value_list_->FindListEnd();
      while (cur != nullptr) {
        if (cur->KeyCmpLess(cur->key_, index_low_key)) {
          ReaderRelease();
          return;
        }
        if (cur->KeyCmpLessEqual(cur->key_, index_high_key)) {
          auto value_list = cur->GetAllValues();
          (*results).reserve((*results).size() + (value_list).size());
          for (auto value : value_list) {
            (*results).emplace_back(value);
          }
          ++count;
          if (count == limit) {
            ReaderRelease();
            return;
          }
        }
        cur = cur->prev_;
      }
      cur_node = cur_node->left_sibling_;
    }
  }

  /**
   * Get heap usage of the tree
   * @return size of used heap
   */
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

  /**
   * Get heap usage of a node
   * @param node treenode
   * @return size of used heap of a node
   */
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
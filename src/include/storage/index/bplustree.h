
#include <map>
#include <vector>
#include "index.h"


namespace terrier::storage::index {
typedef struct SplitReturn{
    InnerList *split_value;
    TreeNode *parent;
    TreeNode *left_child;
    TreeNode *right_child;
} SplitReturn;

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class InnerList 
{   
    public:
        KeyType key_;
        ValueType value_;
        InnerList *prev_;
        InnerList *next_;
        InnerList *dup_next_; // singly linked list for storing duplicated key 
        
        InnerList(KeyType key, ValueType val, InnerList *prev = nullptr, InnerList *next = nullptr)
        {
            
            *key_ = key;
            *value_ = val;
            prev_ = prev;
            next_ = next;
            dup_next_ = nullptr;
        }
   
        bool isInDupChain(){
            return dup_next == nullptr;
        }

        void InsertDup( InnerList *new_value ){
            InnerList *cur = this;
            InnerList *next = dup_next_;
            if(next == nullptr){
                dup_next = new_value;
                return;
            }
            while (next != nullptr){
                cur = next;
                next = cur->dup_next_;
            }
            cur->dup_next_ = new_value;
        }
};

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class TreeNode
{
    public:
        size_t size;
        InnerList *value_list_; // list of value points
        std::list<InnerList> *ptr_list_; // list of pointers to the next treeNode
        Map<KeyType, InnerList*> *value_node_map_;
        Map<KeyType, InnerList*> *ptr_node_map_;
        TreeNode *parent_;
        TreeNode *sibling_;
        TreeNode(TreeNode *parent, value_list = nullptr, ptr_list = nullptr, parent = nullptr; sibling = nullptr){
            value_list_ = value_list;
            ptr_list_ = ptr_list;
            parent_ = parent;
            sibling_ = sibling;
        }
        ~TreeNode()
        {
        }
        bool isLeaf() { return ptr_list_ == nullptr; }
        
        
        /**
         * Recursively find the position to perform insert. 
         * Terminate if reach the child node, insert into it
         * otherwise find the best child node and recursively perform insertion
         **/
        TreeNode *Insert( InnerList *new_value ){
            bool result = nullptr;
            if ( isLeaf() ){
                result = insertAtLeafNode(new_value);
            }
            else{
                TreeNode *child_node = findBestFitChild(KeyType key);
                result = chld_node->Insert(new_value);
            }
            return result;
        }

        TreeNode *InsertUnique( InnerList *new_value ){
            // TODO: implement unique insertion
            return Insert(new_value);
        }
        TreeNode *Delete( KeyType key, ValueType value ){
            return nullptr;
        }
	    ValueType Find( KeyType key );


        private:
        TreeNode *insertAtLeafNode(InnerList *new_list){
            KeyType key = new_list->key_; 
            ValueType val = new_list->value_;
            TreeNode *result = nullptr;
            InnerList *cur = value_list_;
            InnerList *next = nullptr;
            while (cur != nullptr){
                // if contains key just append in the key linked list
                if (cur->key_ == key){
                    bool sub_res = cur->InsertDup(new_list);
                    result = this;
                }
                else{
                    
                    // else, find best fit point to insert a new node
                    next = cur ->next_;
                    if (next != nullptr && next->key_ <= key) cur = cur->next_;
                    else{
                        // if should insert at the front
                        if (cur->key_ > key){
                            
                            new_list->next_ = cur;
                            cur->prev_ = new_list; 
                            value_list_ = new_list;
                            size ++;
                            break;
                        }
                        // if reaches the end, just directly append at the end
                        else if (next == nullptr){
                            cur->next_ = new_list;
                            size ++;
                            break;
                        }
                        // if place between cur and next
                        else{
                            cur->next_ = new_list;
                            next->prev_ = new_list;
                            new_list->next_ = next;
                            new_list->prev_ = cur;
                            size ++;
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
        InnerList *findBestFitChild(KeyType key) {
            InnerList *cur_val = value_list_;
            InnerList *next_val;
            std::list<InnerList*>::iterator ptr_iter = ptr_list_.begin();
            while (cur_val != nullptr){
                if (cur_val->key_ > key){
                    return *ptr_iter;
                }
                ++ptr_iter;
                next_val = cur_val->next_;
                if(next_val->key_ == nullptr) return *ptr_iter;
                else if (next_val->key_ > key) return *ptr_iter;
                else cur_val = next_val;
            }
        }

        InnerList *findInCurNode(KeyType key){
            InnerList *result = nullptr;
            for (InnerList *i = value_list_; i != nullptr; i = i->next_){
                if (i->key_ == key){
                    result - i;
                    break;
                }
            }
            return result;
        }
        // going from left to root, recursively split child, insert a new node at parent
        // in case split fails at current level, return nullptr, restore  tree from failed node 
        TreeNode *Split(TreeNode *cur_node, TreeNode *root_node, size_t order, vector<InnerList *> restore_stack,
                        InnerList *split_value_list = nullptr, TreeNode *left_child = nullptr, TreeNode *right_child = nullptr){
            
            restore_stack.push_back(split_value_list); 
            // base case: root is split, create a new root and return it
            if (cur_node == nullptr){
                TreeNode *new_root = new TreeNode(nullptr);
                // if the creation fails
                if (new_root == nullptr){
                    return RestoreTreeFromNode(left_child, right_child, restore_stack);
                }
                ConfigureNewSplitNode(new_root, split_value_list, left_child, right_child);
                return new_root
            }
            
            if (cur_node->isLeaf()){
                // base case: leaf node and no need to split
                if (cur_node->size <= order) return root_node;
            } 
            else{
                // insert the new node in current
                ConfigureNewSplitNode(cur_node, split_value_list, left_chld, right_child);
            }
            // otherwise split the current node 
            SplitReturn *split_res = SplitNode(cur_node);
            // fail to split into two nodes
            if (split_res == nullptr) {
                RestoreTreeFromNode(left_node, right_node, restore_stack);
                return root_node;
            }
            return Split(cur_res->parent, root_node, order, restore_stack, split_res->split_value, split_res->left_child, split_res->right_child);
            }
            // if null there are two cases, if 
        // return the top node and recursively restore child according to restoring stack
        TreeNode *RestoreTreeFromNode(TreeNode *left_node, TreeNode *right_node. vector<InnerList *> restore_stack);
        // configure parent, insert split value, left child, right child, 
        void ConfigureNewSplitNode(TreeNode *new_node, InnerList *split_value_list, TreeNode *left_child, TreeNode *right_child);
        // assume the node exceeds capacity
        // split the current node into two for left and non-leaf
        SplitReturn *SplitNode(TreeNode node) {
            SplitReturn result;
            return &result;
        }

};

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class BPlusTree
{
    private:
        TreeNode *root; // with parent node as empty for root
        size_t order_;
    public:
        BPlusTree(size_t order_)
        {
            order_ = order;
            root = 
        }
        InnerList *RebalanceTree(TreeNode *leaf_node){
            // wrapper for recursively rebalance the tree 
            // if size exceeds, pop the middle element going from child to parent
            return Split(left_node);
        }

        bool Insert( KeyType key, ValueType value ){
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
};
}
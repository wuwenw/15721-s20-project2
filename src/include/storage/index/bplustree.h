
#include <map>
#include "index.h"


namespace terrier::storage::index {
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

        bool InsertDup( KeyType key, ValueType val ){
            InnerList *cur = this;
            InnerList *next = dup_next_;
            while (next != nullptr){
                cur = next;
                next = cur->dup_next;
            }
            next = new InnerList(key, val);
            if (next == nullptr) return false;
            return true;
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
        TreeNode *subling_;
        TreeNode(TreeNode parent){
            value_list_ = nullptr;
            ptr_list_ = nullptr;
            value_node_map_ = new Map<KeyType, InnerList*>();
            ptr_node_map_ = new Map<KeyType, InnerList*>();
            parent_ = parent;
        }
        ~TreeNode()
        {
            if (ptr_list != nullptr) delete ptr_list;
            delete value_node_map_;
            delete ptr_node_map_;
        }
        bool isLeaf() { return ptr_list_ == nullptr; }
        
        
        /**
         * Recursively find the position to perform insert. 
         * Terminate if reach the child node, insert into it
         * otherwise find the best child node and recursively perform insertion
         **/
        TreeNode *Insert( KeyType key, ValueType value, size_t order ){
            bool result = nullptr;
            if ( isLeaf() ){
                result = insertInCurNode(key, value);
            }
            else{
                TreeNode *child_node = findBestFitChild(KeyType key);
                result = chld_node->Insert(key, value, order);
            }
            return result;
        }

        TreeNode *InsertUnique( KeyType key, ValueType value, size_t order ){
            // TODO: implement unique insertion
            return Insert(key, value, order);
        }
        TreeNode *Delete( KeyType key, ValueType value, size_t order ){
            return nullptr;
        }
	    ValueType Find( KeyType key );


        private:
        TreeNode *insertInCurNode( KeyType key, ValueType val){
            TreeNode *result = nullptr;
            InnerList *cur = value_list_;
            InnerList *next = nullptr;
            while (cur != nullptr){
                // if contains key just append in the key linked list
                if (cur->key_ == key){
                    bool sub_res = cur->InsertDup(key, val);
                    if (!sub_res) return result;
                    result = this;
                }
                else{
                    
                    // else, find best fit point to insert a new node
                    next = cur ->next_;
                    if (next != nullptr && next->key_ <= key) cur = cur->next_;
                    else{
                        // construct a new node
                        InnerList *new_list = new ListNode(key, value);
                        if (new_list == nullptr) return result;
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

        InnerList *Split();
        InnerList *Merge();
};

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class BPlusTree
{
    private:
        TreeNode *root;
        size_t order_;
    public:
        BPlusTree(size_t order_)
        {
            order_ = order;
        }

        InnerList *rebalanceTree(TreeNode *leaf_node, KeyType key, ValueType value){
            // wrapper for recursively rebalance the tree 
            // check if really neet to split
            if (leaf_node->size <= order) return root;
            // if size exceeds, pop the middle element going from child to parent
            TreeNode *cur_node = leaf_node;
            TreeNode *parent_node = cur_node->parent;
            while (cur_node != root){
                // find the middle value to split
                // split the value list into two
                if( cur_node->isLeaf()){
                    
                }
                else{

                }
            }
            // rollback if any steps fail

            // return the new root
            return nullptr;
        }

        bool Insert( KeyType key, ValueType value ){
            bool result = false;
            TreeNode *new_root = nullptr;
            result = root->Insert(key, value, order);
            if (!result) return result;
            new_root = rebalanceTree(result, key, value);
            if (new_root == nullptr) return false;
            root = new_root;
            return true;
        }
};
}
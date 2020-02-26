#include <map>


namespace terrier::storage::index {
template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class InnerList 
{   
    private:
        KeyType key_;
        ValueType value_;
        InnerList *prev_;
        InnerList *next_;
    public:
        InnerList(KeyType key, ValueType val)
        {
            *key_ = key;
            *value_ = val;
        }
};

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class TreeNode
{
    private:
        Mutable latch_;
        InnerList *value_list_; // list of value points
        InnerList *ptr_list_; // list of pointers to the next treeNode
        Map<KeyType, InnerList*> *value_node_map_;
        Map<KeyType, InnerList*> *ptr_node_map_;
        TreeNode *parent_;
    public:
        TreeNode()
        {
            value_list_ = nullptr;
            ptr_list_ = nullptr;
            value_node_map_ = new Map<KeyType, InnerList*>();
            ptr_node_map_ = new Map<KeyType, InnerList*>();
        }
        ~TreeNode()
        {
            delete value_node_map_;
            delete ptr_node_map_;
        }
        Void Insert( KeyType key, ValueType value );
        Void Delete( KeyType key );
	    VAL Find( KeyType key );
};

template <typename KeyType, typename ValueType, typename KeyComparator = std::less<KeyType>,
          typename KeyEqualityChecker = std::equal_to<KeyType>, typename KeyHashFunc = std::hash<KeyType>,
          typename ValueEqualityChecker = std::equal_to<ValueType>>
class BPlusTree 
{
    private:
        TreeNode *root;
};
}
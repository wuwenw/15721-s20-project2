#include <algorithm>
#include <random>
#include <vector>
#include "bwtree/bwtree.h"
#include "storage/index/bplustree.h"
#include "gtest/gtest.h"
#include "test_util/random_test_util.h"
#include "test_util/multithread_test_util.h"
#include "test_util/test_harness.h"

namespace terrier::storage::index {

struct BPlusTreeTests : public TerrierTest {
  const uint32_t num_threads_ =
      MultiThreadTestUtil::HardwareConcurrency() + (MultiThreadTestUtil::HardwareConcurrency() % 2);
};

void PrintNode(terrier::storage::index::BPlusTree<int64_t, int64_t>::TreeNode *node) {
  terrier::storage::index::BPlusTree<int64_t, int64_t>::InnerList *value = node->value_list_;
  size_t idx = 0;
  while (value != nullptr) {
    std::cerr << value->key_ << "-";
    value = value->next_;
  }
  std::cerr << "    Node size" << node->size << "\n";
  for (idx = 0; idx < node->ptr_list_.size(); idx ++) {
      PrintNode(node->ptr_list_[idx]);
  }
  std::cerr << "-------------\n";

}
void PrintTree(terrier::storage::index::BPlusTree<int64_t, int64_t> *tree) {
  if (tree->root != nullptr) {
    PrintNode(tree->root);
  }
}



// NOLINTNEXTLINE
TEST_F(BPlusTreeTests, EmptyTest) { EXPECT_TRUE(true); }

TEST_F(BPlusTreeTests, NaiveSequentialInsert) {
  // This defines the key space (0 ~ (1M - 1))
  const uint32_t key_num = 1;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  /*
    0
  */
  tree->Insert(0, 0);
  EXPECT_EQ(tree->root->value_list_->key_, 0);
  EXPECT_EQ(tree->root->IsLeaf(), true);
  EXPECT_EQ(tree->root->size, 1);
  /*
    0,1
  */
  tree->Insert(1, 1);
  EXPECT_EQ(tree->root->size, 2);
  EXPECT_EQ(tree->root->value_list_->key_, 0);
  EXPECT_EQ(tree->root->value_list_->next_->key_, 1);
  EXPECT_EQ(tree->root->IsLeaf(), true);
  /*
      1
    0   1,2
  */
  tree->Insert(2, 2);
  EXPECT_EQ(tree->root->size, 1);
  EXPECT_EQ(tree->root->value_list_->key_, 1);
  EXPECT_EQ(tree->root->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->next_->key_, 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->parent_, tree->root);
  EXPECT_EQ(tree->root->ptr_list_[1]->parent_, tree->root);

  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_, tree->root->ptr_list_[1]);
  EXPECT_EQ(tree->root->ptr_list_[1]->left_sibling_, tree->root->ptr_list_[0]);
  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_->size, 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->left_sibling_->size, 1);

  PrintTree(tree);
  /*
      1   2
    0   1   2,3
  */
  tree->Insert(3, 3);
  PrintTree(tree);
  EXPECT_EQ(tree->root->size, 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root->value_list_->key_, 1);
  EXPECT_EQ(tree->root->value_list_->next_->key_, 2);
  EXPECT_EQ(tree->root->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[2]->value_list_->key_, 2);
  EXPECT_EQ(tree->root->ptr_list_[2]->value_list_->next_->key_, 3);
  EXPECT_EQ(tree->root->ptr_list_[2]->size, 2);


  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_, tree->root->ptr_list_[1]);
  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_->right_sibling_, tree->root->ptr_list_[2]);
  EXPECT_EQ(tree->root->ptr_list_[2]->left_sibling_, tree->root->ptr_list_[1]);
  EXPECT_EQ(tree->root->ptr_list_[2]->left_sibling_->left_sibling_, tree->root->ptr_list_[0]);
  EXPECT_EQ(tree->root->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[2]->size, 2);

  /*
          2
      1       3
    0   1   2   3,4
  */
  tree->Insert(4, 4);
  PrintTree(tree);
  EXPECT_EQ(tree->root->size, 1);
  EXPECT_EQ(tree->root->value_list_->key_, 2);
  EXPECT_EQ(tree->root->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->value_list_->key_, 1);
  EXPECT_EQ(tree->root->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[0]->IsLeaf(), false);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 3);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->size, 1);

  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 3);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->size, 2);
  std::cerr << "hahaha\n";
  tree->Insert(4, 5);
  PrintTree(tree);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[0], 4);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[1], 5);

  delete tree;
}

TEST_F(BPlusTreeTests, NaiveRandomInsert) {
  // This defines the key space (0 ~ (1M - 1))
  const uint32_t key_num = 1;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  /*
    0
  */
  tree->Insert(0, 0);
  EXPECT_EQ(tree->root->value_list_->key_, 0);
  EXPECT_EQ(tree->root->IsLeaf(), true);
  EXPECT_EQ(tree->root->size, 1);
  /*
    0,4
  */
  tree->Insert(4, 4);
  EXPECT_EQ(tree->root->size, 2);
  EXPECT_EQ(tree->root->value_list_->key_, 0);
  EXPECT_EQ(tree->root->value_list_->next_->key_, 4);
  EXPECT_EQ(tree->root->IsLeaf(), true);
  /*
      4
    0   4,8
  */
  tree->Insert(8, 8);
  EXPECT_EQ(tree->root->size, 1);
  EXPECT_EQ(tree->root->value_list_->key_, 4);
  EXPECT_EQ(tree->root->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->next_->key_, 8);
  EXPECT_EQ(tree->root->ptr_list_[0]->parent_, tree->root);
  EXPECT_EQ(tree->root->ptr_list_[1]->parent_, tree->root);

  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_, tree->root->ptr_list_[1]);
  EXPECT_EQ(tree->root->ptr_list_[1]->left_sibling_, tree->root->ptr_list_[0]);
  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_->size, 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->left_sibling_->size, 1);

  PrintTree(tree);
  /*
      4   8
    0   4   8,12
  */
  tree->Insert(12, 12);
  PrintTree(tree);
  EXPECT_EQ(tree->root->size, 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root->value_list_->key_, 4);
  EXPECT_EQ(tree->root->value_list_->next_->key_, 8);
  EXPECT_EQ(tree->root->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[2]->value_list_->key_, 8);
  EXPECT_EQ(tree->root->ptr_list_[2]->value_list_->next_->key_, 12);
  EXPECT_EQ(tree->root->ptr_list_[2]->size, 2);


  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_, tree->root->ptr_list_[1]);
  EXPECT_EQ(tree->root->ptr_list_[0]->right_sibling_->right_sibling_, tree->root->ptr_list_[2]);
  EXPECT_EQ(tree->root->ptr_list_[2]->left_sibling_, tree->root->ptr_list_[1]);
  EXPECT_EQ(tree->root->ptr_list_[2]->left_sibling_->left_sibling_, tree->root->ptr_list_[0]);
  EXPECT_EQ(tree->root->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[2]->size, 2);

  /*
          8
      4       12
    0   4   8   12,16
  */
  tree->Insert(16, 16);
  PrintTree(tree);
  EXPECT_EQ(tree->root->size, 1);
  EXPECT_EQ(tree->root->value_list_->key_, 8);
  EXPECT_EQ(tree->root->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->value_list_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[0]->IsLeaf(), false);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 12);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->size, 1);

  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 8);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 12);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->key_, 16);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->size, 2);
  std::cerr << "hahaha\n";
  tree->Insert(16, 20);
  PrintTree(tree);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[0], 16);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[1], 20);
  

  tree->Insert(5, 5);
  tree->Insert(11, 11);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->value_list_->next_->key_, 5);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->value_list_->next_->key_, 11);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->parent_->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->parent_->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->size, 2);
  std::cerr << "pre insert 10, 10\n";
  PrintTree(tree);
  tree->Insert(10, 10);
  PrintTree(tree);
  // std::cerr << "print node\n";
  // PrintNode(tree->root->ptr_list_[1]);
  // std::cerr << "parent\n";
  // PrintNode(tree->root->ptr_list_[1]->ptr_list_[0]->parent_);
  // terrier::storage::index::BPlusTree<int64_t, int64_t>::TreeNode::SplitReturn a = tree->root->SplitNode(tree->root->ptr_list_[1]->ptr_list_[0]);
  
  // std::cerr << "two children\n";
  // std::cerr << a.left_child->value_list_->key_<<"\n";
  // std::cerr << a.right_child->value_list_->key_<<"\n";
  // PrintNode(tree->root->ptr_list_[1]);
  // std::cerr << "left parent\n";
  // PrintNode(a.left_child->parent_);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 10);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->next_->key_, 12);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_.size(), 3);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 8);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 10);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->key_, 11);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[2]->value_list_->key_, 12);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[2]->value_list_->next_->key_, 16);

  for (int64_t i = 18; i < 100000; i++) {
    tree->InsertUnique(i, i);
  }
  // PrintTree(tree);
  delete tree;
}
TEST_F(BPlusTreeTests, ManyInsert) {
  // This defines the key space (0 ~ (1M - 1))
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(8);

  for (int64_t i = 18; i < 100000; i++) {
    tree->InsertUnique(i, i);
  }
  // PrintTree(tree);
  delete tree;
}

TEST_F(BPlusTreeTests, NaiveSequentialScanTest) {
  const uint32_t key_num = 32;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);
  std::cerr << "================= begin insert ==============\n";
  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
    tree->Insert(keys[i], keys[i]);
  }
  std::cerr << "================= finish insert ==============\n";
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], results);
    EXPECT_EQ(results, std::vector<int64_t>(1, keys[i]));
  }  

  delete tree;
}

TEST_F(BPlusTreeTests, SequentialScanTest) {
  const uint32_t key_num = 1024;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);
  std::cerr << "================= begin insert ==============\n";
  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
    tree->Insert(keys[i], keys[i]);
  }
  std::cerr << "================= finish insert ==============\n";
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], results);
    EXPECT_EQ(results, std::vector<int64_t>(1, keys[i]));
}
  delete tree;
}

TEST_F(BPlusTreeTests, NaiveDuplicateScanTest) {
  const uint32_t key_num = 32;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
  }
  std::cerr << "================= finish insert ==============\n";
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], results);
    EXPECT_EQ(results, std::vector<int64_t>(3, keys[i]));
  }

  delete tree;
}

TEST_F(BPlusTreeTests, DuplicateScanTest) {
  const uint32_t key_num = 1024;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
  }
  std::cerr << "================= finish insert ==============\n";
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], results);
    EXPECT_EQ(results, std::vector<int64_t>(5, keys[i]));
  }

  delete tree;
  }
}  // namespace terrier::storage::index
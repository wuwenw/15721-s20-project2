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

  delete tree;
}

TEST_F(BPlusTreeTests, NaiveSequentialScanTest) {
  const uint32_t key_num = 1;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  for (int64_t i = 0; i < key_num; i++) {
    tree->Insert(keys[i], keys[i]);
    tree->GetValue(keys[i], results);
    EXPECT_EQ(results, std::vector<int64_t>(1, keys[i]));
  }

  delete tree;
}

}  // namespace terrier::storage::index
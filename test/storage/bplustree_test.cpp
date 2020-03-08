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

// NOLINTNEXTLINE
TEST_F(BPlusTreeTests, EmptyTest) { EXPECT_TRUE(true); }

TEST_F(BPlusTreeTests, NaiveSequentialInsert) {
  // This defines the key space (0 ~ (1M - 1))
  const uint32_t key_num = 8;
  terrier::storage::index::BPlusTree<int64_t, int64_t> *tree =
      new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  for (uint32_t i = 0; i < key_num; i++) {
    int64_t key = keys[i];
    tree->Insert(key, key);
  }
  // test tree structiure
  /*
        3
     1      5
   0   2  4   67
  */
  // test root
  EXPECT_EQ(tree->root->size, 1);
  EXPECT_EQ(tree->root->value_list_->key_, 3);
  EXPECT_EQ(tree->root->IsLeaf(), false);
  // test first level two nodes
  EXPECT_EQ(tree->root->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->value_list_->key_, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->value_list_->key_, 5);
  EXPECT_EQ(tree->root->ptr_list_[0]->IsLeaf(), false);
  EXPECT_EQ(tree->root->ptr_list_[1]->IsLeaf(), false);

  EXPECT_EQ(tree->root->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->size, 1);

  EXPECT_EQ(tree->root->ptr_list_[1]->IsLeaf(), false);
  // test left node level
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_.size(), 2);

  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->IsLeaf(), true);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->value_list_->key_, 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->IsLeaf(), true);

  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[1]->size, 1);

  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->IsLeaf(), true);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 6);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->next_->key_, 7);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->IsLeaf(), true);

  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->size, 1);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[0]->size, 2);

  // assert leaf linked properly
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->right_sibling_->value_list_->key_, 2);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->right_sibling_->right_sibling_->value_list_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[0]->ptr_list_[0]->right_sibling_->right_sibling_->right_sibling_->value_list_->key_,
            6);

  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 6);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->left_sibling_->value_list_->key_, 4);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->left_sibling_->left_sibling_->value_list_->key_, 2);
  EXPECT_EQ(tree->root->ptr_list_[1]->ptr_list_[1]->left_sibling_->left_sibling_->left_sibling_->value_list_->key_, 0);
  delete tree;
}
}  // namespace terrier::storage::index
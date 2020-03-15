#include "storage/index/bplustree.h"
#include <algorithm>
#include <random>
#include <vector>
#include "bwtree/bwtree.h"
#include "gtest/gtest.h"
#include "test_util/multithread_test_util.h"
#include "test_util/random_test_util.h"
#include "test_util/test_harness.h"

namespace terrier::storage::index {

struct BPlusTreeTests : public TerrierTest {
  const uint32_t num_threads_ =
      MultiThreadTestUtil::HardwareConcurrency() + (MultiThreadTestUtil::HardwareConcurrency() % 2);
};

void PrintNode(terrier::storage::index::BPlusTree<int64_t, int64_t>::TreeNode *node) {
  terrier::storage::index::BPlusTree<int64_t, int64_t>::InnerList *value = node->value_list_;
  size_t idx = 0;
  // std::cerr<<"here\n";
  while (value != nullptr) {
    std::cerr << value->key_ << ",";
    value = value->next_;
  }
  std::cerr << ">>>>>\n";
  for (idx = 0; idx < node->ptr_list_.size(); idx++) {
    PrintNode(node->ptr_list_[idx]);
  }
}
void PrintTree(terrier::storage::index::BPlusTree<int64_t, int64_t> *tree) {
  if (tree->root_ != nullptr) {
    PrintNode(tree->root_);
  }
}

// NOLINTNEXTLINE
TEST_F(BPlusTreeTests, EmptyTest) { EXPECT_TRUE(true); }

TEST_F(BPlusTreeTests, NaiveSequentialInsert) {
  // This defines the key space (0 ~ (1M - 1))
  const uint32_t key_num = 1;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  /*
    0
  */
  tree->Insert(0, 0);
  EXPECT_EQ(tree->root_->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->IsLeaf(), true);
  EXPECT_EQ(tree->root_->size_, 1);
  /*
    0,1
  */
  tree->Insert(1, 1);
  EXPECT_EQ(tree->root_->size_, 2);
  EXPECT_EQ(tree->root_->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->value_list_->next_->key_, 1);
  EXPECT_EQ(tree->root_->IsLeaf(), true);
  /*
      1
    0   1,2
  */
  tree->Insert(2, 2);
  EXPECT_EQ(tree->root_->size_, 1);
  EXPECT_EQ(tree->root_->value_list_->key_, 1);
  EXPECT_EQ(tree->root_->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->key_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->parent_, tree->root_);
  EXPECT_EQ(tree->root_->ptr_list_[1]->parent_, tree->root_);

  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_, tree->root_->ptr_list_[1]);
  EXPECT_EQ(tree->root_->ptr_list_[1]->left_sibling_, tree->root_->ptr_list_[0]);
  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->left_sibling_->size_, 1);

  PrintTree(tree);
  /*
      1   2
    0   1   2,3
  */
  tree->Insert(3, 3);
  PrintTree(tree);
  EXPECT_EQ(tree->root_->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root_->value_list_->key_, 1);
  EXPECT_EQ(tree->root_->value_list_->next_->key_, 2);
  EXPECT_EQ(tree->root_->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->key_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->next_->key_, 3);
  EXPECT_EQ(tree->root_->ptr_list_[2]->size_, 2);

  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_, tree->root_->ptr_list_[1]);
  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_->right_sibling_, tree->root_->ptr_list_[2]);
  EXPECT_EQ(tree->root_->ptr_list_[2]->left_sibling_, tree->root_->ptr_list_[1]);
  EXPECT_EQ(tree->root_->ptr_list_[2]->left_sibling_->left_sibling_, tree->root_->ptr_list_[0]);
  EXPECT_EQ(tree->root_->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[2]->size_, 2);

  /*
          2
      1       3
    0   1   2   3,4
  */
  tree->Insert(4, 4);
  PrintTree(tree);
  EXPECT_EQ(tree->root_->size_, 1);
  EXPECT_EQ(tree->root_->value_list_->key_, 2);
  EXPECT_EQ(tree->root_->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[0]->IsLeaf(), false);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 3);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->value_list_->key_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->size_, 1);

  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 3);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->next_->key_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->size_, 2);
  tree->Insert(4, 5);
  PrintTree(tree);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[0], 4);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[1], 5);

  delete tree;
}

TEST_F(BPlusTreeTests, NaiveRandomInsert) {
  // This defines the key space (0 ~ (1M - 1))
  const uint32_t key_num = 1;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  /*
    0
  */
  tree->Insert(0, 0);
  EXPECT_EQ(tree->root_->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->IsLeaf(), true);
  EXPECT_EQ(tree->root_->size_, 1);
  /*
    0,4
  */
  tree->Insert(4, 4);
  EXPECT_EQ(tree->root_->size_, 2);
  EXPECT_EQ(tree->root_->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->value_list_->next_->key_, 4);
  EXPECT_EQ(tree->root_->IsLeaf(), true);
  /*
      4
    0   4,8
  */
  tree->Insert(8, 8);
  EXPECT_EQ(tree->root_->size_, 1);
  EXPECT_EQ(tree->root_->value_list_->key_, 4);
  EXPECT_EQ(tree->root_->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->key_, 8);
  EXPECT_EQ(tree->root_->ptr_list_[0]->parent_, tree->root_);
  EXPECT_EQ(tree->root_->ptr_list_[1]->parent_, tree->root_);

  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_, tree->root_->ptr_list_[1]);
  EXPECT_EQ(tree->root_->ptr_list_[1]->left_sibling_, tree->root_->ptr_list_[0]);
  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->left_sibling_->size_, 1);

  PrintTree(tree);
  /*
      4   8
    0   4   8,12
  */
  tree->Insert(12, 12);
  PrintTree(tree);
  EXPECT_EQ(tree->root_->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root_->value_list_->key_, 4);
  EXPECT_EQ(tree->root_->value_list_->next_->key_, 8);
  EXPECT_EQ(tree->root_->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->key_, 8);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->next_->key_, 12);
  EXPECT_EQ(tree->root_->ptr_list_[2]->size_, 2);

  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_, tree->root_->ptr_list_[1]);
  EXPECT_EQ(tree->root_->ptr_list_[0]->right_sibling_->right_sibling_, tree->root_->ptr_list_[2]);
  EXPECT_EQ(tree->root_->ptr_list_[2]->left_sibling_, tree->root_->ptr_list_[1]);
  EXPECT_EQ(tree->root_->ptr_list_[2]->left_sibling_->left_sibling_, tree->root_->ptr_list_[0]);
  EXPECT_EQ(tree->root_->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[2]->size_, 2);

  /*
          8
      4       12
    0   4   8   12,16
  */
  tree->Insert(16, 16);
  PrintTree(tree);
  EXPECT_EQ(tree->root_->size_, 1);
  EXPECT_EQ(tree->root_->value_list_->key_, 8);
  EXPECT_EQ(tree->root_->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[0]->IsLeaf(), false);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 12);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->IsLeaf(), false);

  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->value_list_->key_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->size_, 1);

  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 8);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 12);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->next_->key_, 16);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->size_, 2);
  tree->Insert(16, 20);
  PrintTree(tree);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[0], 16);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->next_->same_key_values_[1], 20);

  tree->Insert(5, 5);
  tree->Insert(11, 11);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->value_list_->next_->key_, 5);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->value_list_->next_->key_, 11);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->parent_->size_, 1);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->parent_->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->size_, 2);
  PrintTree(tree);
  tree->Insert(10, 10);
  PrintTree(tree);
  // std::cerr << "print node\n";
  // PrintNode(tree->root_->ptr_list_[1]);
  // std::cerr << "parent\n";
  // PrintNode(tree->root_->ptr_list_[1]->ptr_list_[0]->parent_);
  // terrier::storage::index::BPlusTree<int64_t, int64_t>::TreeNode::SplitReturn a =
  // tree->root_->SplitNode(tree->root_->ptr_list_[1]->ptr_list_[0]);

  // std::cerr << "two children\n";
  // std::cerr << a.left_child->value_list_->key_<<"\n";
  // std::cerr << a.right_child->value_list_->key_<<"\n";
  // PrintNode(tree->root_->ptr_list_[1]);
  // std::cerr << "left parent\n";
  // PrintNode(a.left_child->parent_);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 10);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->key_, 12);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_.size(), 3);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 8);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 10);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->next_->key_, 11);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[2]->value_list_->key_, 12);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[2]->value_list_->next_->key_, 16);

//  for (int64_t i = 18; i < 100000; i++) {
//    tree->InsertUnique(i, i);
//  }
  // PrintTree(tree);
  delete tree;
}

TEST_F(BPlusTreeTests, ComplexRandomInsert) {
  // This defines the key space (0 ~ (1M - 1))
  const uint32_t key_num = 1;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  tree->Insert(12, 12);
  tree->Insert(36, 36);
  tree->Insert(9, 9);
  tree->Insert(10, 10);
  tree->Insert(7, 7);
  tree->Insert(15, 15);
  tree->Insert(81, 81);
  tree->Insert(72, 72);
  tree->Insert(78, 78);
  tree->Insert(25, 25);
  tree->Insert(31, 31);
  tree->Insert(0, 0);
  tree->Insert(2, 2);
  tree->Insert(12, 12);
  tree->Insert(36, 36);
  tree->Insert(9, 9);
  tree->Insert(10, 10);
  tree->Insert(7, 7);
  tree->Insert(15, 15);
  tree->Insert(81, 81);
  tree->Insert(72, 72);
  tree->Insert(78, 78);
  tree->Insert(25, 25);
  tree->Insert(31, 31);
  tree->Insert(0, 0);
  tree->Insert(2, 2);
  // l1
  EXPECT_EQ(tree->root_->size_, 2);
  EXPECT_EQ(tree->root_->value_list_->key_, 12);
  EXPECT_EQ(tree->root_->value_list_->next_->key_, 36);
  EXPECT_EQ(tree->root_->ptr_list_.size(), 3);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->next_->key_, 9);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 15);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->key_, 25);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->key_, 72);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->next_->key_, 78);
//  EXPECT_EQ(tree->InsertUnique(12, 12), false);
//  EXPECT_EQ(tree->InsertUnique(36, 36), false);
//  EXPECT_EQ(tree->InsertUnique(7, 7), false);
//  EXPECT_EQ(tree->InsertUnique(9, 9), false);
//  EXPECT_EQ(tree->InsertUnique(10, 10), false);
//  EXPECT_EQ(tree->InsertUnique(72, 72), false);
//  EXPECT_EQ(tree->InsertUnique(78, 78), false);
//  EXPECT_EQ(tree->InsertUnique(25, 25), false);
//  EXPECT_EQ(tree->InsertUnique(31, 31), false);

  // PrintTree(tree);
  delete tree;
}
//TEST_F(BPlusTreeTests, ManyInsert) {
//  // This defines the key space (0 ~ (1M - 1))
//  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(8);
//
//  for (int64_t i = 18; i < 100000; i++) {
//    tree->InsertUnique(i, i);
//  }
//  // PrintTree(tree);
//  delete tree;
//}

TEST_F(BPlusTreeTests, RandomDeletion) {
  // This defines the key space (0 ~ (1M - 1))
  const uint32_t key_num = 1;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(4);

  std::vector<int64_t> keys;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
  }
  // test insertion
  tree->Insert(12, 12);
  tree->Insert(36, 36);
  tree->Insert(9, 9);
  tree->Insert(10, 10);
  tree->Insert(7, 7);
  tree->Insert(15, 15);
  tree->Insert(81, 81);
  tree->Insert(72, 72);
  tree->Insert(78, 78);
  tree->Insert(25, 25);
  tree->Insert(31, 31);
  tree->Insert(0, 0);
  tree->Insert(2, 2);
  tree->Insert(34, 34);
  tree->Insert(65, 65);
  tree->Insert(105, 105);
  tree->Insert(97, 97);
  tree->Insert(26, 26);
  tree->Insert(16, 16);
  tree->Insert(19, 19);
  tree->Insert(80, 80);
  tree->Insert(3, 3);
  tree->Insert(67, 67);
  tree->Insert(71, 71);
  tree->Insert(178, 178);
  tree->Insert(164, 164);
  tree->Insert(145, 145);
  tree->Insert(157, 157);
  tree->Insert(162, 162);
  tree->Insert(135, 135);

  // test insertion result
  /*
                                                          31,72

                   3,10,15,19                              36,65                                  81,105,157

  0,2    3,7,9    10,12    15,16   19,25,26       31,34   36,45   65,67,71      72,78,80   81,97   105,135,145   157,162,164,178
  */
  // level 1
  EXPECT_EQ(tree->root_->size_, 2);
  EXPECT_EQ(tree->root_->value_list_->key_, 31);
  EXPECT_EQ(tree->root_->value_list_->next_->key_, 81);
  EXPECT_EQ(tree->root_->ptr_list_.size(), 3);
  // level 2
  EXPECT_EQ(tree->root_->ptr_list_[0]->size_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->key_, 3);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->next_->key_, 10);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->next_->next_->key_, 15);
  EXPECT_EQ(tree->root_->ptr_list_[0]->value_list_->next_->next_->next_->key_, 19);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_.size(), 5);

  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 36);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->key_, 72);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_.size(), 3);

  EXPECT_EQ(tree->root_->ptr_list_[2]->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->key_, 105);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->next_->key_, 157);
  EXPECT_EQ(tree->root_->ptr_list_[2]->ptr_list_.size(), 3);

  // level 3
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[0]->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[0]->value_list_->key_, 0);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[0]->value_list_->next_->key_, 2);

  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->size_, 3);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->value_list_->key_, 3);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->value_list_->next_->key_, 7);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[1]->value_list_->next_->next_->key_, 9);

  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[2]->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[2]->value_list_->key_, 10);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[2]->value_list_->next_->key_, 12);

  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[3]->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[3]->value_list_->key_, 15);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[3]->value_list_->next_->key_, 16);

  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[4]->size_, 3);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[4]->value_list_->key_, 19);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[4]->value_list_->next_->key_, 25);
  EXPECT_EQ(tree->root_->ptr_list_[0]->ptr_list_[4]->value_list_->next_->next_->key_, 26);



  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->size_, 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->value_list_->key_, 31);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[0]->value_list_->next_->key_, 34);

  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->size_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[1]->value_list_->key_, 36);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[2]->value_list_->key_, 72);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[2]->value_list_->next_->key_, 78);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[2]->value_list_->next_->next_->key_, 80);
  /*
                                                        31,81

                  3,10,15,19                                   36,72                                105,157

  0,2    3,7,9    10,12    15,16   19,25,26        31,34    36,65,67,71    72,78,80       81,97     105,135,145     157,162,164,178
  */
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->key_, 105);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->next_->key_, 157);
  EXPECT_EQ(tree->root_->ptr_list_[2]->ptr_list_[1]->value_list_->key_, 105);

  // test deletion
  tree->Delete(105, 105);
  /*
                                                               31,81

                  3,10,15,19                                   36,72                                 135,157

  0,2    3,7,9    10,12    15,16   19,25,26        31,34    36,65,67,71    72,78,80        81,97     135,145     157,162,164,178
  */
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->key_, 135);
  EXPECT_EQ(tree->root_->ptr_list_[2]->value_list_->next_->key_, 157);
  EXPECT_EQ(tree->root_->ptr_list_[2]->ptr_list_[1]->value_list_->key_, 135);
  EXPECT_EQ(tree->root_->ptr_list_[2]->ptr_list_[1]->value_list_->next_->key_, 145);
  EXPECT_EQ(tree->root_->ptr_list_[2]->ptr_list_[1]->value_list_->next_->next_, nullptr);
  
  PrintNode(tree->root_);
  std::cerr << "Enter Deleting 97\n";
  tree->Delete(97, 97);
  PrintNode(tree->root_);
  std::cerr << tree->root_->ptr_list_[1]->ptr_list_.size() << "\n";
  /*
                                                               31

                  3,10,15,19                                                             36,72,81,157

  0,2    3,7,9    10,12    15,16   19,25,26                       31,34    36,65,67,71    72,78,80        81,135,145     157,162,164,178
  */
  EXPECT_EQ(tree->root_->size_, 1);
  EXPECT_EQ(tree->root_->value_list_->key_, 31);
  EXPECT_EQ(tree->root_->ptr_list_.size(), 2);
  EXPECT_EQ(tree->root_->ptr_list_[1]->size_, 4);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->key_, 36);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->key_, 72);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->next_->key_, 81);
  EXPECT_EQ(tree->root_->ptr_list_[1]->value_list_->next_->next_->next_->key_, 157);
  
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_.size(), 5);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[3]->value_list_->key_, 81);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[3]->value_list_->next_->key_, 135);
  EXPECT_EQ(tree->root_->ptr_list_[1]->ptr_list_[3]->value_list_->next_->next_->key_, 145);
  
  tree->Delete(10, 10);
  /*
                                                               31

                  3,9,15,19                                                             36,72,81,157

  0,2    3,7      9,12    15,16   19,25,26                       31,34    36,65,67,71    72,78,80        81,135,145     157,162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 10 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(7, 7);
  /*
                                                     31

                  3,15,19                                                             36,72,81,157

  0,2    3,9,12    15,16   19,25,26                       31,34    36,65,67,71    72,78,80        81,135,145     157,162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 7 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(78, 78);
  /*
                                                     31

                  3,15,19                                                             36,72,81,157

  0,2    3,9,12    15,16   19,25,26                       31,34    36,65,67,71    72,80        81,135,145     157,162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 78 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(67, 67);
  /*
                                                     31

                  3,15,19                                                             36,72,81,157

  0,2    3,9,12    15,16   19,25,26                       31,34    36,65,71    72,80        81,135,145     157,162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 67 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(72, 72);
  /*
                                                     31

                  3,15,19                                                    36,80,135,157

  0,2    3,9,12    15,16   19,25,26                       31,34    36,65,71    80,81    135,145     157,162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 72 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(157, 157);
  /*
                                                     31

                  3,15,19                                                    36,80,135,162

  0,2    3,9,12    15,16   19,25,26                       31,34    36,65,71    80,81    135,145     162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 157 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(36, 36);
  /*
                                                     31

                  3,15,19                                                    65,80,135,162

  0,2    3,9,12    15,16   19,25,26                       31,34    65,71    80,81    135,145     162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 36 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(81, 81);
  /*
                                                     31

                  3,15,19                                                    65,80,162

  0,2    3,9,12    15,16   19,25,26                       31,34    65,71    80,135,145     162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 81 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(2, 2);
  /*
                                            31

                9,15,19                                            65,80,162

  0,3    9,12    15,16   19,25,26                       31,34    65,71    80,135,145     162,164,178
  */
  std::cerr << "---------------------------------- Post deleting 2 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(164, 164);
  /*
                                            31

                9,15,19                                            65,80,162

  0,3    9,12    15,16   19,25,26                       31,34    65,71    80,135,145     162,178
  */
  std::cerr << "---------------------------------- Post deleting 164 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(3, 3);
  /*
                                     31

            15,19                                            65,80,162

  0,9,12    15,16   19,25,26                       31,34    65,71    80,135,145     162,178
  */
  std::cerr << "---------------------------------- Post deleting 3 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(34, 34);
  /*
                                     31

            15,19                                            80,162

  0,9,12    15,16   19,25,26                       31,65,71    80,135,145     162,178
  */
  std::cerr << "---------------------------------- Post deleting 34 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(135, 135);
  /*
                                     31

            15,19                                            80,162

  0,9,12    15,16   19,25,26                       31,65,71    80,145     162,178
  */
  std::cerr << "---------------------------------- Post deleting 135 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(26, 26);
  /*
                                     31

            15,19                                          80,162

  0,9,12    15,16   19,25                       31,65,71    80,145     162,178
  */
  std::cerr << "---------------------------------- Post deleting 26 ---------------------\n";
  if (tree->root_ == nullptr) std::cerr << "root is null\n";
  PrintNode(tree->root_);

  tree->Delete(178, 178);
  /*
                          15,19,31,80

  0,9,12    15,16     19,25     31,65,71    80,145,162
  */
  std::cerr << "---------------------------------- Post deleting 178 ---------------------\n";
  PrintTree(tree);

  tree->Delete(162, 162);
  /*
                          15,19,31,80

  0,9,12    15,16     19,25     31,65,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 162 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(0, 0);
  /*
                          15,19,31,80

  9,12    15,16     19,25     31,65,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 0 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(16, 16);
  /*
                          15,31,80

  9,12    15,19,25     31,65,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 16 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(19, 19);
  /*
                15,31,80

  9,12    15,25     31,65,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 19 ---------------------\n";
  PrintNode(tree->root_);
  
  tree->Delete(65, 65);
  /*
                15,31,80

  9,12    15,25     31,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 65 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(12, 12);
  /*
                31,80

  9,15,25     31,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 12 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(15, 15);
  /*
                31,80

  9,25     31,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 15 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(9, 9);
  /*
          80

  25,31,71    80,145
  */
  std::cerr << "---------------------------------- Post deleting 9 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(80, 80);
  /*
  25,31,71,145
  */
  std::cerr << "---------------------------------- Post deleting 80 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(25, 25);
  /*
  31,71,145
  */
  std::cerr << "---------------------------------- Post deleting 25 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(145, 145);
  /*
  31,71
  */
  std::cerr << "---------------------------------- Post deleting 145 ---------------------\n";
  PrintNode(tree->root_);

  tree->Delete(31, 31);
  /*
  71
  */
  std::cerr << "---------------------------------- Post deleting 31 ---------------------\n";
  PrintNode(tree->root_);
  tree->Insert(71, 71);
  tree->Delete(71, 71);
  /*
  */
  std::cerr << "---------------------------------- Post deleting 71 ---------------------\n";
  PrintNode(tree->root_);
  tree->Delete(71, 71);
  

  EXPECT_EQ(tree->root_->size_, 0);
  EXPECT_EQ(tree->root_->IsLeaf(), true);
  EXPECT_EQ(tree->root_->value_list_, nullptr);



  // PrintTree(tree);
  delete tree;
}

TEST_F(BPlusTreeTests, NaiveSequentialScanTest) {
  const uint32_t key_num = 32;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);
  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
    tree->Insert(keys[i], keys[i]);
  }
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], &results);
    EXPECT_EQ(results, std::vector<int64_t>(1, keys[i]));
  }

  delete tree;
}

TEST_F(BPlusTreeTests, SequentialScanTest) {
  const uint32_t key_num = 1024;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);
  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
    tree->Insert(keys[i], keys[i]);
  }
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], &results);
    EXPECT_EQ(results, std::vector<int64_t>(1, keys[i]));
  }
  delete tree;
}

TEST_F(BPlusTreeTests, NaiveDuplicateScanTest) {
  const uint32_t key_num = 32;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

  std::vector<int64_t> keys;
  std::vector<int64_t> results;
  keys.reserve(key_num);

  for (int64_t i = 0; i < key_num; i++) {
    keys.emplace_back(i);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
    tree->Insert(keys[i], keys[i]);
  }
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], &results);
    EXPECT_EQ(results, std::vector<int64_t>(3, keys[i]));
  }

  delete tree;
}

TEST_F(BPlusTreeTests, DuplicateScanTest) {
  const uint32_t key_num = 1024;
  auto tree = new terrier::storage::index::BPlusTree<int64_t, int64_t>(2);

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
  for (int64_t i = 0; i < key_num; i++) {
    tree->GetValue(keys[i], &results);
    EXPECT_EQ(results, std::vector<int64_t>(5, keys[i]));
  }

  delete tree;
}
}  // namespace terrier::storage::index
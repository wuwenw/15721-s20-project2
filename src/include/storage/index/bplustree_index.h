#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "storage/index/bplustree.h"
#include "storage/index/index.h"
#include "storage/index/index_defs.h"
#include "transaction/deferred_action_manager.h"
#include "transaction/transaction_context.h"
#include "transaction/transaction_manager.h"

namespace terrier::storage::index {
template <uint8_t KeySize>
class CompactIntsKey;
template <uint16_t KeySize>
class GenericKey;

/**
 * Wrapper around 15-721 Project 2's B+Tree
 * @tparam KeyType the type of keys stored in the BPlusTree
 */
template <typename KeyType>
class BPlusTreeIndex final : public Index {
  friend class IndexBuilder;

 private:
  explicit BPlusTreeIndex(IndexMetadata metadata)
      : Index(std::move(metadata)), bplustree_{new BPlusTree<KeyType, TupleSlot>} {}

  const std::unique_ptr<BPlusTree<KeyType, TupleSlot>> bplustree_;

 public:
  /**
   * Return index type
   * @return index type
   */
  IndexType Type() const final { return IndexType::BPLUSTREE; }

  /**
   * perform GC
   */
  void PerformGarbageCollection() final {
    // FIXME(15-721 project2): invoke garbage collection on the underlying data structure
  }

  /**
   * Get usage of heap
   * @return size of used heap
   */
  size_t GetHeapUsage() const final {
    // FIXME(15-721 project2): access the underlying data structure and report the heap usage
    return bplustree_->GetHeapUsage();
  }

  /**
   * Insert api
   * @param txn transaction
   * @param tuple key information
   * @param location value to be inserted
   * @return false if fail
   */
  bool Insert(const common::ManagedPointer<transaction::TransactionContext> txn, const ProjectedRow &tuple,
              const TupleSlot location) final {
    TERRIER_ASSERT(!(metadata_.GetSchema().Unique()),
                   "This Insert is designed for secondary indexes with no uniqueness constraints.");
    KeyType index_key;
    index_key.SetFromProjectedRow(tuple, metadata_, metadata_.GetSchema().GetColumns().size());
    const bool result = bplustree_->Insert(index_key, location, true);

    TERRIER_ASSERT(
        result,
        "non-unique index shouldn't fail to insert. If it did, something went wrong deep inside the BPlusTree itself.");
    // Register an abort action with the txn context in case of rollback
    txn->RegisterAbortAction([=]() {
      const bool UNUSED_ATTRIBUTE result = bplustree_->Delete(index_key, location);
      // bplustree_->Delete(index_key, location);
      TERRIER_ASSERT(result, "Delete on the index failed.");
    });
    return result;
  }

  /**
   * Insert uniqe api
   * @param txn transaction
   * @param tuple key information
   * @param location value to be inserted
   * @return false if fail
   */
  bool InsertUnique(const common::ManagedPointer<transaction::TransactionContext> txn, const ProjectedRow &tuple,
                    const TupleSlot location) final {
    TERRIER_ASSERT(metadata_.GetSchema().Unique(), "This Insert is designed for indexes with uniqueness constraints.");
    KeyType index_key;
    index_key.SetFromProjectedRow(tuple, metadata_, metadata_.GetSchema().GetColumns().size());
    bool predicate_satisfied UNUSED_ATTRIBUTE = false;

    // The predicate checks if any matching keys have write-write conflicts or are still visible to the calling txn.
    auto predicate UNUSED_ATTRIBUTE = [txn](const TupleSlot slot) -> bool {
      const auto *const data_table = slot.GetBlock()->data_table_;
      const auto has_conflict = data_table->HasConflict(*txn, slot);
      const auto is_visible = data_table->IsVisible(*txn, slot);
      return has_conflict || is_visible;
    };
    bool result = bplustree_->InsertUnique(index_key, location, predicate, &predicate_satisfied);

    TERRIER_ASSERT(predicate_satisfied != result, "If predicate is not satisfied then insertion should succeed.");

    if (result) {
      // Register an abort action with the txn context in case of rollback
      txn->RegisterAbortAction([=]() {
        const bool UNUSED_ATTRIBUTE result = bplustree_->Delete(index_key, location);
        // bplustree_->Delete(index_key, location);
        TERRIER_ASSERT(result, "Delete on the index failed.");
      });
    } else {
      // Presumably you've already made modifications to a DataTable (the source of the TupleSlot argument to this
      // function) however, the index found a constraint violation and cannot allow that operation to succeed. For MVCC
      // correctness, this txn must now abort for the GC to clean up the version chain in the DataTable correctly.
      txn->SetMustAbort();
    }

    return result;
  }

  /**
   * Delete api
   * @param txn transaction
   * @param tuple key information
   * @param location value to be inserted
   */
  void Delete(const common::ManagedPointer<transaction::TransactionContext> txn, const ProjectedRow &tuple,
              const TupleSlot location) final {
    KeyType index_key;
    index_key.SetFromProjectedRow(tuple, metadata_, metadata_.GetSchema().GetColumns().size());

    TERRIER_ASSERT(!(location.GetBlock()->data_table_->HasConflict(*txn, location)) &&
                       !(location.GetBlock()->data_table_->IsVisible(*txn, location)),
                   "Called index delete on a TupleSlot that has a conflict with this txn or is still visible.");

    // Register a deferred action for the GC with txn manager. See base function comment.
    txn->RegisterCommitAction([=](transaction::DeferredActionManager *deferred_action_manager) {
      deferred_action_manager->RegisterDeferredAction([=]() {
        const bool UNUSED_ATTRIBUTE result = bplustree_->Delete(index_key, location);
        TERRIER_ASSERT(result, "Deferred delete on the index failed.");
      });
    });
  }

  /**
   * Scan api
   * @param txn transaction
   * @param key key
   * @param value_list values
   */
  void ScanKey(const transaction::TransactionContext &txn, const ProjectedRow &key,
               std::vector<TupleSlot> *value_list) final {
    TERRIER_ASSERT(value_list->empty(), "Result set should begin empty.");

    std::vector<TupleSlot> results;

    // Build search key
    KeyType index_key;
    index_key.SetFromProjectedRow(key, metadata_, metadata_.GetSchema().GetColumns().size());

    // Perform lookup in BPlusTree
    // FIXME(15-721 project2): perform a lookup of the underlying data structure of the key
    bplustree_->GetValue(index_key, &results);
    // Avoid resizing our value_list, even if it means over-provisioning
    value_list->reserve(results.size());

    // Perform visibility check on result
    for (const auto &result : results) {
      if (IsVisible(txn, result)) value_list->emplace_back(result);
      if (metadata_.GetSchema().Unique()) break;
    }

    TERRIER_ASSERT(!(metadata_.GetSchema().Unique()) || (metadata_.GetSchema().Unique() && value_list->size() <= 1),
                   "Invalid number of results for unique index.");
  }

  /**
   * ScanAscending api
   * @param txn transaction
   * @param scan_type type of scan
   * @param num_attrs number of attributes
   * @param low_key lower key
   * @param high_key higher key
   * @param limit limit of results
   * @param value_list values
   */
  void ScanAscending(const transaction::TransactionContext &txn, ScanType scan_type, uint32_t num_attrs,
                     ProjectedRow *low_key, ProjectedRow *high_key, uint32_t limit,
                     std::vector<TupleSlot> *value_list) final {
    TERRIER_ASSERT(value_list->empty(), "Result set should begin empty.");
    TERRIER_ASSERT(scan_type == ScanType::Closed || scan_type == ScanType::OpenLow || scan_type == ScanType::OpenHigh ||
                       scan_type == ScanType::OpenBoth,
                   "Invalid scan_type passed into BPlusTreeIndex::Scan");

    bool low_key_exists = (scan_type == ScanType::Closed || scan_type == ScanType::OpenHigh);
    bool high_key_exists = (scan_type == ScanType::Closed || scan_type == ScanType::OpenLow);

    // Build search keys
    KeyType index_low_key, index_high_key;
    if (low_key_exists) index_low_key.SetFromProjectedRow(*low_key, metadata_, num_attrs);
    if (high_key_exists) index_high_key.SetFromProjectedRow(*high_key, metadata_, num_attrs);

    // FIXME(15-721 project2): perform a lookup of the underlying data structure of the key
    std::vector<TupleSlot> results;
    bplustree_->GetValueAscending(index_low_key, index_high_key, &results, limit);
    value_list->reserve(results.size());
    for (const auto &result : results) {
      if (IsVisible(txn, result)) value_list->emplace_back(result);
    }
  }

  /**
   * ScanDescending api
   * @param txn transaction
   * @param low_key lower key
   * @param high_key higher key
   * @param value_list values
   */
  void ScanDescending(const transaction::TransactionContext &txn, const ProjectedRow &low_key,
                      const ProjectedRow &high_key, std::vector<TupleSlot> *value_list) final {
    TERRIER_ASSERT(value_list->empty(), "Result set should begin empty.");

    // Build search keys
    KeyType index_low_key, index_high_key;
    index_low_key.SetFromProjectedRow(low_key, metadata_, metadata_.GetSchema().GetColumns().size());
    index_high_key.SetFromProjectedRow(high_key, metadata_, metadata_.GetSchema().GetColumns().size());

    // FIXME(15-721 project2): perform a lookup of the underlying data structure of the key
    std::vector<TupleSlot> results;
    bplustree_->GetValueDescendingLimited(index_low_key, index_high_key, &results, 0);
    value_list->reserve(results.size());
    for (const auto &result : results) {
      if (IsVisible(txn, result)) value_list->emplace_back(result);
    }
  }

  /**
   * ScanLimitDescending api
   * @param txn transaction
   * @param low_key lower key
   * @param high_key higher key
   * @param value_list values
   * @param limit limit of results
   */
  void ScanLimitDescending(const transaction::TransactionContext &txn, const ProjectedRow &low_key,
                           const ProjectedRow &high_key, std::vector<TupleSlot> *value_list,
                           const uint32_t limit) final {
    TERRIER_ASSERT(value_list->empty(), "Result set should begin empty.");
    TERRIER_ASSERT(limit > 0, "Limit must be greater than 0.");

    // Build search keys
    KeyType index_low_key, index_high_key;
    index_low_key.SetFromProjectedRow(low_key, metadata_, metadata_.GetSchema().GetColumns().size());
    index_high_key.SetFromProjectedRow(high_key, metadata_, metadata_.GetSchema().GetColumns().size());

    // FIXME(15-721 project2): perform a lookup of the underlying data structure of the key
    std::vector<TupleSlot> results;
    bplustree_->GetValueDescendingLimited(index_low_key, index_high_key, &results, limit);
    value_list->reserve(results.size());
    for (const auto &result : results) {
      if (IsVisible(txn, result)) value_list->emplace_back(result);
    }
  }
};

extern template class BPlusTreeIndex<CompactIntsKey<8>>;
extern template class BPlusTreeIndex<CompactIntsKey<16>>;
extern template class BPlusTreeIndex<CompactIntsKey<24>>;
extern template class BPlusTreeIndex<CompactIntsKey<32>>;

extern template class BPlusTreeIndex<GenericKey<64>>;
extern template class BPlusTreeIndex<GenericKey<128>>;
extern template class BPlusTreeIndex<GenericKey<256>>;

}  // namespace terrier::storage::index

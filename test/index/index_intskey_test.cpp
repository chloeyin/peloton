//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// index_test.cpp
//
// Identification: test/index/index_test.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "common/harness.h"
#include "gtest/gtest.h"

#include "common/logger.h"
#include "common/platform.h"
#include "common/timer.h"
#include "index/index_factory.h"
#include "storage/tuple.h"

namespace peloton {
namespace test {

//===--------------------------------------------------------------------===//
// Index IntsKey Tests
//===--------------------------------------------------------------------===//

class IndexIntsKeyTests : public PelotonTest {};

catalog::Schema *key_schema = nullptr;
catalog::Schema *tuple_schema = nullptr;

// You can't set this too large because we will
// have duplicates for the TINYINT keys
const int NUM_TUPLES = 128;

/*
 * BuildIndex()
 */
index::Index *BuildIndex(IndexType index_type, const bool unique_keys,
                         std::vector<type::Type::TypeId> col_types) {
  // Build tuple and key schema
  std::vector<catalog::Column> column_list;
  std::vector<oid_t> key_attrs;

  const int num_cols = (int)col_types.size();
  const char column_char = 'A';
  for (int i = 0; i < num_cols; i++) {
    std::ostringstream os;
    os << static_cast<char>((int)column_char + i);

    catalog::Column column(col_types[i], type::Type::GetTypeSize(col_types[i]),
                           os.str(), true);
    column_list.push_back(column);
    key_attrs.push_back(i);
  }  // FOR
  key_schema = new catalog::Schema(column_list);
  key_schema->SetIndexedColumns(key_attrs);
  tuple_schema = new catalog::Schema(column_list);

  // Build index metadata
  index::IndexMetadata *index_metadata = new index::IndexMetadata(
      "MAGIC_TEST_INDEX", 125,  // Index oid
      INVALID_OID, INVALID_OID, index_type, IndexConstraintType::DEFAULT,
      tuple_schema, key_schema, key_attrs, unique_keys);

  // Build index
  // The type of index key has been chosen inside this function, but we are
  // unable to know the exact type of key it has chosen
  index::Index *index = index::IndexFactory::GetIndex(index_metadata);

  // TODO: I don't think there is an easy way to check what kind of key we got.
  // It would be nice if we could check for an CompactIntsKey

  // Actually this will never be hit since if index creation fails an exception
  // would be raised (maybe out of memory would result in a nullptr? Anyway
  // leave it here)
  EXPECT_TRUE(index != NULL);

  return index;
}

void IndexIntsKeyTestHelper(IndexType index_type,
                            std::vector<type::Type::TypeId> col_types) {
  auto pool = TestingHarness::GetInstance().GetTestingPool();
  std::vector<ItemPointer *> location_ptrs;

  // CREATE
  std::unique_ptr<index::Index> index(BuildIndex(index_type, true, col_types));

  // POPULATE
  std::vector<std::shared_ptr<storage::Tuple>> keys;
  std::vector<std::shared_ptr<ItemPointer>> items;

#ifdef LOG_TRACE_ENABLED
  Timer<std::milli> timer;
  timer.Start();
#endif

  for (int i = 0; i < NUM_TUPLES; i++) {
    std::shared_ptr<storage::Tuple> key(new storage::Tuple(key_schema, true));
    std::shared_ptr<ItemPointer> item(new ItemPointer(i, i * i));

    for (int col_idx = 0; col_idx < (int)col_types.size(); col_idx++) {
      int val = i;
      switch (col_types[col_idx]) {
        case type::Type::TINYINT: {
          val = val % 128;
          key->SetValue(col_idx, type::ValueFactory::GetTinyIntValue(val),
                        pool);
          break;
        }
        case type::Type::SMALLINT: {
          key->SetValue(col_idx, type::ValueFactory::GetSmallIntValue(val),
                        pool);
          break;
        }
        case type::Type::INTEGER: {
          key->SetValue(col_idx, type::ValueFactory::GetIntegerValue(val),
                        pool);
          break;
        }
        case type::Type::BIGINT: {
          key->SetValue(col_idx, type::ValueFactory::GetBigIntValue(val), pool);
          break;
        }
        default:
          throw peloton::Exception("Unexpected type!");
      }
    }

    // INSERT
    index->InsertEntry(key.get(), item.get());

    keys.push_back(key);
    items.push_back(item);
  }  // FOR
#ifdef LOG_TRACE_ENABLED
  timer.Stop();
  LOG_INFO("%s<%d Keys> Insert: Duration = %.2lf",
           IndexTypeToString(index_type).c_str(), (int)col_types.size(),
           timer.GetDuration());
  timer.Reset();
#endif

// SCAN
#ifdef LOG_TRACE_ENABLED
  timer.Start();
#endif
  for (int i = 0; i < NUM_TUPLES; i++) {
    location_ptrs.clear();
    index->ScanKey(keys[i].get(), location_ptrs);
    EXPECT_EQ(location_ptrs.size(), 1);
    EXPECT_EQ(location_ptrs[0]->block, items[i]->block);
  }  // FOR
#ifdef LOG_TRACE_ENABLED
  timer.Stop();
  LOG_INFO("%s<%d Keys> Scan: Duration = %.2lf",
           IndexTypeToString(index_type).c_str(), (int)col_types.size(),
           timer.GetDuration());
  timer.Reset();
#endif

// DELETE
#ifdef LOG_TRACE_ENABLED
  timer.Start();
#endif
  for (int i = 0; i < NUM_TUPLES; i++) {
    index->DeleteEntry(keys[i].get(), items[i].get());
    location_ptrs.clear();
    index->ScanKey(keys[i].get(), location_ptrs);
    EXPECT_EQ(0, location_ptrs.size());
  }  // FOR
#ifdef LOG_TRACE_ENABLED
  timer.Stop();
  LOG_INFO("%s<%d Keys> Delete: Duration = %.2lf",
           IndexTypeToString(index_type).c_str(), (int)col_types.size(),
           timer.GetDuration());
#endif

  delete tuple_schema;
}

// TEST_F(IndexIntsKeyTests, SpeedTest) {
//  std::vector<type::Type::TypeId> col_types = {
//      type::Type::INTEGER, type::Type::INTEGER, type::Type::INTEGER,
//  };
//  IndexIntsKeyTestHelper(IndexType::BWTREE, col_types);
//}

TEST_F(IndexIntsKeyTests, BwTreeTest) {
  std::vector<type::Type::TypeId> types = {
      type::Type::BIGINT, type::Type::INTEGER, type::Type::SMALLINT,
      type::Type::TINYINT};

  // ONE COLUMN
  for (type::Type::TypeId type0 : types) {
    std::vector<type::Type::TypeId> col_types = {type0};
    IndexIntsKeyTestHelper(IndexType::BWTREE, col_types);
  }
  // TWO COLUMNS
  for (type::Type::TypeId type0 : types) {
    for (type::Type::TypeId type1 : types) {
      std::vector<type::Type::TypeId> col_types = {type0, type1};
      IndexIntsKeyTestHelper(IndexType::BWTREE, col_types);
    }
  }
  // THREE COLUMNS
  for (type::Type::TypeId type0 : types) {
    for (type::Type::TypeId type1 : types) {
      for (type::Type::TypeId type2 : types) {
        std::vector<type::Type::TypeId> col_types = {type0, type1, type2};
        IndexIntsKeyTestHelper(IndexType::BWTREE, col_types);
      }
    }
  }
  // FOUR COLUMNS
  for (type::Type::TypeId type0 : types) {
    for (type::Type::TypeId type1 : types) {
      for (type::Type::TypeId type2 : types) {
        for (type::Type::TypeId type3 : types) {
          std::vector<type::Type::TypeId> col_types = {type0, type1, type2,
                                                       type3};
          IndexIntsKeyTestHelper(IndexType::BWTREE, col_types);
        }
      }
    }
  }
}

// FIXME: The B-Tree core dumps. If we're not going to support then we should
// probably drop it.
// TEST_F(IndexIntsKeyTests, BTreeTest) {
// IndexIntsKeyHelper(IndexType::BTREE);
// }

}  // End test namespace
}  // End peloton namespace

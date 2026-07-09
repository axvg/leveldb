// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/learned_index.h"

#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "db/dbformat.h"
#include "leveldb/comparator.h"
#include "leveldb/iterator.h"
#include "gtest/gtest.h"

namespace leveldb {

class VectorIterator : public Iterator {
 public:
  VectorIterator(std::vector<std::string> keys, std::vector<std::string> values)
      : keys_(std::move(keys)), values_(std::move(values)), pos_(0) {}
  bool Valid() const override { return pos_ < keys_.size(); }
  void SeekToFirst() override { pos_ = 0; }
  void SeekToLast() override { pos_ = keys_.empty() ? 0 : keys_.size() - 1; }
  void Seek(const Slice&) override { pos_ = 0; }
  void Next() override { pos_++; }
  void Prev() override { pos_--; }
  Slice key() const override { return keys_[pos_]; }
  Slice value() const override { return values_[pos_]; }
  Status status() const override { return Status::OK(); }

 private:
  std::vector<std::string> keys_;
  std::vector<std::string> values_;
  size_t pos_;
};

namespace {

std::string MakeInternalKey(long value, uint64_t seq) {
  char user[32];
  std::snprintf(user, sizeof(user), "%016ld", value);
  InternalKey ikey(Slice(user, 16), seq, kTypeValue);
  return ikey.Encode().ToString();
}

// primer i con seps[i] >= query, es para comparar
int RefLowerBound(const std::vector<std::string>& seps, const Comparator* cmp,
                  const Slice& query) {
  int i = 0;
  while (i < static_cast<int>(seps.size()) &&
         cmp->Compare(Slice(seps[i]), query) < 0) {
    i++;
  }
  return i;
}

}  // namespace

class LearnedIndexTest : public testing::Test {
 protected:
  InternalKeyComparator icmp_{BytewiseComparator()};
};

// mismo bloque que la busqueda binaria, para todo epsilon
TEST_F(LearnedIndexTest, MatchesBinarySearch) {
  std::mt19937 rng(123);
  std::uniform_int_distribution<int> gap(1, 20);

  // separadores crecientes, uno por bloque
  const int kBlocks = 2000;
  std::vector<std::string> seps;
  std::vector<std::string> handles;
  std::vector<long> values;
  long v = 100;
  for (int i = 0; i < kBlocks; i++) {
    v += gap(rng);
    values.push_back(v);
    seps.push_back(MakeInternalKey(v, 1000));
    handles.push_back(std::to_string(i));
  }

  for (int error : {0, 1, 4, 16, 64, 256}) {
    VectorIterator it(seps, handles);
    LearnedIndex li;
    li.Build(&it, &icmp_, error);
    ASSERT_TRUE(li.ok());
    ASSERT_EQ(li.num_blocks(), static_cast<size_t>(kBlocks));

    // separadores y las llaves de al lado (+-1)
    for (int i = 0; i < kBlocks; i++) {
      for (long q : {values[i], values[i] - 1, values[i] + 1}) {
        std::string qk = MakeInternalKey(q, 500);
        int ref = RefLowerBound(seps, &icmp_, qk);
        Slice handle;
        bool got = li.Lookup(qk, &handle);
        if (ref >= kBlocks) {
          ASSERT_FALSE(got) << "q=" << q << " should be past last block";
        } else {
          ASSERT_TRUE(got) << "q=" << q << " error=" << error;
          ASSERT_EQ(handle.ToString(), std::to_string(ref))
              << "q=" << q << " error=" << error;
        }
      }
    }
  }
}

TEST_F(LearnedIndexTest, DisabledOnNonIntegerKey) {
  std::vector<std::string> seps, handles;
  InternalKey a(Slice("apple"), 1, kTypeValue);
  InternalKey b(Slice("banana"), 1, kTypeValue);
  seps.push_back(a.Encode().ToString());
  seps.push_back(b.Encode().ToString());
  handles.push_back("0");
  handles.push_back("1");

  VectorIterator it(seps, handles);
  LearnedIndex li;
  li.Build(&it, &icmp_, 8);
  ASSERT_FALSE(li.ok());

  Slice handle;
  std::string q = MakeInternalKey(5, 1);
  ASSERT_FALSE(li.Lookup(q, &handle));
}

}  // namespace leveldb

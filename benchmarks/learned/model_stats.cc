// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "db/dbformat.h"
#include "leveldb/comparator.h"
#include "leveldb/iterator.h"
#include "table/learned_index.h"

namespace leveldb {

class VecIter : public Iterator {
 public:
  VecIter(std::vector<std::string> k, std::vector<std::string> v)
      : k_(std::move(k)), v_(std::move(v)), pos_(0) {}
  bool Valid() const override { return pos_ < k_.size(); }
  void SeekToFirst() override { pos_ = 0; }
  void SeekToLast() override { pos_ = k_.empty() ? 0 : k_.size() - 1; }
  void Seek(const Slice&) override { pos_ = 0; }
  void Next() override { pos_++; }
  void Prev() override { pos_--; }
  Slice key() const override { return k_[pos_]; }
  Slice value() const override { return v_[pos_]; }
  Status status() const override { return Status::OK(); }

 private:
  std::vector<std::string> k_, v_;
  size_t pos_;
};

}  // namespace leveldb

int main() {
  using namespace leveldb;
  InternalKeyComparator icmp(BytewiseComparator());

  // una sstable de ~2 MB tiene ~500 bloques, probamos tamanos cercanos
  const int block_counts[] = {250, 500, 1000, 2000};
  const int epsilons[] = {2, 4, 8, 16, 32, 64, 128};

  const char* dists[] = {"uniform", "skewed"};

  std::printf("dist,blocks,epsilon,segments,model_bytes\n");
  for (const char* dist : dists) {
    for (int blocks : block_counts) {
      std::vector<std::string> keys, handles;
      long v = 0;
      for (int i = 0; i < blocks; i++) {
        if (std::string(dist) == "uniform") {
          v += 10;                  // espaciado constante, un solo segmento
        } else {
          v += 1 + (long)i * i / 50;  // cdf cuadratica, varios segmentos
        }
        char user[32];
        std::snprintf(user, sizeof(user), "%016ld", v);
        InternalKey ikey(Slice(user, 16), 1000, kTypeValue);
        keys.push_back(ikey.Encode().ToString());
        handles.push_back(std::to_string(i));
      }
      for (int eps : epsilons) {
        VecIter it(keys, handles);
        LearnedIndex li;
        li.Build(&it, &icmp, eps);
        std::printf("%s,%d,%d,%zu,%zu\n", dist, blocks, eps,
                    li.num_segments(), li.ModelSizeBytes());
      }
    }
  }
  return 0;
}

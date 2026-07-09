// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_LEARNED_INDEX_H_
#define STORAGE_LEVELDB_TABLE_LEARNED_INDEX_H_

#include <string>
#include <vector>

#include "leveldb/slice.h"
#include "table/plr.h"

namespace leveldb {

class Comparator;
class Iterator;

class LearnedIndex {
 public:
  LearnedIndex() : cmp_(nullptr), ok_(false), error_(0) {}

  // con una llave rara el modelo queda apagado (ok() da false)
  void Build(Iterator* index_iter, const Comparator* cmp, int error);

  bool ok() const { return ok_; }

  // false si no aplica o la llave pasa el ultimo bloque
  bool Lookup(const Slice& ikey, Slice* handle) const;

  size_t num_segments() const { return model_.num_segments(); }
  size_t num_blocks() const { return handles_.size(); }
  size_t ModelSizeBytes() const {
    return model_.num_segments() * sizeof(PLRSegment);
  }

 private:
  // entero a double, mantiene el orden
  static bool ParseKey(const Slice& user_key, double* out);

  int LowerBound(int begin, int end, const Slice& ikey) const;

  const Comparator* cmp_;
  bool ok_;
  int error_;
  std::vector<std::string> sep_keys_;
  std::vector<std::string> handles_;
  PLRModel model_;  // llave -> indice de bloque
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_LEARNED_INDEX_H_

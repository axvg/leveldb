// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/learned_index.h"

#include <algorithm>
#include <cstdint>

#include "leveldb/comparator.h"
#include "leveldb/iterator.h"

namespace leveldb {

// db_bench usa %016d, normalizamos a 16
static const int kKeyWidth = 16;

bool LearnedIndex::ParseKey(const Slice& user_key, double* out) {
  const size_t d = user_key.size();
  if (d == 0 || d > kKeyWidth) return false;
  uint64_t v = 0;
  for (size_t i = 0; i < d; i++) {
    char c = user_key[i];
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<uint64_t>(c - '0');
  }
  for (int i = static_cast<int>(d); i < kKeyWidth; i++) v *= 10;  // rellena con ceros a la derecha
  *out = static_cast<double>(v);
  return true;
}

void LearnedIndex::Build(Iterator* index_iter, const Comparator* cmp,
                         int error) {
  ok_ = false;
  cmp_ = cmp;
  error_ = error;
  sep_keys_.clear();
  handles_.clear();
  std::vector<double> xs;
  std::vector<double> ys;

  size_t idx = 0;
  for (index_iter->SeekToFirst(); index_iter->Valid(); index_iter->Next()) {
    Slice ikey = index_iter->key();
    if (ikey.size() < 8) return;
    Slice ukey(ikey.data(), ikey.size() - 8);
    double x;
    if (!ParseKey(ukey, &x)) return;  // cualquier return de aca deja ok_ en false
    if (!xs.empty() && x <= xs.back()) return;
    sep_keys_.push_back(ikey.ToString());
    handles_.push_back(index_iter->value().ToString());
    xs.push_back(x);
    ys.push_back(static_cast<double>(idx));
    idx++;
  }
  if (!index_iter->status().ok() || sep_keys_.empty()) return;

  model_.Build(xs, ys, static_cast<double>(error));
  ok_ = true;
}

int LearnedIndex::LowerBound(int begin, int end, const Slice& ikey) const {
  int lo = begin, hi = end;
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (cmp_->Compare(Slice(sep_keys_[mid]), ikey) < 0) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

bool LearnedIndex::Lookup(const Slice& ikey, Slice* handle) const {
  if (!ok_ || ikey.size() < 8) return false;
  Slice ukey(ikey.data(), ikey.size() - 8);
  double kx;
  if (!ParseKey(ukey, &kx)) return false;

  const int n = static_cast<int>(sep_keys_.size());
  double predf = model_.Predict(kx);
  int pred = static_cast<int>(predf + 0.5);
  if (pred < 0) pred = 0;
  if (pred >= n) pred = n - 1;

  int lo = pred - error_ - 1;
  int hi = pred + error_ + 1;
  if (lo < 0) lo = 0;
  if (hi > n - 1) hi = n - 1;

  // igual que Seek pero dentro de la ventana, si falla se amplia
  int idx;
  if (cmp_->Compare(Slice(sep_keys_[hi]), ikey) < 0) {
    idx = LowerBound(hi + 1, n, ikey);
  } else if (cmp_->Compare(Slice(sep_keys_[lo]), ikey) >= 0) {
    idx = LowerBound(0, lo + 1, ikey);
  } else {
    idx = LowerBound(lo + 1, hi + 1, ikey);
  }

  if (idx >= n) return false;  // la llave pasa el ultimo bloque
  *handle = Slice(handles_[idx]);
  return true;
}

}  // namespace leveldb

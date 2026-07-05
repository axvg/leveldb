// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_PLR_H_
#define STORAGE_LEVELDB_TABLE_PLR_H_

#include <cstddef>
#include <vector>

namespace leveldb {

struct PLRSegment {
  double x_start;
  double slope;
  double intercept;
};

class PLRModel {
 public:
  PLRModel() : error_(0.0) {}
  // ojo, xs va en aumento estricto
  void Build(const std::vector<double>& xs, const std::vector<double>& ys,
             double error);

  double Predict(double x) const;

  double error() const { return error_; }
  size_t num_segments() const { return segments_.size(); }
  const std::vector<PLRSegment>& segments() const { return segments_; }

 private:
  std::vector<PLRSegment> segments_;
  double error_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_PLR_H_

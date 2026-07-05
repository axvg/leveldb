// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/plr.h"

#include <algorithm>
#include <cassert>

namespace leveldb {

namespace {

struct Point {
  double x, y;
};

double Slope(const Point& a, const Point& b) {
  return (b.y - a.y) / (b.x - a.x);
}

Point Intersection(const Point& p1, double s1, const Point& p2, double s2) {
  double x = (s1 * p1.x - s2 * p2.x + p2.y - p1.y) / (s1 - s2);
  double y = p1.y + s1 * (x - p1.x);
  return {x, y};
}

// cono de pendientes, como bourbon / xie
class GreedyPLR {
 public:
  GreedyPLR(double error, std::vector<PLRSegment>* out)
      : error_(error), out_(out), state_(kNeed2) {}

  void Process(const Point& p) {
    switch (state_) {
      case kNeed2:
        s0_ = p;
        state_ = kNeed1;
        break;
      case kNeed1:
        s1_ = p;
        Setup();
        state_ = kReady;
        break;
      case kReady:
        if (!InCone(p)) {
          Commit();
          s0_ = p;
          state_ = kNeed1;
        } else {
          Shrink(p);
        }
        break;
    }
  }

  void Finish() {
    if (state_ == kReady) {
      Commit();
    } else if (state_ == kNeed1) {
      out_->push_back({s0_.x, 0.0, s0_.y});
    }
  }

 private:
  enum State { kNeed2, kNeed1, kReady };

  void Setup() {
    Point s0_low = {s0_.x, s0_.y - error_};
    Point s0_high = {s0_.x, s0_.y + error_};
    rho_upper_ = Slope(s0_low, {s1_.x, s1_.y + error_});
    rho_lower_ = Slope(s0_high, {s1_.x, s1_.y - error_});
    if (rho_upper_ > rho_lower_) {
      sint_ = Intersection(s0_low, rho_upper_, s0_high, rho_lower_);
    } else {
      sint_ = s0_;  // error 0, cono degenerado y la recta pasa por s0
    }
  }

  bool InCone(const Point& p) const {
    double up = sint_.y + rho_upper_ * (p.x - sint_.x);
    double low = sint_.y + rho_lower_ * (p.x - sint_.x);
    return (p.y + error_ >= low) && (p.y - error_ <= up);
  }

  void Shrink(const Point& p) {
    double up_cand = Slope(sint_, {p.x, p.y + error_});
    double low_cand = Slope(sint_, {p.x, p.y - error_});
    rho_upper_ = std::min(rho_upper_, up_cand);
    rho_lower_ = std::max(rho_lower_, low_cand);
  }

  void Commit() {
    double slope = (rho_lower_ + rho_upper_) / 2.0;
    double intercept = sint_.y - slope * sint_.x;
    out_->push_back({s0_.x, slope, intercept});
  }

  double error_;
  std::vector<PLRSegment>* out_;
  State state_;
  Point s0_, s1_, sint_;
  double rho_lower_ = 0.0, rho_upper_ = 0.0;
};

}  // namespace

void PLRModel::Build(const std::vector<double>& xs,
                     const std::vector<double>& ys, double error) {
  assert(xs.size() == ys.size());
  assert(error >= 0.0);
  segments_.clear();
  error_ = error;
  GreedyPLR plr(error, &segments_);
  for (size_t i = 0; i < xs.size(); i++) {
    assert(i == 0 || xs[i] > xs[i - 1]);
    plr.Process({xs[i], ys[i]});
  }
  plr.Finish();
}

double PLRModel::Predict(double x) const {
  if (segments_.empty()) return 0.0;
  size_t lo = 0, hi = segments_.size();
  while (lo + 1 < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (segments_[mid].x_start <= x) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  const PLRSegment& s = segments_[lo];
  return s.slope * x + s.intercept;
}

}  // namespace leveldb

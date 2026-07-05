// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/plr.h"

#include <cmath>
#include <random>
#include <vector>

#include "gtest/gtest.h"

namespace leveldb {

static void CheckErrorBound(const std::vector<double>& xs, double error) {
  std::vector<double> ys(xs.size());
  for (size_t i = 0; i < xs.size(); i++) ys[i] = static_cast<double>(i);

  PLRModel model;
  model.Build(xs, ys, error);

  ASSERT_GT(model.num_segments(), 0u);
  for (size_t i = 0; i < xs.size(); i++) {
    double pred = model.Predict(xs[i]);
    ASSERT_LE(std::fabs(pred - ys[i]), error + 1e-6)
        << "x=" << xs[i] << " i=" << i << " pred=" << pred;
  }
}

TEST(PLRTest, Empty) {
  PLRModel model;
  model.Build({}, {}, 4.0);
  ASSERT_EQ(model.num_segments(), 0u);
  ASSERT_EQ(model.Predict(10.0), 0.0);
}

TEST(PLRTest, SinglePoint) {
  PLRModel model;
  model.Build({5.0}, {0.0}, 2.0);
  ASSERT_EQ(model.num_segments(), 1u);
  ASSERT_EQ(model.Predict(5.0), 0.0);
}

TEST(PLRTest, PerfectLineIsOneSegment) {
  // recta exacta y = 2x + 3: deberia salir un solo segmento
  std::vector<double> xs, ys;
  for (int i = 0; i < 1000; i++) {
    xs.push_back(i);
    ys.push_back(2.0 * i + 3.0);
  }
  PLRModel model;
  model.Build(xs, ys, 0.0);
  ASSERT_EQ(model.num_segments(), 1u);
  ASSERT_NEAR(model.Predict(500.0), 1003.0, 1e-6);
}

TEST(PLRTest, LinearKeysWithinBound) {
  std::vector<double> xs;
  for (int i = 0; i < 100000; i++) xs.push_back(i * 3.0);
  CheckErrorBound(xs, 8.0);
}

TEST(PLRTest, RandomIncreasingKeysWithinBound) {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> gap(1, 50);
  for (double error : {0.0, 1.0, 4.0, 16.0, 64.0}) {
    std::vector<double> xs;
    double x = 0;
    for (int i = 0; i < 50000; i++) {
      x += gap(rng);
      xs.push_back(x);
    }
    CheckErrorBound(xs, error);
  }
}

TEST(PLRTest, LargerErrorMeansFewerSegments) {
  std::mt19937 rng(7);
  std::normal_distribution<double> step(10.0, 5.0);
  std::vector<double> xs, ys;
  double x = 0;
  for (int i = 0; i < 20000; i++) {
    x += 1.0 + std::fabs(step(rng));  // con ruido pero creciente
    xs.push_back(x);
    ys.push_back(i);
  }
  PLRModel small, big;
  small.Build(xs, ys, 2.0);
  big.Build(xs, ys, 64.0);
  ASSERT_GE(small.num_segments(), big.num_segments());
}

}  // namespace leveldb

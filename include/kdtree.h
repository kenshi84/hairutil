#pragma once

#include <memory>
#include <Eigen/Core>
#include <nanoflann.hpp>

struct KdTreeSearchResult {
  std::vector<long> indices;
  std::vector<double> dists;
};

template <int N>
struct KdTree {
  Eigen::Matrix<double, -1, N> points;    // intentionally hold a copy

  using CoreType = nanoflann::KDTreeEigenMatrixAdaptor<Eigen::Matrix<double, -1, N>, N>;
  std::shared_ptr<CoreType> core;

  void build(int leaf_max_size = 10) {
    core = std::make_shared<CoreType>(N, points, leaf_max_size);
  }

  size_t knnSearch(const Eigen::Matrix<double, N, 1>& point, size_t k, std::vector<long>& indices, std::vector<double>& dists) const {
    indices.resize(k);
    dists.resize(k);
    size_t res = core->index_->knnSearch(point.data(), k, indices.data(), dists.data());
    indices.resize(res);
    dists.resize(res);
    for (double& d : dists)
      d = std::sqrt(d);           // distances returned are squared!
    return res;
  }

  size_t knnSearch(const Eigen::Matrix<double, N, 1>& point, size_t k, KdTreeSearchResult& res) const {
    return knnSearch(point, k, res.indices, res.dists);
  }

  size_t radiusSearch(const Eigen::Matrix<double, N, 1>& point, double radius, std::vector<long>& indices, std::vector<double>& dists) const {
    nanoflann::SearchParameters params;
    params.sorted = true;
    std::vector<nanoflann::ResultItem<long, double>> indicesDists;
    const double radius_sq = radius * radius;         // radius being passed is assumed to be squared!
    size_t res = core->index_->radiusSearch(point.data(), radius_sq, indicesDists, params);

    indices.resize(res);
    dists.resize(res);
    for (size_t i = 0; i < res; ++i) {
      indices[i] = indicesDists[i].first;
      dists[i] = std::sqrt(indicesDists[i].second);       // distances returned are squared!
    }

    return res;
  }

  size_t radiusSearch(const Eigen::Matrix<double, N, 1>& point, double radius, KdTreeSearchResult& res) const {
    return radiusSearch(point, radius, res.indices, res.dists);
  }
};

using KdTree2d = KdTree<2>;
using KdTree3d = KdTree<3>;

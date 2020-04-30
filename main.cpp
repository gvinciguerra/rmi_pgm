//
// Created by Giorgio Vinciguerra on 27/04/2020.
// Modified by Ryan Marcus, 29/04/2020.
//

#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include <tuple>
#include "rmis/wiki.h"
#include "rmis/fb.h"
#include "rmis/osm.h"
#include "rmis/books.h"
#include "PGM-index/include/pgm_index.hpp"

using timer = std::chrono::high_resolution_clock;

uint64_t NUM_LOOKUPS = 10000000;

std::vector<std::string> DATASET_NAMES = {
  "data/books_200M_uint64",
  "data/osm_cellids_200M_uint64",
  "data/wiki_ts_200M_uint64",
  "data/fb_200M_uint64"
};

template<class ForwardIt, class T, class Compare = std::less<T>>
ForwardIt lower_bound_branchless(ForwardIt first, ForwardIt last, const T &value, Compare comp = Compare()) {
    auto n = std::distance(first, last);

    while (n > 1) {
        auto half = n / 2;
        __builtin_prefetch(&*first + half / 2, 0, 0);
        __builtin_prefetch(&*first + half + half / 2, 0, 0);
        first = comp(*std::next(first, half), value) ? first + half : first;
        n -= half;
    }

    return std::next(first, comp(*first, value));
}

template<class T>
void do_not_optimize(T const &value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

template<typename T>
static std::vector<T> load_data(const std::string &filename) {
    std::vector<T> data;

    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "unable to open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    // Read size.
    uint64_t size;
    in.read(reinterpret_cast<char *>(&size), sizeof(uint64_t));
    data.resize(size);

    // Read values.
    in.read(reinterpret_cast<char *>(data.data()), size * sizeof(T));
    in.close();

    return data;
}

std::vector<std::pair<uint64_t, size_t>> generate_queries(std::vector<uint64_t>& dataset) {
  std::vector<std::pair<uint64_t, size_t>> results;
  results.reserve(NUM_LOOKUPS);

  std::mt19937 g(42);
  std::uniform_int_distribution<size_t> distribution(0, dataset.size());
  
  for (uint64_t i = 0; i < NUM_LOOKUPS; i++) {
    size_t idx = distribution(g);
    uint64_t key = dataset[idx];
    size_t correct_lb = std::distance(
      dataset.begin(),
      std::lower_bound(dataset.begin(), dataset.end(), key)
      );
    
    results.push_back(std::make_pair(key, correct_lb));
  }

  return results;
}

template<typename F, class V>
size_t query_time(F f, const V &queries) {
  auto start = timer::now();
  
  uint64_t cnt = 0;
  for (auto &q : queries) {
    cnt += f(q.first, q.second);
  }
  do_not_optimize(cnt);
  
    auto stop = timer::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count() / queries.size();
}


template<int dataset_idx, uint64_t build_time, size_t rmi_size,
         uint64_t (* RMI_FUNC)(uint64_t, size_t*)>
void measure_perfomance() {
  std::string dataset_name = DATASET_NAMES[dataset_idx];
  //std::cout << "Reading " << dataset_name << std::endl;
  auto dataset = load_data<uint64_t>(dataset_name);

  //std::cout << "Generating queries..." << std::endl;
  std::vector<std::pair<uint64_t, size_t>> queries = generate_queries(dataset);
  
  //std::cout << "Elements \t" << dataset.size() << std::endl;
  //std::cout << "RMI construct. \t" << build_time << " ns" << std::endl;
  //std::cout << "RMI index size \t" << rmi_size << " bytes" << std::endl;

  // Test lookups for RMI.
  auto rmi_ns = query_time([&dataset](auto x, auto correct_idx) {
    auto data_size_ = dataset.size();
    size_t error;
    uint64_t guess = RMI_FUNC(x, &error);
    uint64_t start = (guess < error ? 0 : guess - error);
    uint64_t stop = (guess + error >= data_size_ ? data_size_ : guess + error);

    auto lb_result = std::lower_bound(dataset.begin() + start, dataset.begin() + stop, x);
    size_t lb_position = std::distance(dataset.begin(), lb_result);
    if (lb_position != correct_idx) {
      std::cerr << "RMI returned incorrect result for lookup key " << x << std::endl;
      std::cerr << "Start: " << start
                << " Stop: " << stop
                << " Correct: " << correct_idx << std::endl;
      exit(-1);
    }
    return lb_position;
  }, queries);

  // Test lookups for PGM.
  //PGMIndex<uint64_t, 64> index(dataset);
  PGMIndex<uint64_t, 32, 4> index(dataset);
  auto pgm_ns = query_time([&index, &dataset](auto x, auto correct_idx) {
    auto approx_range = index.find_approximate_position(x);
    auto lb_result = lower_bound_branchless(dataset.begin() + approx_range.lo,
                                            dataset.begin() + approx_range.hi,
                                            x);
    
    size_t lb_position = std::distance(dataset.begin(), lb_result);
    if (lb_position != correct_idx) {
      std::cerr << "PGM returned incorrect result for lookup key " << x << std::endl;
      std::cerr << "Start: " << approx_range.lo
                << " Stop: " << approx_range.hi
                << " Correct: " << correct_idx << std::endl;

      exit(-1);
    }
    return lb_position;
  }, queries);

  std::cout << dataset_name << ","
            << rmi_ns << ","
            << pgm_ns << std::endl;
}

int main(int argc, char **argv) {
    // load each RMI
    if (wiki::load("rmi_data/")
        && fb::load("rmi_data/")
        && osm::load("rmi_data/")
        && books::load("rmi_data/")) {
      // RMIs loaded.
    } else {
      std::cerr << "Failed to load RMIs" << std::endl;
      exit(-1);
    }
    std::cout << "Dataset,RMI,PGM" << std::endl;
    measure_perfomance<0, books::RMI_SIZE, books::BUILD_TIME_NS, books::lookup>();
    measure_perfomance<1, osm::RMI_SIZE, osm::BUILD_TIME_NS, osm::lookup>();
    measure_perfomance<2, wiki::RMI_SIZE, wiki::BUILD_TIME_NS, wiki::lookup>();
    measure_perfomance<3, fb::RMI_SIZE, fb::BUILD_TIME_NS, fb::lookup>();
    
    return 0;
}

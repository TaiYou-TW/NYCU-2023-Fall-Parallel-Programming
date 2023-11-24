#include "page_rank.h"

#include <stdlib.h>
#include <cmath>
#include <omp.h>
#include <utility>

#include "../common/CycleTimer.h"
#include "../common/graph.h"

#define PAD 8

// pageRank --
//
// g:           graph to process (see common/graph.h)
// solution:    array of per-vertex vertex scores (length of array is num_nodes(g))
// damping:     page-rank algorithm's damping parameter
// convergence: page-rank algorithm's convergence threshold
//
void pageRank(Graph g, double *solution, double damping, double convergence)
{
  int numNodes = num_nodes(g);
  double equal_prob = 1.0 / numNodes;
  double damping_factor = damping / numNodes;
  double equal_prob_damping = (1.0 - damping) / numNodes;

  double *score_old = new double[numNodes];
  double *score_new = new double[numNodes];

#pragma omp parallel
  {
    int numThreads = omp_get_num_threads();
    int threadId = omp_get_thread_num();

    for (int i = threadId; i < numNodes; i += numThreads)
    {
      score_old[i] = equal_prob;
    }
  }

  bool converged = false;
  while (!converged)
  {
#pragma omp parallel
    {
      int numThreads = omp_get_num_threads();
      int threadId = omp_get_thread_num();
      double *sums = new double[numThreads * PAD];

      for (int i = threadId; i < numNodes; i += numThreads)
      {
        sums[threadId * PAD] = 0.0;

        const Vertex *start = incoming_begin(g, i);
        const Vertex *end = incoming_end(g, i);
        for (const Vertex *j = start; j != end; ++j)
        {
          sums[threadId * PAD] += score_old[*j] / outgoing_size(g, *j);
        }

        score_new[i] = (damping * sums[threadId * PAD]) + equal_prob_damping;
      }

      delete[] sums;
    }

#pragma omp parallel
    {
      int numThreads = omp_get_num_threads();
      int threadId = omp_get_thread_num();
      double *sums = new double[numThreads * PAD];

      for (int i = threadId; i < numNodes; i += numThreads)
      {
        sums[threadId * PAD] = 0.0;
        for (int j = 0; j < numNodes; ++j)
        {
          if (outgoing_size(g, j) == 0)
          {
            sums[threadId * PAD] += damping_factor * score_old[j];
          }
        }
        score_new[i] += sums[threadId * PAD];
      }

      delete[] sums;
    }

    double global_diff = 0.0;
#pragma omp parallel for reduction(+ : global_diff)
    for (int i = 0; i < numNodes; ++i)
    {
      global_diff += abs(score_new[i] - score_old[i]);
    }

    converged = (global_diff < convergence);

    std::swap(score_old, score_new);
  }

#pragma omp parallel
  {
    int numThreads = omp_get_num_threads();
    int threadId = omp_get_thread_num();

    for (int i = threadId; i < numNodes; i += numThreads)
    {
      solution[i] = score_old[i];
    }
  }

  delete[] score_new;
  delete[] score_old;
}

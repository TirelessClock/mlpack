/**
 * @file methods/amf/init_rules/average_init.hpp
 * @author Sumedh Ghaisas
 *
 * Initialization rule for Alternating Matrix Factorization.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_AMF_AVERAGE_INIT_HPP
#define MLPACK_METHODS_AMF_AVERAGE_INIT_HPP

#include <mlpack/prereqs.hpp>

namespace mlpack {

/**
 * This initialization rule initializes matrix W and H to root of the average of
 * V, perturbed with uniform noise. Uniform noise is generated by Armadillo's
 * 'randu' function.  For better performance, the lowest element of the matrix
 * is subtracted from the average before dividing it by the factorization rank.
 * This computed value is added with the random noise.
 */
class AverageInitialization
{
 public:
  // Empty constructor required for the InitializeRule template
  AverageInitialization() { }

  /**
   * Initialize the matrices W and H to the average value of V with uniform
   * random noise added.
   *
   * @param V Input matrix.
   * @param r Rank of matrix.
   * @param W W matrix, to be initialized.
   * @param H H matrix, to be initialized.
   */
  template<typename MatType, typename WHMatType>
  inline static void Initialize(const MatType& V,
                                const size_t r,
                                WHMatType& W,
                                WHMatType& H)
  {
    const size_t n = V.n_rows;
    const size_t m = V.n_cols;

    double avgV = 0;
    double min = DBL_MAX;

    // Iterate over all elements in the matrix (for sparse matrices, this only
    // iterates over nonzeros).
    for (typename MatType::const_row_col_iterator it = V.begin();
        it != V.end(); ++it)
    {
      avgV += *it;
      // Track the minimum value.
      if (*it < min)
        min = *it;
    }

    avgV = std::sqrt(((avgV / (n * m)) - min) / r);

    // Initialize to random values.
    W.randu(n, r);
    H.randu(r, m);

    W += avgV;
    H += + avgV;
  }

  /**
   * Initialize the matrix W or H to the average value of V with uniform
   * random noise added.
   *
   * @param V Input matrix.
   * @param r Rank of matrix.
   * @param M W or H matrix, to be initialized to the average value of V
   * with uniform random noise added.
   * @param whichMatrix If true, initialize W. Otherwise, initialize H.
   */
  template<typename MatType, typename WHMatType>
  inline static void InitializeOne(const MatType& V,
                                   const size_t r,
                                   WHMatType& M,
                                   const bool whichMatrix = true)
  {
    const size_t n = V.n_rows;
    const size_t m = V.n_cols;

    double avgV = 0;
    double min = DBL_MAX;

    // Iterate over all elements in the matrix (for sparse matrices, this only
    // iterates over nonzeros).
    for (typename MatType::const_row_col_iterator it = V.begin();
        it != V.end(); ++it)
    {
      avgV += *it;
      // Track the minimum value.
      if (*it < min)
        min = *it;
    }
    if (whichMatrix)
    {
      // Initialize W to random values
      M.randu(n, r);
    }
    else
    {
      // Initialize H to random values
      M.randu(r, m);
    }
    M += std::sqrt(((avgV / (n * m)) - min) / r);
  }

  //! Serialize the object (in this case, there is nothing to do).
  template<typename Archive>
  void serialize(Archive& /* ar */, const uint32_t /* version */) { }
};

} // namespace mlpack

#endif

/**
 * @file methods/xgboost/xgbtree.hpp
 * @author Abhimanyu Dayal
 *
 * XGBtree node. Implementation of Decision Tree specifically for XGBoost 
 * weak learner implementation.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_XGBTREE_NODE_HPP
#define MLPACK_METHODS_XGBTREE_NODE_HPP

#include <mlpack/core.hpp>
#include "feature_importance.hpp"

#include "../decision_tree/fitness_functions/mse_gain.hpp"
#include "../decision_tree/select_functions/all_dimension_select.hpp"
#include "../decision_tree/split_functions/best_binary_numeric_split.hpp"
#include "../decision_tree/split_functions/all_categorical_split.hpp"


using namespace std;

// Defined within the mlpack namespace.
namespace mlpack {


class XGBTree :
  public BestBinaryNumericSplit<MSEGain>::AuxiliarySplitInfo,
  public AllCategoricalSplit<MSEGain>::AuxiliarySplitInfo
{

  private: 

  //! The vector of children.
  std::vector<XGBTree*> children;
  union
  {
    //! Stores the prediction value, for leaf nodes of the tree.
    double prediction;
    //! The dimension of the split, for internal nodes.
    size_t splitDimension;
  };
  //! For internal nodes, the type of the split variable.
  size_t dimensionType;
  //! For internal nodes, the split information for the splitter.
  arma::vec splitInfo;
  //! The best gain on the particular node.
  double nodeGain;


  size_t CalculateDirection(const arma::vec& point) const
  {
    if ((data::Datatype) dimensionType == data::Datatype::categorical)
      return CategoricalSplit::CalculateDirection(point[splitDimension],
          splitInfo, *this);
    else
      return NumericSplit::CalculateDirection(point[splitDimension],
          splitInfo, *this);
  }


  public:

  //! Allow access to the numeric split type.
  typedef mlpack::BestBinaryNumericSplit<mlpack::MSEGain> NumericSplit;
  //! Allow access to the categorical split type.
  typedef mlpack::AllCategoricalSplit<mlpack::MSEGain> CategoricalSplit;
  //! Allow access to the dimension selection type.
  typedef mlpack::AllDimensionSelect DimensionSelection;

  //! Note that this class will also hold the members of the NumericSplit and
  //! CategoricalSplit AuxiliarySplitInfo classes, since it inherits from them.
  //! We'll define some convenience typedefs here.
  typedef typename NumericSplit::AuxiliarySplitInfo
    NumericAuxiliarySplitInfo;
  typedef typename CategoricalSplit::AuxiliarySplitInfo
    CategoricalAuxiliarySplitInfo;


  XGBTree() { /*Nothing to do */ }

  XGBTree(const arma::mat& data,
          const arma::rowvec& responses,
          const data::DatasetInfo& datasetInfo,
          const size_t minimumLeafSize,
          const double minimumGainSplit,
          const size_t maximumDepth,
          FeatureImportance* featImp)
  {

    // Copy or move data.
    arma::mat tmpData(std::move(data));
    arma::rowvec tmpResponses(std::move(responses));

    arma::rowvec weights; /* Not to use */
    DimensionSelection dimensionSelector;
    MSEGain msegain;
    
    dimensionSelector.Dimensions() = data.n_rows;

    Train(tmpData, 0, data.n_cols, datasetInfo, tmpResponses, 
      weights, minimumLeafSize, minimumGainSplit, maximumDepth, 
      dimensionSelector, msegain, featImp);

  }

  double Train(arma::mat& data,
                const size_t begin,
                const size_t count,
                const data::DatasetInfo& datasetInfo,
                arma::rowvec& responses,
                arma::rowvec& weights, /* Not to use */
                const size_t minimumLeafSize,
                const double minimumGainSplit,
                const size_t maximumDepth,
                DimensionSelection& dimensionSelector,
                MSEGain msegain,
                FeatureImportance* featImp)
  {
    // Clear children if needed.
    for (size_t i = 0; i < children.size(); ++i)
      delete children[i];
    children.clear();

    // Look through the list of dimensions and obtain the gain of the best split.
    // We'll cache the best numeric and categorical split auxiliary information
    // in splitInfo, which is non-empty only for internal nodes of the tree.
    double bestGain = msegain.template Evaluate<false>(
        responses.cols(begin, begin + count - 1),
        weights);
    size_t bestDim = datasetInfo.Dimensionality(); // This means "no split".
    const size_t end = dimensionSelector.End();

    if (maximumDepth != 1)
    {
      for (size_t i = dimensionSelector.Begin(); i != end;
          i = dimensionSelector.Next())
      {
        double dimGain = -DBL_MAX;
        if (datasetInfo.Type(i) == data::Datatype::categorical)
        {
          dimGain = CategoricalSplit::template SplitIfBetter<false>(bestGain,
              data.cols(begin, begin + count - 1).row(i),
              datasetInfo.NumMappings(i),
              responses.cols(begin, begin + count - 1),
              weights,
              minimumLeafSize,
              minimumGainSplit,
              splitInfo,
              *this,
              msegain);
        }
        else if (datasetInfo.Type(i) == data::Datatype::numeric)
        {
          dimGain = NumericSplit::template SplitIfBetter<false>(bestGain,
              data.cols(begin, begin + count - 1).row(i),
              responses.cols(begin, begin + count - 1),
              weights,
              minimumLeafSize,
              minimumGainSplit,
              splitInfo,
              *this,
              msegain);
        }

        // If the splitter reported that it did not split, move to the next
        // dimension.
        if (dimGain == DBL_MAX)
          continue;

        // Was there an improvement?  If so mark that it's the new best dimension.
        bestDim = i;
        bestGain = dimGain;

        // If the gain is the best possible, no need to keep looking.
        if (bestGain >= 0.0)
          break;
      }
    }

    // Did we split or not?  If so, then split the data and create the children.
    if (bestDim != datasetInfo.Dimensionality())
    {
      if(featImp != nullptr)
      {
        featImp->increaseFeatureFrequency(bestDim, 1);
        featImp->increaseFeatureCover(bestDim, bestGain);
      }
      dimensionType = (size_t) datasetInfo.Type(bestDim);
      splitDimension = bestDim;

      // Get the number of children we will have.
      size_t numChildren = 0;
      if (datasetInfo.Type(bestDim) == data::Datatype::categorical)
        numChildren = CategoricalSplit::NumChildren(splitInfo, *this);
      else
        numChildren = NumericSplit::NumChildren(splitInfo, *this);

      // Calculate all child assignments.
      arma::Row<size_t> childAssignments(count);
      if (datasetInfo.Type(bestDim) == data::Datatype::categorical)
      {
        for (size_t j = begin; j < begin + count; ++j)
          childAssignments[j - begin] = CategoricalSplit::CalculateDirection(
              data(bestDim, j), splitInfo, *this);
      }
      else
      {
        for (size_t j = begin; j < begin + count; ++j)
        {
          childAssignments[j - begin] = NumericSplit::CalculateDirection(
              data(bestDim, j), splitInfo, *this);
        }
      }

      // Figure out counts of children.
      arma::Row<size_t> childCounts(numChildren, arma::fill::zeros);
      for (size_t i = begin; i < begin + count; ++i)
        childCounts[childAssignments[i - begin]]++;

      bestGain = 0.0;

      // Split into children.
      size_t currentCol = begin;
      for (size_t i = 0; i < numChildren; ++i)
      {
        size_t currentChildBegin = currentCol;
        for (size_t j = currentChildBegin; j < begin + count; ++j)
        {
          if (childAssignments[j - begin] == i)
          {
            childAssignments.swap_cols(currentCol - begin, j - begin);
            data.swap_cols(currentCol, j);
            responses.swap_cols(currentCol, j);
            ++currentCol;
          }
        }

        // Now build the child recursively.
        XGBTree* child = new XGBTree();

        // During recursion entropy of child node may change.
        double childGain = child->Train(data, currentChildBegin,
            currentCol - currentChildBegin, datasetInfo, responses,
            weights, minimumLeafSize, minimumGainSplit, maximumDepth - 1,
            dimensionSelector, msegain, featImp);

        bestGain += double(childCounts[i]) / double(count) * (-childGain);
        children.push_back(child);
      }
    }
    else
    {
      // Calculate prediction value because we are a leaf.
      prediction = msegain.template OutputLeafValue<false>(
          responses.cols(begin, begin + count - 1),
          weights);
    }

    nodeGain = bestGain;
    return -bestGain;
  }

  double Predict(const arma::vec& point) const
  {
    if (children.size() == 0)
    {
      // Return cached prediction.
      return prediction;
    }

    return children[CalculateDirection(point)]->Predict(point);
  }

  bool Prune(double threshold)
  {
    size_t numChildren = children.size();

    for (size_t i = 0; i < numChildren; ++i)
    {
      bool store = children[i]->Prune(threshold);
      if (store == true)
      {
        XGBTree* tempStorage = children[i];

        children.erase(children.begin() + i);
        numChildren--;
        i--;
        
        delete tempStorage;
      }
    }

    bool flag = false;
    if (nodeGain < threshold)
      flag = 1;

    return flag;
  }

};

}; // mlpack

#endif



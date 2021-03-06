#pragma once

#include <cilantro/kd_tree.hpp>

namespace cilantro {
    template <typename ScalarT>
    struct Correspondence {
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        Correspondence(size_t i, size_t j, ScalarT val) : indexInFirst(i), indexInSecond(j), value(val) {}

        size_t indexInFirst;
        size_t indexInSecond;
        ScalarT value;

        struct ValueLessComparator {
            inline bool operator()(const Correspondence &c1, const Correspondence &c2) const {
                return c1.value < c2.value;
            }
        };

        struct ValueGreaterComparator {
            inline bool operator()(const Correspondence &c1, const Correspondence &c2) const {
                return c1.value > c2.value;
            }
        };

        struct IndicesLexicographicalComparator {
            inline bool operator()(const Correspondence &c1, const Correspondence &c2) const {
                return std::tie(c1.indexInFirst, c1.indexInSecond) < std::tie(c2.indexInFirst, c2.indexInSecond);
            }
        };
    };

    template <typename ScalarT>
    struct CorrespondenceDistanceEvaluator {
        inline ScalarT operator()(size_t first_ind, size_t second_ind, ScalarT dist) const {
            return dist;
        }
    };

    template <typename ScalarT>
    using CorrespondenceSet = std::vector<Correspondence<ScalarT>>;

    template <typename ScalarT, ptrdiff_t EigenDim, typename CorrValueT = ScalarT>
    void selectFirstSetCorrespondingPoints(const CorrespondenceSet<CorrValueT> &correspondences,
                                           const ConstVectorSetMatrixMap<ScalarT,EigenDim> &first,
                                           VectorSet<ScalarT,EigenDim> &first_corr)
    {
        first_corr.resize(first.rows(), correspondences.size());
#pragma omp parallel for
        for (size_t i = 0; i < correspondences.size(); i++) {
            first_corr.col(i) = first.col(correspondences[i].indexInFirst);
        }
    }

    template <typename ScalarT, ptrdiff_t EigenDim, typename CorrValueT = ScalarT>
    VectorSet<ScalarT,EigenDim> selectFirstSetCorrespondingPoints(const CorrespondenceSet<CorrValueT> &correspondences,
                                                                  const ConstVectorSetMatrixMap<ScalarT,EigenDim> &first)
    {
        VectorSet<ScalarT,EigenDim> first_corr(first.rows(), correspondences.size());
#pragma omp parallel for
        for (size_t i = 0; i < correspondences.size(); i++) {
            first_corr.col(i) = first.col(correspondences[i].indexInFirst);
        }
        return first_corr;
    }

    template <typename ScalarT, ptrdiff_t EigenDim, typename CorrValueT = ScalarT>
    void selectSecondSetCorrespondingPoints(const CorrespondenceSet<CorrValueT> &correspondences,
                                            const ConstVectorSetMatrixMap<ScalarT,EigenDim> &second,
                                            VectorSet<ScalarT,EigenDim> &second_corr)
    {
        second_corr.resize(second.rows(), correspondences.size());
#pragma omp parallel for
        for (size_t i = 0; i < correspondences.size(); i++) {
            second_corr.col(i) = second.col(correspondences[i].indexInSecond);
        }
    }

    template <typename ScalarT, ptrdiff_t EigenDim, typename CorrValueT = ScalarT>
    VectorSet<ScalarT,EigenDim> selectSecondSetCorrespondingPoints(const CorrespondenceSet<CorrValueT> &correspondences,
                                                                   const ConstVectorSetMatrixMap<ScalarT,EigenDim> &second)
    {
        VectorSet<ScalarT,EigenDim> second_corr(second.rows(), correspondences.size());
#pragma omp parallel for
        for (size_t i = 0; i < correspondences.size(); i++) {
            second_corr.col(i) = second.col(correspondences[i].indexInSecond);
        }
        return second_corr;
    }

    template <typename ScalarT, ptrdiff_t EigenDim, typename CorrValueT = ScalarT>
    void selectCorrespondingPoints(const CorrespondenceSet<CorrValueT> &correspondences,
                                   const ConstVectorSetMatrixMap<ScalarT,EigenDim> &first,
                                   const ConstVectorSetMatrixMap<ScalarT,EigenDim> &second,
                                   VectorSet<ScalarT,EigenDim> &first_corr,
                                   VectorSet<ScalarT,EigenDim> &second_corr)
    {
        first_corr.resize(first.rows(), correspondences.size());
        second_corr.resize(second.rows(), correspondences.size());
#pragma omp parallel for
        for (size_t i = 0; i < correspondences.size(); i++) {
            first_corr.col(i) = first.col(correspondences[i].indexInFirst);
            second_corr.col(i) = second.col(correspondences[i].indexInSecond);
        }
    }

    template <typename ScalarT, class ComparatorT = typename Correspondence<ScalarT>::ValueLessComparator>
    void filterCorrespondencesFraction(CorrespondenceSet<ScalarT> &correspondences, double fraction_to_keep) {
        if (fraction_to_keep > 0.0 && fraction_to_keep < 1.0) {
            std::sort(correspondences.begin(), correspondences.end(), ComparatorT());
            correspondences.erase(correspondences.begin() + std::llround(fraction_to_keep*correspondences.size()), correspondences.end());
        }
    }

    template <typename ScalarT, class ComparatorT = typename Correspondence<ScalarT>::ValueLessComparator>
    inline CorrespondenceSet<ScalarT> filterCorrespondencesFraction(const CorrespondenceSet<ScalarT> &correspondences, double fraction_to_keep) {
        CorrespondenceSet<ScalarT> corr_res(correspondences);
        filterCorrespondencesFraction<ScalarT,ComparatorT>(corr_res, fraction_to_keep);
        return corr_res;
    }

    template <typename ScalarT, ptrdiff_t EigenDim, template <class> class DistAdaptor = KDTreeDistanceAdaptors::L2, class EvaluatorT = CorrespondenceDistanceEvaluator<ScalarT>, typename CorrValueT = decltype(std::declval<EvaluatorT>().operator()((size_t)0,(size_t)0,(ScalarT)0))>
    void findNNCorrespondencesUnidirectional(const ConstVectorSetMatrixMap<ScalarT,EigenDim> &query_pts,
                                             const KDTree<ScalarT,EigenDim,DistAdaptor> &ref_tree,
                                             bool ref_is_first,
                                             CorrespondenceSet<CorrValueT> &correspondences,
                                             CorrValueT max_distance,
                                             const EvaluatorT &evaluator = EvaluatorT())
    {
        correspondences.clear();
        correspondences.reserve(query_pts.cols());

        NearestNeighborSearchResult<ScalarT> nn;

        if (ref_is_first) {
#pragma omp parallel for shared (correspondences) private (nn)
            for (size_t i = 0; i < query_pts.cols(); i++) {
                ref_tree.nearestNeighborSearch(query_pts.col(i), nn);
                if (nn.value < max_distance) {
#pragma omp critical
                    correspondences.emplace_back(nn.index, i, evaluator(nn.index, i, nn.value));
                }
            }
        } else {
#pragma omp parallel for shared (correspondences) private (nn)
            for (size_t i = 0; i < query_pts.cols(); i++) {
                ref_tree.nearestNeighborSearch(query_pts.col(i), nn);
                if (nn.value < max_distance) {
#pragma omp critical
                    correspondences.emplace_back(i, nn.index, evaluator(i, nn.index, nn.value));
                }
            }
        }
    }

    template <typename ScalarT, ptrdiff_t EigenDim, template <class> class DistAdaptor = KDTreeDistanceAdaptors::L2, class EvaluatorT = CorrespondenceDistanceEvaluator<ScalarT>, typename CorrValueT = decltype(std::declval<EvaluatorT>().operator()((size_t)0,(size_t)0,(ScalarT)0))>
    inline CorrespondenceSet<CorrValueT> findNNCorrespondencesUnidirectional(const ConstVectorSetMatrixMap<ScalarT,EigenDim> &query_pts,
                                                                             const KDTree<ScalarT,EigenDim,DistAdaptor> &ref_tree,
                                                                             bool ref_is_first,
                                                                             CorrValueT max_distance,
                                                                             const EvaluatorT &evaluator = EvaluatorT())
    {
        CorrespondenceSet<CorrValueT> corr_set;
        findNNCorrespondencesUnidirectional<ScalarT,EigenDim,DistAdaptor,EvaluatorT,CorrValueT>(query_pts, ref_tree, ref_is_first, corr_set, max_distance, evaluator);
        return corr_set;
    }

    template <typename ScalarT, ptrdiff_t EigenDim, template <class> class DistAdaptor = KDTreeDistanceAdaptors::L2, class EvaluatorT = CorrespondenceDistanceEvaluator<ScalarT>, typename CorrValueT = decltype(std::declval<EvaluatorT>().operator()((size_t)0,(size_t)0,(ScalarT)0))>
    void findNNCorrespondencesBidirectional(const ConstVectorSetMatrixMap<ScalarT,EigenDim> &first_points,
                                            const ConstVectorSetMatrixMap<ScalarT,EigenDim> &second_points,
                                            const KDTree<ScalarT,EigenDim,DistAdaptor> &first_tree,
                                            const KDTree<ScalarT,EigenDim,DistAdaptor> &second_tree,
                                            CorrespondenceSet<CorrValueT> &correspondences,
                                            CorrValueT max_distance,
                                            bool require_reciprocal = false,
                                            const EvaluatorT &evaluator = EvaluatorT())
    {
        CorrespondenceSet<CorrValueT> corr_first_to_second, corr_second_to_first;
        findNNCorrespondencesUnidirectional<ScalarT,EigenDim,DistAdaptor,EvaluatorT,CorrValueT>(first_points, second_tree, false, corr_first_to_second, max_distance, evaluator);
        findNNCorrespondencesUnidirectional<ScalarT,EigenDim,DistAdaptor,EvaluatorT,CorrValueT>(second_points, first_tree, true, corr_second_to_first, max_distance, evaluator);

        typename Correspondence<CorrValueT>::IndicesLexicographicalComparator comparator;

        std::sort(corr_first_to_second.begin(), corr_first_to_second.end(), comparator);
        std::sort(corr_second_to_first.begin(), corr_second_to_first.end(), comparator);

        correspondences.clear();
        correspondences.reserve(corr_first_to_second.size()+corr_second_to_first.size());

        if (require_reciprocal) {
            std::set_intersection(corr_first_to_second.begin(), corr_first_to_second.end(),
                                  corr_second_to_first.begin(), corr_second_to_first.end(),
                                  std::back_inserter(correspondences), comparator);
        } else {
            std::set_union(corr_first_to_second.begin(), corr_first_to_second.end(),
                           corr_second_to_first.begin(), corr_second_to_first.end(),
                           std::back_inserter(correspondences), comparator);
        }
    }

    template <typename ScalarT, ptrdiff_t EigenDim, template <class> class DistAdaptor = KDTreeDistanceAdaptors::L2, class EvaluatorT = CorrespondenceDistanceEvaluator<ScalarT>, typename CorrValueT = decltype(std::declval<EvaluatorT>().operator()((size_t)0,(size_t)0,(ScalarT)0))>
    inline CorrespondenceSet<CorrValueT> findNNCorrespondencesBidirectional(const ConstVectorSetMatrixMap<ScalarT,EigenDim> &first_points,
                                                                            const ConstVectorSetMatrixMap<ScalarT,EigenDim> &second_points,
                                                                            const KDTree<ScalarT,EigenDim,DistAdaptor> &first_tree,
                                                                            const KDTree<ScalarT,EigenDim,DistAdaptor> &second_tree,
                                                                            CorrValueT max_distance,
                                                                            bool require_reciprocal = false,
                                                                            const EvaluatorT &evaluator = EvaluatorT())
    {
        CorrespondenceSet<CorrValueT> corr_set;
        findNNCorrespondencesBidirectional<ScalarT,EigenDim,DistAdaptor,EvaluatorT,CorrValueT>(first_points, second_points, first_tree, second_tree, corr_set, max_distance, require_reciprocal, evaluator);
        return corr_set;
    }
}

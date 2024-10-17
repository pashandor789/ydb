#pragma once

#include <ydb/library/yql/core/cbo/cbo_optimizer_new.h> 

namespace NYql::NDq {

struct IBaseOptimizerNodeInternal {
    TOptimizerStatistics Stats;
    EOptimizerNodeKind Kind;

    virtual ~IBaseOptimizerNodeInternal() = default;
};


struct TRelOptimizerNodeInternal : public IBaseOptimizerNodeInternal {
    TRelOptimizerNodeInternal(
        const TOptimizerStatistics& stats,
        TString label 
    ) 
        : Label(std::move(label))
    {
        Stats = stats;
        Kind = EOptimizerNodeKind::RelNodeType;
    }

    TString Label;
};

/**
 * Internal Join nodes are used inside the CBO. They don't own join condition data structures
 * and therefore avoid copying them during generation of candidate plans.
 *
 * These datastructures are owned by the query graph, so it is important to keep the graph around
 * while internal nodes are being used.
 *
 * After join enumeration, internal nodes need to be converted to regular nodes, that own the data
 * structures
*/
struct TJoinOptimizerNodeInternal : public IBaseOptimizerNodeInternal {
    TJoinOptimizerNodeInternal(
        TOptimizerStatistics&& stats,
        const std::shared_ptr<IBaseOptimizerNodeInternal>& left, 
        const std::shared_ptr<IBaseOptimizerNodeInternal>& right,
        const TVector<TJoinColumn>& leftJoinKeys,
        const TVector<TJoinColumn>& rightJoinKeys, 
        const EJoinKind joinType, 
        const EJoinAlgoType joinAlgo,
        const bool leftAny,
        const bool rightAny
    ) 
        : LeftArg(left)
        , RightArg(right)
        , LeftJoinKeys(leftJoinKeys)
        , RightJoinKeys(rightJoinKeys)
        , JoinType(joinType)
        , JoinAlgo(joinAlgo)
        , LeftAny(leftAny)
        , RightAny(rightAny)
    {
        Stats = std::move(stats);
        Kind = EOptimizerNodeKind::JoinNodeType;
    }

    virtual ~TJoinOptimizerNodeInternal() = default;

    std::shared_ptr<IBaseOptimizerNodeInternal> LeftArg;
    std::shared_ptr<IBaseOptimizerNodeInternal> RightArg;
    const TVector<TJoinColumn>& LeftJoinKeys;
    const TVector<TJoinColumn>& RightJoinKeys;
    EJoinKind JoinType;
    EJoinAlgoType JoinAlgo;
    const bool LeftAny;
    const bool RightAny;
};

/**
 * Create a new internal join node and compute its statistics and cost
*/
std::shared_ptr<TJoinOptimizerNodeInternal> MakeJoinInternal(
    TOptimizerStatistics&& stats,
    const std::shared_ptr<IBaseOptimizerNodeInternal>& left,
    const std::shared_ptr<IBaseOptimizerNodeInternal>& right,
    const TVector<TJoinColumn>& leftJoinKeys,
    const TVector<TJoinColumn>& rightJoinKeys,
    EJoinKind joinKind,
    EJoinAlgoType joinAlgo,
    bool leftAny,
    bool rightAny
);

/**
 * Convert a tree of internal optimizer nodes to external nodes that own the data structures.
 *
 * The internal node tree can have references to external nodes (since some subtrees are optimized
 * separately if the plan contains non-orderable joins). So we check the instances and if we encounter
 * an external node, we return the whole subtree unchanged.
*/
std::shared_ptr<IBaseOptimizerNode> ConvertFromInternal(const std::shared_ptr<IBaseOptimizerNodeInternal>& internal);

} // namespace NYql::NDq

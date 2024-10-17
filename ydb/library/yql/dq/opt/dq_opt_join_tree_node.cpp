#include "dq_opt_join_tree_node.h"

namespace NYql::NDq {

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
) {
    return std::make_shared<TJoinOptimizerNodeInternal>(std::move(stats), left, right, leftJoinKeys, rightJoinKeys, joinKind, joinAlgo, leftAny, rightAny);
}

std::shared_ptr<IBaseOptimizerNode> ConvertFromInternal(const std::shared_ptr<IBaseOptimizerNodeInternal>& internal) {
    if (internal->Kind == EOptimizerNodeKind::RelNodeType) {
        auto rel = std::static_pointer_cast<TRelOptimizerNodeInternal>(internal);
        return std::make_shared<TRelOptimizerNode>(std::move(rel->Label), std::make_shared<TOptimizerStatistics>(std::move(rel->Stats)));
    }

    auto join = std::static_pointer_cast<TJoinOptimizerNodeInternal>(internal);

    auto lhs = ConvertFromInternal(join->LeftArg);
    auto rhs = ConvertFromInternal(join->RightArg);

    auto newJoin = std::make_shared<TJoinOptimizerNode>(lhs, rhs, join->LeftJoinKeys, join->RightJoinKeys, join->JoinType, join->JoinAlgo, join->LeftAny, join->RightAny);
    newJoin->Stats = std::make_shared<TOptimizerStatistics>(std::move(join->Stats));
    return newJoin;
}

} // namespace NYql::NDq

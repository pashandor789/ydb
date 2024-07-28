#include "analyze_actor.h"

#include <ydb/core/base/path.h>
#include <ydb/core/base/tablet_pipecache.h>
#include <ydb/library/actors/core/log.h>
#include <ydb/library/services/services.pb.h>


namespace NKikimr::NKqp {

enum {
    FirstRoundCookie = 0,
    SecondRoundCookie = 1,
};

using TNavigate = NSchemeCache::TSchemeCacheNavigate;

void TAnalyzeActor::Bootstrap() {
    using TNavigate = NSchemeCache::TSchemeCacheNavigate;
    auto navigate = std::make_unique<TNavigate>();
    auto& entry = navigate->ResultSet.emplace_back();
    entry.Path = SplitPath(TablePath);
    entry.Operation = TNavigate::EOp::OpTable;
    entry.RequestType = TNavigate::TEntry::ERequestType::ByPath;
    navigate->Cookie = FirstRoundCookie;

    Send(NKikimr::MakeSchemeCacheID(), new TEvTxProxySchemeCache::TEvNavigateKeySet(navigate.release()));

    Become(&TAnalyzeActor::StateWork);
}

void TAnalyzeActor::Handle(NStat::TEvStatistics::TEvScanTableResponse::TPtr& ev, const TActorContext& ctx) {
    NYql::IKikimrGateway::TGenericResult result;
    result.SetSuccess();
    Promise.SetValue(std::move(result));
    
    this->Die(ctx);
}

void TAnalyzeActor::Handle(TEvTxProxySchemeCache::TEvNavigateKeySetResult::TPtr& ev, const TActorContext& ctx) {
    std::unique_ptr<TNavigate> navigate(ev->Get()->Request.Release());
    Y_ABORT_UNLESS(navigate->ResultSet.size() == 1);
    auto& entry = navigate->ResultSet.front();

    if (entry.Status != TNavigate::EStatus::Ok) {
        Promise.SetValue(
            NYql::NCommon::ResultFromIssues<NYql::IKikimrGateway::TGenericResult>(
                NYql::TIssuesIds::KIKIMR_TEMPORARILY_UNAVAILABLE,
                TStringBuilder() << "Can't get statistics aggregator ID. " << entry.Status, 
                {}
            )
        );
        this->Die(ctx);
        return;
    }

    if (navigate->Cookie == SecondRoundCookie) {
        if (entry.DomainInfo->Params.HasStatisticsAggregator()) {
            SendStatisticsAggregatorAnalyze(entry.DomainInfo->Params.GetStatisticsAggregator());
        } else {
            Promise.SetValue(
                NYql::NCommon::ResultFromIssues<NYql::IKikimrGateway::TGenericResult>(
                    NYql::TIssuesIds::KIKIMR_TEMPORARILY_UNAVAILABLE,
                    TStringBuilder() << "Can't get statistics aggregator ID.", {}
                )
            );
        }

        this->Die(ctx);
        return;
    }
    
    PathId = entry.TableId.PathId;

    auto& domainInfo = entry.DomainInfo;

    auto navigateDomainKey = [this] (TPathId domainKey) {
        using TNavigate = NSchemeCache::TSchemeCacheNavigate;
        auto navigate = std::make_unique<TNavigate>();
        auto& entry = navigate->ResultSet.emplace_back();
        entry.TableId = TTableId(domainKey.OwnerId, domainKey.LocalPathId);
        entry.Operation = TNavigate::EOp::OpPath;
        entry.RequestType = TNavigate::TEntry::ERequestType::ByTableId;
        entry.RedirectRequired = false;
        navigate->Cookie = SecondRoundCookie;

        Send(MakeSchemeCacheID(), new TEvTxProxySchemeCache::TEvNavigateKeySet(navigate.release()));
    };

    if (!domainInfo->IsServerless()) {
        if (domainInfo->Params.HasStatisticsAggregator()) {
            SendStatisticsAggregatorAnalyze(domainInfo->Params.GetStatisticsAggregator());
            return;
        }
            
        navigateDomainKey(domainInfo->DomainKey);  
    } else {
        navigateDomainKey(domainInfo->ResourcesDomainKey);
    }
}

void TAnalyzeActor::SendStatisticsAggregatorAnalyze(ui64 statisticsAggregatorId) {
    auto scanTable = std::make_unique<NStat::TEvStatistics::TEvScanTable>();
    auto& record = scanTable->Record;
    PathIdFromPathId(PathId, record.MutablePathId());

    
    Send(
        MakePipePerNodeCacheID(false),
        new TEvPipeCache::TEvForward(scanTable.release(), statisticsAggregatorId, true),
        IEventHandle::FlagTrackDelivery
    );
}

void TAnalyzeActor::HandleUnexpectedEvent(ui32 typeRewrite) {
    ALOG_CRIT(
        NKikimrServices::KQP_GATEWAY, 
        "TAnalyzeActor, unexpected event, request type: " << typeRewrite;
    );
        
    Promise.SetValue(
        NYql::NCommon::ResultFromError<NYql::IKikimrGateway::TGenericResult>(
            YqlIssue(
                {}, NYql::TIssuesIds::UNEXPECTED, 
                TStringBuilder() << "Unexpected event: " << typeRewrite
            )
        )
    );

    this->PassAway();
}

}// end of NKikimr::NKqp

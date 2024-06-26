#ifndef DQWorkerClient_H
#define DQWorkerClient_H

#include <utility>

#include "DQM/EcalCommon/interface/DQWorker.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"

class DetId;
namespace edm {
  class ConsumesCollector;
}  // namespace edm

namespace ecaldqm {
  class StatusManager;

  class DQWorkerClient : public DQWorker {
  public:
    enum ProcessType { kLumi, kJob, nProcessType };

    DQWorkerClient();
    ~DQWorkerClient() override {}

    static void fillDescriptions(edm::ParameterSetDescription&);

    void endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;
    void resetPerLumi();

    void bookMEs(DQMStore::IBooker&) override;
    void releaseMEs() override;

    void releaseSource();
    bool retrieveSource(DQMStore::IGetter&, ProcessType);

    bool runsOn(ProcessType _type) const { return _type == kJob || hasLumiPlots_; }
    virtual void resetMEs();
    virtual void producePlots(ProcessType) = 0;

    // mechanisms to register EDGetTokens for any additional objects used internally
    virtual void setTokens(edm::ConsumesCollector&) {}

    void setStatusManager(StatusManager const& _manager) { statusManager_ = &_manager; }

    static constexpr int kBad = 0, kGood = 1, kUnknown = 2, kMBad = 3, kMGood = 4, kMUnknown = 5;

  protected:
    void setME(edm::ParameterSet const& _ps) final;
    void setSource(edm::ParameterSet const&) override;

    bool using_(std::string const& _name, ProcessType _type = kJob) const {
      MESetCollection::const_iterator itr(sources_.find(_name));
      if (itr == sources_.end())
        return false;
      if (_type == kJob)
        return true;
      else
        return itr->second->getLumiFlag();
    }

    void towerAverage_(MESet&, MESet const&, float);

    MESetCollection sources_;
    std::set<std::string> qualitySummaries_;

    bool hasLumiPlots_;

    StatusManager const* statusManager_;
  };
}  // namespace ecaldqm
#endif

#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHit.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFraction.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "RecoParticleFlow/PFClusterProducer/interface/CaloRecHitResolutionProvider.h"
#include "RecoParticleFlow/PFClusterProducer/interface/PFClusterBuilderBase.h"
#include "CondFormats/DataRecord/interface/HcalPFCutsRcd.h"
#include "CondTools/Hcal/interface/HcalPFCutsHandler.h"

#include "Math/GenVector/VectorUtil.h"
#include "TMath.h"
#include "vdt/vdtMath.h"

#include <iterator>
#include <unordered_map>

class PFlow2DClusterizerWithTime : public PFClusterBuilderBase {
  typedef PFlow2DClusterizerWithTime B2DGPF;

public:
  PFlow2DClusterizerWithTime(const edm::ParameterSet& conf, edm::ConsumesCollector& cc);

  ~PFlow2DClusterizerWithTime() override = default;
  PFlow2DClusterizerWithTime(const B2DGPF&) = delete;
  B2DGPF& operator=(const B2DGPF&) = delete;

  void update(const edm::EventSetup& es) override {
    _positionCalc->update(es);
    if (_allCellsPosCalc)
      _allCellsPosCalc->update(es);
    if (_convergencePosCalc)
      _convergencePosCalc->update(es);
  }

  void buildClusters(const reco::PFClusterCollection&,
                     const std::vector<bool>&,
                     reco::PFClusterCollection& outclus,
                     const HcalPFCuts*) override;

private:
  const unsigned _maxIterations;
  const double _stoppingTolerance;
  const double _showerSigma2;
  const double _timeSigma_eb;
  const double _timeSigma_ee;
  const bool _excludeOtherSeeds;
  const double _minFracTot;
  const double _maxNSigmaTime;
  const double _minChi2Prob;
  const bool _clusterTimeResFromSeed;

  const std::unordered_map<std::string, int> _layerMap;
  std::unordered_map<int, double> _recHitEnergyNorms;
  std::unique_ptr<PFCPositionCalculatorBase> _allCellsPosCalc;
  std::unique_ptr<PFCPositionCalculatorBase> _convergencePosCalc;

  std::unique_ptr<CaloRecHitResolutionProvider> _timeResolutionCalcBarrel;
  std::unique_ptr<CaloRecHitResolutionProvider> _timeResolutionCalcEndcap;

  void seedPFClustersFromTopo(const reco::PFCluster&,
                              const std::vector<bool>&,
                              reco::PFClusterCollection&,
                              const HcalPFCuts*) const;

  void growPFClusters(const reco::PFCluster&,
                      const std::vector<bool>&,
                      const unsigned toleranceScaling,
                      const unsigned iter,
                      double dist,
                      reco::PFClusterCollection&,
                      const HcalPFCuts* hcalCuts) const;
  void clusterTimeResolution(reco::PFCluster& cluster, double& res) const;
  void clusterTimeResolutionFromSeed(reco::PFCluster& cluster, double& res) const;
  double dist2Time(const reco::PFCluster&, const reco::PFRecHitRef&, int cell_layer, double prev_timeres2) const;
  bool passChi2Prob(size_t iCluster,
                    double dist2,
                    std::vector<double>& clus_chi2,
                    std::vector<size_t>& clus_chi2_nhits) const;
  void prunePFClusters(reco::PFClusterCollection&) const;
};

DEFINE_EDM_PLUGIN(PFClusterBuilderFactory, PFlow2DClusterizerWithTime, "PFlow2DClusterizerWithTime");

#ifdef PFLOW_DEBUG
#define LOGVERB(x) edm::LogVerbatim(x)
#define LOGWARN(x) edm::LogWarning(x)
#define LOGERR(x) edm::LogError(x)
#define LOGDRESSED(x) edm::LogInfo(x)
#else
#define LOGVERB(x) LogTrace(x)
#define LOGWARN(x) edm::LogWarning(x)
#define LOGERR(x) edm::LogError(x)
#define LOGDRESSED(x) LogDebug(x)
#endif

PFlow2DClusterizerWithTime::PFlow2DClusterizerWithTime(const edm::ParameterSet& conf, edm::ConsumesCollector& cc)
    : PFClusterBuilderBase(conf, cc),
      _maxIterations(conf.getParameter<unsigned>("maxIterations")),
      _stoppingTolerance(conf.getParameter<double>("stoppingTolerance")),
      _showerSigma2(std::pow(conf.getParameter<double>("showerSigma"), 2.0)),
      _timeSigma_eb(std::pow(conf.getParameter<double>("timeSigmaEB"), 2.0)),
      _timeSigma_ee(std::pow(conf.getParameter<double>("timeSigmaEE"), 2.0)),
      _excludeOtherSeeds(conf.getParameter<bool>("excludeOtherSeeds")),
      _minFracTot(conf.getParameter<double>("minFracTot")),
      _maxNSigmaTime(std::pow(conf.getParameter<double>("maxNSigmaTime"), 2.0)),
      _minChi2Prob(conf.getParameter<double>("minChi2Prob")),
      _clusterTimeResFromSeed(conf.getParameter<bool>("clusterTimeResFromSeed")),

      _layerMap({{"PS2", (int)PFLayer::PS2},
                 {"PS1", (int)PFLayer::PS1},
                 {"ECAL_ENDCAP", (int)PFLayer::ECAL_ENDCAP},
                 {"ECAL_BARREL", (int)PFLayer::ECAL_BARREL},
                 {"NONE", (int)PFLayer::NONE},
                 {"HCAL_BARREL1", (int)PFLayer::HCAL_BARREL1},
                 {"HCAL_BARREL2_RING0", (int)PFLayer::HCAL_BARREL2},
                 {"HCAL_BARREL2_RING1", 100 * (int)PFLayer::HCAL_BARREL2},
                 {"HCAL_ENDCAP", (int)PFLayer::HCAL_ENDCAP},
                 {"HF_EM", (int)PFLayer::HF_EM},
                 {"HF_HAD", (int)PFLayer::HF_HAD}}) {
  const std::vector<edm::ParameterSet>& thresholds = conf.getParameterSetVector("recHitEnergyNorms");
  for (const auto& pset : thresholds) {
    const std::string& det = pset.getParameter<std::string>("detector");
    const double& rhE_norm = pset.getParameter<double>("recHitEnergyNorm");
    auto entry = _layerMap.find(det);
    if (entry == _layerMap.end()) {
      throw cms::Exception("InvalidDetectorLayer") << "Detector layer : " << det << " is not in the list of recognized"
                                                   << " detector layers!";
    }
    _recHitEnergyNorms.emplace(_layerMap.find(det)->second, rhE_norm);
  }

  const auto& acConf = conf.getParameterSet("allCellsPositionCalc");
  if (!acConf.empty()) {
    const std::string& algoac = acConf.getParameter<std::string>("algoName");
    if (!algoac.empty())
      _allCellsPosCalc = PFCPositionCalculatorFactory::get()->create(algoac, acConf, cc);
  }
  // if necessary a third pos calc for convergence testing
  const auto& convConf = conf.getParameterSet("positionCalcForConvergence");
  if (!convConf.empty()) {
    const std::string& algoconv = convConf.getParameter<std::string>("algoName");
    if (!algoconv.empty())
      _convergencePosCalc = PFCPositionCalculatorFactory::get()->create(algoconv, convConf, cc);
  }
  const auto& timeResConfBarrel = conf.getParameterSet("timeResolutionCalcBarrel");
  if (!timeResConfBarrel.empty())
    _timeResolutionCalcBarrel = std::make_unique<CaloRecHitResolutionProvider>(timeResConfBarrel);
  const auto& timeResConfEndcap = conf.getParameterSet("timeResolutionCalcEndcap");
  if (!timeResConfEndcap.empty())
    _timeResolutionCalcEndcap = std::make_unique<CaloRecHitResolutionProvider>(timeResConfEndcap);
}

void PFlow2DClusterizerWithTime::buildClusters(const reco::PFClusterCollection& input,
                                               const std::vector<bool>& seedable,
                                               reco::PFClusterCollection& output,
                                               const HcalPFCuts* hcalCuts) {
  reco::PFClusterCollection clustersInTopo;
  for (const auto& topocluster : input) {
    clustersInTopo.clear();
    seedPFClustersFromTopo(topocluster, seedable, clustersInTopo, hcalCuts);
    const unsigned tolScal = std::pow(std::max(1.0, clustersInTopo.size() - 1.0), 2.0);
    growPFClusters(topocluster, seedable, tolScal, 0, tolScal, clustersInTopo, hcalCuts);
    // step added by Josh Bendavid, removes low-fraction clusters
    // did not impact position resolution with fraction cut of 1e-7
    // decreases the size of each pf cluster considerably
    prunePFClusters(clustersInTopo);
    // recalculate the positions of the pruned clusters
    if (_convergencePosCalc) {
      // if defined, use the special position calculation for convergence tests
      _convergencePosCalc->calculateAndSetPositions(clustersInTopo, hcalCuts);
    } else {
      if (clustersInTopo.size() == 1 && _allCellsPosCalc) {
        _allCellsPosCalc->calculateAndSetPosition(clustersInTopo.back(), hcalCuts);
      } else {
        _positionCalc->calculateAndSetPositions(clustersInTopo, hcalCuts);
      }
    }
    for (auto& clusterout : clustersInTopo) {
      output.insert(output.end(), std::move(clusterout));
    }
  }
}

void PFlow2DClusterizerWithTime::seedPFClustersFromTopo(const reco::PFCluster& topo,
                                                        const std::vector<bool>& seedable,
                                                        reco::PFClusterCollection& initialPFClusters,
                                                        const HcalPFCuts* hcalCuts) const {
  const auto& recHitFractions = topo.recHitFractions();
  for (const auto& rhf : recHitFractions) {
    if (!seedable[rhf.recHitRef().key()])
      continue;
    initialPFClusters.push_back(reco::PFCluster());
    reco::PFCluster& current = initialPFClusters.back();
    current.addRecHitFraction(rhf);
    current.setSeed(rhf.recHitRef()->detId());
    if (_convergencePosCalc) {
      _convergencePosCalc->calculateAndSetPosition(current, hcalCuts);
    } else {
      _positionCalc->calculateAndSetPosition(current, hcalCuts);
    }
  }
}

void PFlow2DClusterizerWithTime::growPFClusters(const reco::PFCluster& topo,
                                                const std::vector<bool>& seedable,
                                                const unsigned toleranceScaling,
                                                const unsigned iter,
                                                double diff,
                                                reco::PFClusterCollection& clusters,
                                                const HcalPFCuts* hcalCuts) const {
  if (iter >= _maxIterations) {
    LOGDRESSED("PFlow2DClusterizerWithTime:growAndStabilizePFClusters")
        << "reached " << _maxIterations << " iterations, terminated position "
        << "fit with diff = " << diff;
  }
  if (iter >= _maxIterations || diff <= _stoppingTolerance * toleranceScaling)
    return;
  // reset the rechits in this cluster, keeping the previous position
  std::vector<reco::PFCluster::REPPoint> clus_prev_pos;
  // also calculate and keep the previous time resolution
  std::vector<double> clus_prev_timeres2;

  for (auto& cluster : clusters) {
    const reco::PFCluster::REPPoint& repp = cluster.positionREP();
    clus_prev_pos.emplace_back(repp.rho(), repp.eta(), repp.phi());
    if (_convergencePosCalc) {
      if (clusters.size() == 1 && _allCellsPosCalc) {
        _allCellsPosCalc->calculateAndSetPosition(cluster, hcalCuts);
      } else {
        _positionCalc->calculateAndSetPosition(cluster, hcalCuts);
      }
    }
    double resCluster2;
    if (_clusterTimeResFromSeed)
      clusterTimeResolutionFromSeed(cluster, resCluster2);
    else
      clusterTimeResolution(cluster, resCluster2);
    clus_prev_timeres2.push_back(resCluster2);
    cluster.resetHitsAndFractions();
  }
  // loop over topo cluster and grow current PFCluster hypothesis
  std::vector<double> dist2, frac;

  // Store chi2 values and nhits to calculate chi2 prob. inline
  std::vector<double> clus_chi2;
  std::vector<size_t> clus_chi2_nhits;

  double fractot = 0;
  for (const reco::PFRecHitFraction& rhf : topo.recHitFractions()) {
    const reco::PFRecHitRef& refhit = rhf.recHitRef();
    int cell_layer = (int)refhit->layer();
    if (cell_layer == PFLayer::HCAL_BARREL2 && std::abs(refhit->positionREP().eta()) > 0.34) {
      cell_layer *= 100;
    }
    double recHitEnergyNorm = _recHitEnergyNorms.find(cell_layer)->second;
    if (hcalCuts != nullptr) {  // this means, cutsFromDB is set to True in the producer code
      if ((cell_layer == PFLayer::HCAL_BARREL1) || (cell_layer == PFLayer::HCAL_ENDCAP)) {
        HcalDetId thisId = refhit->detId();
        const HcalPFCut* item = hcalCuts->getValues(thisId.rawId());
        recHitEnergyNorm = item->noiseThreshold();
      }
    }

    math::XYZPoint topocellpos_xyz(refhit->position());
    dist2.clear();
    frac.clear();
    fractot = 0;

    // add rechits to clusters, calculating fraction based on distance
    // need position in vector to get cluster time resolution
    for (size_t iCluster = 0; iCluster < clusters.size(); ++iCluster) {
      reco::PFCluster& cluster = clusters[iCluster];
      const math::XYZPoint& clusterpos_xyz = cluster.position();
      const math::XYZVector deltav = clusterpos_xyz - topocellpos_xyz;
      double d2 = deltav.Mag2() / _showerSigma2;

      double d2time = dist2Time(cluster, refhit, cell_layer, clus_prev_timeres2[iCluster]);
      d2 += d2time;

      if (_minChi2Prob > 0. && !passChi2Prob(iCluster, d2time, clus_chi2, clus_chi2_nhits))
        d2 = 999.;

      dist2.emplace_back(d2);

      if (d2 > 100) {
        LOGDRESSED("PFlow2DClusterizerWithTime:growAndStabilizePFClusters")
            << "Warning! :: pfcluster-topocell distance is too large! d= " << d2;
      }
      // fraction assignment logic
      double fraction;
      if (refhit->detId() == cluster.seed() && _excludeOtherSeeds) {
        fraction = 1.0;
      } else if (seedable[refhit.key()] && _excludeOtherSeeds) {
        fraction = 0.0;
      } else {
        fraction = cluster.energy() / recHitEnergyNorm * vdt::fast_expf(-0.5 * d2);
      }
      fractot += fraction;
      frac.emplace_back(fraction);
    }
    for (unsigned i = 0; i < clusters.size(); ++i) {
      if (fractot > _minFracTot || (refhit->detId() == clusters[i].seed() && fractot > 0.0)) {
        frac[i] /= fractot;
      } else {
        continue;
      }
      // if the fraction has been set to 0, the cell
      // is now added to the cluster - careful ! (PJ, 19/07/08)
      // BUT KEEP ONLY CLOSE CELLS OTHERWISE MEMORY JUST EXPLOSES
      // (PJ, 15/09/08 <- similar to what existed before the
      // previous bug fix, but keeps the close seeds inside,
      // even if their fraction was set to zero.)
      // Also add a protection to keep the seed in the cluster
      // when the latter gets far from the former. These cases
      // (about 1% of the clusters) need to be studied, as
      // they create fake photons, in general.
      // (PJ, 16/09/08)
      if (dist2[i] < 100.0 || frac[i] > 0.9999) {
        clusters[i].addRecHitFraction(reco::PFRecHitFraction(refhit, frac[i]));
      }
    }
  }
  // recalculate positions and calculate convergence parameter
  double diff2 = 0.0;
  for (unsigned i = 0; i < clusters.size(); ++i) {
    if (_convergencePosCalc) {
      _convergencePosCalc->calculateAndSetPosition(clusters[i], hcalCuts);
    } else {
      if (clusters.size() == 1 && _allCellsPosCalc) {
        _allCellsPosCalc->calculateAndSetPosition(clusters[i], hcalCuts);
      } else {
        _positionCalc->calculateAndSetPosition(clusters[i], hcalCuts);
      }
    }
    const double delta2 = reco::deltaR2(clusters[i].positionREP(), clus_prev_pos[i]);
    if (delta2 > diff2)
      diff2 = delta2;
  }
  diff = std::sqrt(diff2);
  dist2.clear();
  frac.clear();
  clus_prev_pos.clear();  // avoid badness
  clus_chi2.clear();
  clus_chi2_nhits.clear();
  clus_prev_timeres2.clear();
  growPFClusters(topo, seedable, toleranceScaling, iter + 1, diff, clusters, hcalCuts);
}

void PFlow2DClusterizerWithTime::prunePFClusters(reco::PFClusterCollection& clusters) const {
  for (auto& cluster : clusters) {
    cluster.pruneUsing([&](const reco::PFRecHitFraction& rhf) { return rhf.fraction() > _minFractionToKeep; });
  }
}

void PFlow2DClusterizerWithTime::clusterTimeResolutionFromSeed(reco::PFCluster& cluster, double& clusterRes2) const {
  clusterRes2 = 10000.;
  for (auto& rhf : cluster.recHitFractions()) {
    const reco::PFRecHit& rh = *(rhf.recHitRef());
    if (rh.detId() == cluster.seed()) {
      cluster.setTime(rh.time());
      bool isBarrel = (rh.layer() == PFLayer::HCAL_BARREL1 || rh.layer() == PFLayer::HCAL_BARREL2 ||
                       rh.layer() == PFLayer::ECAL_BARREL);
      if (isBarrel)
        clusterRes2 = _timeResolutionCalcBarrel->timeResolution2(rh.energy());
      else
        clusterRes2 = _timeResolutionCalcEndcap->timeResolution2(rh.energy());
      cluster.setTimeError(std::sqrt(float(clusterRes2)));
    }
  }
}

void PFlow2DClusterizerWithTime::clusterTimeResolution(reco::PFCluster& cluster, double& clusterRes2) const {
  double sumTimeSigma2 = 0.;
  double sumSigma2 = 0.;

  for (auto& rhf : cluster.recHitFractions()) {
    const reco::PFRecHit& rh = *(rhf.recHitRef());
    const double rhf_f = rhf.fraction();

    if (rhf_f == 0.)
      continue;

    bool isBarrel = (rh.layer() == PFLayer::HCAL_BARREL1 || rh.layer() == PFLayer::HCAL_BARREL2 ||
                     rh.layer() == PFLayer::ECAL_BARREL);
    double res2 = 10000.;
    if (isBarrel)
      res2 = _timeResolutionCalcBarrel->timeResolution2(rh.energy());
    else
      res2 = _timeResolutionCalcEndcap->timeResolution2(rh.energy());

    sumTimeSigma2 += rhf_f * rh.time() / res2;
    sumSigma2 += rhf_f / res2;
  }
  if (sumSigma2 > 0.) {
    clusterRes2 = 1. / sumSigma2;
    cluster.setTime(sumTimeSigma2 / sumSigma2);
    cluster.setTimeError(std::sqrt(float(clusterRes2)));
  } else {
    clusterRes2 = 999999.;
  }
}

double PFlow2DClusterizerWithTime::dist2Time(const reco::PFCluster& cluster,
                                             const reco::PFRecHitRef& refhit,
                                             int cell_layer,
                                             double prev_timeres2) const {
  const double deltaT = cluster.time() - refhit->time();
  const double t2 = deltaT * deltaT;
  double res2 = 100.;

  if (cell_layer == PFLayer::HCAL_BARREL1 || cell_layer == PFLayer::HCAL_BARREL2 ||
      cell_layer == PFLayer::ECAL_BARREL) {
    if (_timeResolutionCalcBarrel) {
      const double resCluster2 = prev_timeres2;
      res2 = resCluster2 + _timeResolutionCalcBarrel->timeResolution2(refhit->energy());
    } else {
      return t2 / _timeSigma_eb;
    }
  } else if (cell_layer == PFLayer::HCAL_ENDCAP || cell_layer == PFLayer::HF_EM || cell_layer == PFLayer::HF_HAD ||
             cell_layer == PFLayer::ECAL_ENDCAP) {
    if (_timeResolutionCalcEndcap) {
      const double resCluster2 = prev_timeres2;
      res2 = resCluster2 + _timeResolutionCalcEndcap->timeResolution2(refhit->energy());
    } else {
      return t2 / _timeSigma_ee;
    }
  }

  double distTime2 = t2 / res2;
  if (distTime2 > _maxNSigmaTime)
    return 999.;  // reject hit

  return distTime2;
}

bool PFlow2DClusterizerWithTime::passChi2Prob(size_t iCluster,
                                              double dist2,
                                              std::vector<double>& clus_chi2,
                                              std::vector<size_t>& clus_chi2_nhits) const {
  if (iCluster >= clus_chi2.size()) {  // first hit
    clus_chi2.push_back(dist2);
    clus_chi2_nhits.push_back(1);
    return true;
  } else {
    double chi2 = clus_chi2[iCluster];
    size_t nhitsCluster = clus_chi2_nhits[iCluster];
    chi2 += dist2;
    if (TMath::Prob(chi2, nhitsCluster) >= _minChi2Prob) {
      clus_chi2[iCluster] = chi2;
      clus_chi2_nhits[iCluster] = nhitsCluster + 1;
      return true;
    }
  }
  return false;
}

// Original author: Leonardo Cristella

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/global/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/EDGetToken.h"
#include "FWCore/Utilities/interface/ESGetToken.h"

#include "SimDataFormats/Associations/interface/MultiClusterToCaloParticleAssociator.h"
#include "MultiClusterAssociatorByEnergyScoreImpl.h"

#include "DataFormats/HGCRecHit/interface/HGCRecHitCollections.h"

class MultiClusterAssociatorByEnergyScoreProducer : public edm::global::EDProducer<> {
public:
  explicit MultiClusterAssociatorByEnergyScoreProducer(const edm::ParameterSet &);
  ~MultiClusterAssociatorByEnergyScoreProducer() override;

  static void fillDescriptions(edm::ConfigurationDescriptions &descriptions);

private:
  void produce(edm::StreamID, edm::Event &, const edm::EventSetup &) const override;
  edm::EDGetTokenT<std::unordered_map<DetId, const unsigned int>> hitMap_;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeometry_;
  const bool hardScatterOnly_;
  std::shared_ptr<hgcal::RecHitTools> rhtools_;
  std::vector<edm::InputTag> hits_label_;
  std::vector<edm::EDGetTokenT<HGCRecHitCollection>> hits_token_;
};

MultiClusterAssociatorByEnergyScoreProducer::MultiClusterAssociatorByEnergyScoreProducer(const edm::ParameterSet &ps)
    : hitMap_(consumes<std::unordered_map<DetId, const unsigned int>>(ps.getParameter<edm::InputTag>("hitMapTag"))),
      caloGeometry_(esConsumes<CaloGeometry, CaloGeometryRecord>()),
      hardScatterOnly_(ps.getParameter<bool>("hardScatterOnly")),
      hits_label_(ps.getParameter<std::vector<edm::InputTag>>("hits")) {
  for (auto &label : hits_label_) {
    hits_token_.push_back(consumes<HGCRecHitCollection>(label));
  }
  rhtools_.reset(new hgcal::RecHitTools());

  // Register the product
  produces<hgcal::MultiClusterToCaloParticleAssociator>();
}

MultiClusterAssociatorByEnergyScoreProducer::~MultiClusterAssociatorByEnergyScoreProducer() {}

void MultiClusterAssociatorByEnergyScoreProducer::produce(edm::StreamID,
                                                          edm::Event &iEvent,
                                                          const edm::EventSetup &es) const {
  edm::ESHandle<CaloGeometry> geom = es.getHandle(caloGeometry_);
  rhtools_->setGeometry(*geom);

  std::vector<const HGCRecHit *> hits;
  for (auto &token : hits_token_) {
    edm::Handle<HGCRecHitCollection> hits_handle;
    iEvent.getByToken(token, hits_handle);
    for (const auto &hit : *hits_handle) {
      hits.push_back(&hit);
    }
  }

  const std::unordered_map<DetId, const unsigned int> *hitMap = &iEvent.get(hitMap_);

  auto impl = std::make_unique<MultiClusterAssociatorByEnergyScoreImpl>(
      iEvent.productGetter(), hardScatterOnly_, rhtools_, hitMap, hits);
  auto toPut = std::make_unique<hgcal::MultiClusterToCaloParticleAssociator>(std::move(impl));
  iEvent.put(std::move(toPut));
}

void MultiClusterAssociatorByEnergyScoreProducer::fillDescriptions(edm::ConfigurationDescriptions &cfg) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("hitMapTag", edm::InputTag("recHitMapProducer", "hgcalRecHitMap"));
  desc.add<std::vector<edm::InputTag>>("hits",
                                       {edm::InputTag("HGCalRecHit", "HGCEERecHits"),
                                        edm::InputTag("HGCalRecHit", "HGCHEFRecHits"),
                                        edm::InputTag("HGCalRecHit", "HGCHEBRecHits")});
  desc.add<bool>("hardScatterOnly", true);

  cfg.add("multiClusterAssociatorByEnergyScore", desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(MultiClusterAssociatorByEnergyScoreProducer);

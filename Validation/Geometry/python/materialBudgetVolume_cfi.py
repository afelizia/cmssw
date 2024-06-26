import FWCore.ParameterSet.Config as cms
from SimG4Core.Configuration.SimG4Core_cff import *

g4SimHits.Watchers = cms.VPSet(cms.PSet(
    MaterialBudgetVolume = cms.PSet(
        lvNames = cms.vstring('BEAM', 'BEAM1', 'BEAM2', 'BEAM3', 'BEAM4', 'Tracker', 'ECAL', 'HCal', 'VCAL', 'MGNT', 'MUON', 'OQUA', 'CALOEC', 'HFNoseVol'),
        lvLevels = cms.vint32(3, 3, 3, 3, 3, 3, 4, 4, 3, 4, 3, 3, 4, 3),
        useDD4hep = cms.bool(False),
        rMax = cms.double(-1.),
        zMax = cms.double(-1.),
    ),
    type = cms.string('MaterialBudgetVolume'),
))

from Configuration.ProcessModifiers.dd4hep_cff import dd4hep
dd4hep.toModify( g4SimHits.Watchers[0].MaterialBudgetVolume, useDD4hep = True )

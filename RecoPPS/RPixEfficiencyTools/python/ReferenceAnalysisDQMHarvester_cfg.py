import FWCore.ParameterSet.Config as cms
from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer
import FWCore.ParameterSet.VarParsing as VarParsing
from Configuration.StandardSequences.Eras import eras
from Configuration.AlCa.GlobalTag import GlobalTag
from DQMServices.Core.DQMEDHarvester import DQMEDHarvester


#SETUP PROCESS
process = cms.Process("DQMHarvesterProcess", eras.Run2_2018,eras.run2_miniAOD_devel)


#SPECIFY INPUT PARAMETERS
process.options = cms.untracked.PSet(
    wantSummary = cms.untracked.bool(False),
    FailPath = cms.untracked.vstring('Type Mismatch')
    )
options = VarParsing.VarParsing()
options.register('inputFileName', # parameter name 
                '', # default value - empty means no default value
                VarParsing.VarParsing.multiplicity.singleton,
                VarParsing.VarParsing.varType.string,
                "input ROOT file name (file created by DQMWorker)")

options.register('outputDirectoryPath',
                './OutputFiles/',
                VarParsing.VarParsing.multiplicity.singleton,
                VarParsing.VarParsing.varType.string,
                "directory in which the output ROOT file will be saved")
options.parseArguments()


#PREPARE LOGGER
process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger = cms.Service("MessageLogger",
    destinations = cms.untracked.vstring('cout'),
    cout = cms.untracked.PSet( 
        optionalPSet = cms.untracked.bool(True),
        INFO = cms.untracked.PSet(
            limit = cms.untracked.int32(0)
        ),
        noTimeStamps = cms.untracked.bool(False),
        FwkReport = cms.untracked.PSet(
            optionalPSet = cms.untracked.bool(True),
            reportEvery = cms.untracked.int32(10000),
            # reportEvery = cms.untracked.int32(1),
            limit = cms.untracked.int32(50000000)
        ),
        default = cms.untracked.PSet(
            limit = cms.untracked.int32(10000000)
        ),
        threshold = cms.untracked.string('INFO')
        ),
    categories = cms.untracked.vstring(
        "FwkReport"
        ),
)
process.MessageLogger.statistics = cms.untracked.vstring()


#LOAD NECCESSARY DEPENDENCIES
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')
process.load("DQM.Integration.config.environment_cfi")
process.load("DQMServices.Components.DQMEnvironment_cfi")
process.load("Geometry.VeryForwardGeometry.geometryRPFromDD_2018_cfi")

#SETUP GLOBAL TAG
process.GlobalTag = GlobalTag(process.GlobalTag, '123X_dataRun2_v4')


#PREPARE SOURCE
process.source = cms.Source("DQMRootSource",
    fileNames = cms.untracked.vstring("file:"+options.inputFileName),
)

#SETUP HARVESTER
process.harvester = DQMEDHarvester('ReferenceAnalysisDQMHarvester',
    
)


#CONFIGURE DQM Saver
process.dqmEnv.subSystemFolder = "CalibPPS"
process.dqmSaver.convention = 'Offline'
process.dqmSaver.workflow = "/CalibPPS/AlignmentGlobal/CMSSW_11_3_0_pre4" #TODO CalibPPS/AlignmentGlobal' is inherited from somewhere else - replace it
process.dqmSaver.saveByRun = -1
process.dqmSaver.saveAtJobEnd = True
process.dqmSaver.forceRunNumber = 999999
process.dqmSaver.dirName = options.outputDirectoryPath # todo confirm if this works

#SCHEDULE JOB
process.path = cms.Path(
    process.harvester
)

process.end_path = cms.EndPath(
    process.dqmSaver
)

process.schedule = cms.Schedule(
    process.path,
    process.end_path
)
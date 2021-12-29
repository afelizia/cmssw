#include "SimTransport/PPSProtonTransport/interface/TotemTransport.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include <CLHEP/Units/GlobalSystemOfUnits.h>
#include "TLorentzVector.h"
#include "TFile.h"

#include <cmath>

TotemTransport::TotemTransport(const edm::ParameterSet& iConfig)
    : BaseProtonTransport(iConfig),
      m_model_ip_150_r_name_(iConfig.getParameter<std::string>("Model_IP_150_R_Name")),
      m_model_ip_150_l_name_(iConfig.getParameter<std::string>("Model_IP_150_L_Name")),
      m_beampipe_aperture_radius_(iConfig.getParameter<double>("BeampipeApertureRadius")) {
  MODE = TransportMode::TOTEM;
  if (fPPSRegionStart_56_ > 0)
    fPPSRegionStart_56_ *= -1;  // make sure sector 56 has negative position, as TOTEM convention

  edm::LogVerbatim("TotemTransport")
      << "=============================================================================\n"
      << "             Bulding LHC Proton transporter based on TOTEM model\n"
      << "=============================================================================\n";

  m_aprox_ip_150_r_ = ReadParameterization(m_model_ip_150_r_name_, beam1Filename_);
  m_aprox_ip_150_l_ = ReadParameterization(m_model_ip_150_l_name_, beam2Filename_);

  if (m_aprox_ip_150_r_ == nullptr || m_aprox_ip_150_l_ == nullptr) {
    edm::LogError("TotemTransport") << "Parameterisation " << m_model_ip_150_r_name_ << " or " << m_model_ip_150_l_name_
                                    << " missing in file. Cannot proceed. ";
    throw edm::Exception(edm::errors::Configuration) << "TotemTransport is not properly initialized";
  }
  edm::LogVerbatim("TotemTransport") << "Parameterizations read from file, pointers:" << m_aprox_ip_150_r_ << " "
                                     << m_aprox_ip_150_l_ << " ";
}
TotemTransport::~TotemTransport() {}
//
// this method is the same for all propagator, but since transportProton is different for each derived class
// it needes to be overriden
//
void TotemTransport::process(const HepMC::GenEvent* ievt,
                             const edm::EventSetup& iSetup,
                             CLHEP::HepRandomEngine* engine) {
  clear();
  engine_ = engine;  // the engine needs to be updated for each event

  HepMC::GenEvent evt(*ievt);

  for (HepMC::GenEvent::particle_const_iterator eventParticle = evt.particles_begin();
       eventParticle != evt.particles_end();
       ++eventParticle) {
    if (!((*eventParticle)->status() == 1 && (*eventParticle)->pdg_id() == 2212))
      continue;

    if (!(fabs((*eventParticle)->momentum().eta()) > etaCut_ && fabs((*eventParticle)->momentum().pz()) > momentumCut_))
      continue;  // discard protons outside kinematic acceptance

    unsigned int line = (*eventParticle)->barcode();
    HepMC::GenParticle* gpart = (*eventParticle);
    if (gpart->pdg_id() != 2212)
      continue;  // only transport stable protons
    if (gpart->status() != 1)
      continue;
    if (m_beamPart.find(line) != m_beamPart.end())  // assures this protons has not been already propagated
      continue;

    transportProton(gpart);
  }
}
//
//
// here comes the real thing
//
//
bool TotemTransport::transportProton(HepMC::GenParticle* in_trk) {
  //
  edm::LogVerbatim("TotemTransport") << "Starting proton transport using TOTEM method\n";
  //
  ApplyBeamCorrection(in_trk);

  const HepMC::GenVertex* in_pos = in_trk->production_vertex();
  const HepMC::FourVector in_mom = in_trk->momentum();
  //
  // ATTENTION: HepMC uses mm, vertex config of CMS uses cm and SimTransport uses mm
  //
  double in_position[3] = {in_pos->position().x(), in_pos->position().y(), in_pos->position().z()};  //in LHC ref. frame

  double crossingAngleX = (in_mom.z() > 0) ? fCrossingAngleX_45_ : fCrossingAngleX_56_;

  // Move the position to z=0. Do it in the CMS ref frame. Totem parameterization does the rotation internatlly
  in_position[0] =
      in_position[0] - in_position[2] * (in_mom.x() / in_mom.z() - crossingAngleX * urad);  // in CMS ref. frame
  in_position[1] = in_position[1] - in_position[2] * (in_mom.y() / (in_mom.z()));
  in_position[2] = 0.;
  double in_momentum[3] = {in_mom.x(), in_mom.y(), in_mom.z()};
  double out_position[3];
  double out_momentum[3];
  edm::LogVerbatim("TotemTransport") << "before transport ->"
                                     << " position: " << in_position[0] << ", " << in_position[1] << ", "
                                     << in_position[2] << " momentum: " << in_momentum[0] << ", " << in_momentum[1]
                                     << ", " << in_momentum[2];

  LHCOpticsApproximator* approximator = nullptr;
  double zin;
  double zout;
  if (in_mom.z() > 0) {
    approximator = m_aprox_ip_150_l_;
    zin = 0.0;  // Totem propagations assumes the starting point at 0 (zero)
    zout = fPPSRegionStart_45_;
  } else {
    approximator = m_aprox_ip_150_r_;
    zin = 0.0;  // Totem propagations assumes the starting point at 0 (zero)
    zout = fPPSRegionStart_56_;
  }

  bool invert_beam_coord_system =
      true;  // it doesn't matter the option here, it is hard coded as TRUE inside LHCOpticsApproximator!

  bool tracked = approximator->Transport_m_GeV(
      in_position, in_momentum, out_position, out_momentum, invert_beam_coord_system, zout - zin);

  if (!tracked)
    return false;

  edm::LogVerbatim("TotemTransport") << "after transport -> "
                                     << "position: " << out_position[0] << ", " << out_position[1] << ", "
                                     << out_position[2] << "momentum: " << out_momentum[0] << ", " << out_momentum[1]
                                     << ", " << out_momentum[2];

  if (out_position[0] * out_position[0] + out_position[1] * out_position[1] >
      m_beampipe_aperture_radius_ * m_beampipe_aperture_radius_) {
    edm::LogVerbatim("TotemTransport") << "Proton ouside beampipe\n"
                                       << "===== END Transport "
                                       << "====================";
    return false;
  }

  TVector3 out_pos(out_position[0] * meter, out_position[1] * meter, out_position[2] * meter);
  TVector3 out_mom(out_momentum[0], out_momentum[1], out_momentum[2]);

  if (verbosity_) {
    LogDebug("TotemTransport") << "output -> position: ";
    out_pos.Print();
    LogDebug("TotemTransport") << " momentum: ";
    out_mom.Print();
  }

  double px = -out_momentum[0];  // calculates px by means of TH_X, which is in the LHC ref. frame.
  double py = out_momentum[1];   // this need to be checked again, since it seems an invertion is occuring in the prop.
  double pz =
      out_momentum[2];  // totem calculates output pz already in the CMS ref. frame, it doesn't need to be converted
  double e = sqrt(px * px + py * py + pz * pz + ProtonMassSQ);
  TLorentzVector p_out(px, py, pz, e);
  double x1_ctpps = -out_position[0] * meter;  // Totem parameterization uses meter, one need it in millimeter
  double y1_ctpps = out_position[1] * meter;

  unsigned int line = in_trk->barcode();

  if (verbosity_)
    LogDebug("TotemTransport") << "TotemTransport:filterPPS: barcode = " << line << " x=  " << x1_ctpps
                               << " y= " << y1_ctpps;

  m_beamPart[line] = p_out;
  m_xAtTrPoint[line] = x1_ctpps;
  m_yAtTrPoint[line] = y1_ctpps;
  return true;
}
//
LHCOpticsApproximator* TotemTransport::ReadParameterization(const std::string& m_model_name,
                                                            const std::string& rootfile) {
  edm::FileInPath fileName(rootfile.c_str());
  TFile* f = TFile::Open(fileName.fullPath().c_str(), "read");
  if (!f) {
    edm::LogError("TotemTransport") << "File " << fileName << " not found. Exiting.";
    return nullptr;
  }
  edm::LogVerbatim("TotemTransport") << "Root file opened, pointer:" << f;

  // read parametrization
  LHCOpticsApproximator* aprox = (LHCOpticsApproximator*)f->Get(m_model_name.c_str());
  f->Close();
  return aprox;
}


#include "SimG4Core/CustomPhysics/interface/G4SQ.h"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "G4ParticleTable.hh"

#include "G4PhaseSpaceDecayChannel.hh"
#include "G4DecayTable.hh"

// ######################################################################
// ###                          SEXAQUARK                             ###
// ######################################################################

G4SQ* G4SQ::theInstance = 0;

G4SQ* G4SQ::Definition(double mass)
{
  if (theInstance !=0) return theInstance;
  const G4String name = "sexaq";
  // search in particle table]
  G4ParticleTable* pTable = G4ParticleTable::GetParticleTable();
  G4ParticleDefinition* anInstance = pTable->FindParticle(name);
  if (anInstance ==0)
  {
  // create particle
  //
  //    Arguments for constructor are as follows
  //               name             mass          width         charge
  //             2*spin           parity  C-conjugation
  //          2*Isospin       2*Isospin3       G-parity   
  //               type    lepton number  baryon number   PDG encoding
  //             stable         lifetime    decay table
  //             shortlived      subType    anti_encoding
  
    anInstance = new G4ParticleDefinition(
                 name,            mass,             0,         0.0,
                    0,              +1,             0,
                    0,               0,             0,             
             "baryon",               0,            +2,   1020000020,
                 true,            -1.0,          NULL,
                false,         "sexaq");
 

  }
  theInstance = reinterpret_cast<G4SQ*>(anInstance);
  return theInstance;
}

//G4SQ* G4SQ::SQDefinition(double mass)
//{ 
//  return Definition(mass);
//}

G4SQ*  G4SQ::SQ(double mass)
{ 
  return Definition(mass * GeV); // will use correct mass if instance exists
}



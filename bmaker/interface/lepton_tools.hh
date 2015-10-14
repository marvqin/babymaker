// LEPTON_TOOLS: Functions to select analysis leptons

#ifndef H_LEPTON_TOOLS
#define H_LEPTON_TOOLS

#include "FWCore/Framework/interface/ESHandle.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/Electron.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"

#include "babymaker/bmaker/interface/utilities.hh"

class lepton_tools{

public:
  lepton_tools();
  ~lepton_tools();

  ///////////////// LEPTON CUTS ///////////////////////
  const float SignalLeptonPtCut	 = 20.0;
  const float VetoLeptonPtCut	 = 10.0;
  const float MuonEtaCut	 = 2.4;
  const float ElectronEtaCut	 = 2.5;
  const float MuonMiniIsoCut	 = 0.2;
  const float ElectronMiniIsoCut = 0.1;

  enum CutLevel{kVeto, kLoose, kMedium, kTight};
  template<class T>
  T chooseVal(CutLevel threshold, T valVeto, T valLoose, T valMedium, T valTight){
    switch(threshold){
      default:
      case kVeto:
        return valVeto;
      case kLoose:
        return valLoose;
      case kMedium:
        return valMedium;
      case kTight:
        return valTight;
    }
    return valVeto;
  }

  bool isVetoMuon(const pat::Muon &lep, edm::Handle<reco::VertexCollection> vtx, double lepIso);
  bool isSignalMuon(const pat::Muon &lep, edm::Handle<reco::VertexCollection> vtx, double lepIso);
  bool idMuon(const pat::Muon &lep, edm::Handle<reco::VertexCollection> vtx, CutLevel threshold);
  bool vertexMuon(const pat::Muon &lep, edm::Handle<reco::VertexCollection> vtx, double &dz, double &d0);
  double getEffAreaMuon(double eta);
  double getRelIsolation(const pat::Muon &lep, double rho);

  bool isVetoElectron(const pat::Electron &lep, edm::Handle<reco::VertexCollection> vtx, double lepIso);
  bool isSignalElectron(const pat::Electron &lep, edm::Handle<reco::VertexCollection> vtx, double lepIso);
  bool idElectron(const pat::Electron &lep, edm::Handle<reco::VertexCollection> vtx, CutLevel threshold, bool doIso=false);
  bool vertexElectron(const pat::Electron &lep, edm::Handle<reco::VertexCollection> vtx, double &dz, double &d0);
  double getEffAreaElectron(double eta);
  double getRelIsolation(const pat::Electron &lep, double rho);

  double getPFIsolation(edm::Handle<pat::PackedCandidateCollection> pfcands,
			const reco::Candidate* ptcl,  
                        double r_iso_min, double r_iso_max, double kt_scale,
                        double rho, bool charged_only);

};

#endif
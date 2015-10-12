//// BMAKER_BASIC: Creates baby tree with basic branches
//// Function names follow the first-lowercase, following words-uppercase. No underscores

// System include files
#include <cmath>
#include <memory>
#include <iostream>
#include <stdlib.h>
#include <iomanip>  // put_time

// FW include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

// FW physics include files
#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include <fastjet/JetDefinition.hh>
#include <fastjet/PseudoJet.hh>
#include <fastjet/ClusterSequence.hh>
#include <fastjet/GhostedAreaSpec.hh>

// ROOT include files
#include "TFile.h"
#include "TROOT.h"

// User include files
#include "babymaker/bmaker/interface/bmaker_basic.hh"
#include "babymaker/bmaker/interface/baby_basic.hh"
#include "babymaker/bmaker/interface/phys_objects.hh"

using namespace std;
using namespace utilities;
using namespace phys_objects;

///////////////////////// analyze: METHOD CALLED EACH EVENT ///////////////////////////
void bmaker_basic::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  nevents++;
  isData = iEvent.isRealData();

  ////////////////////// Event info /////////////////////
  baby.run() = iEvent.id().run();
  baby.event() = iEvent.id().event();
  baby.lumiblock() = iEvent.luminosityBlock();
  if(isData){
    // We are applying the golden JSON with lumisToProcess in bmaker_basic_cfg.py
    bool nonblind(isInJSON("nonblind", baby.run(), baby.lumiblock()));
    //if(!isInJSON("golden", baby.run(), baby.lumiblock()) && !nonblind) return;
    baby.json() = nonblind;
  } else baby.json() = true;

  ////////////////////// Trigger /////////////////////
  edm::Handle<edm::TriggerResults> triggerBits;
  iEvent.getByLabel(edm::InputTag("TriggerResults","","HLT"),triggerBits);  
  edm::Handle<pat::PackedTriggerPrescales> triggerPrescales;
  iEvent.getByLabel("patTrigger",triggerPrescales);  
  const edm::TriggerNames &names = iEvent.triggerNames(*triggerBits);
  // Not saving data events that don't have triggers we care about
  bool triggerFired(writeTriggers(names, triggerBits, triggerPrescales));
  if(!triggerFired && isData) {
    reportTime(iEvent);
    return;
  }

  //////////////// HLT objects //////////////////
  edm::Handle<pat::TriggerObjectStandAloneCollection> triggerObjects;
  iEvent.getByLabel("selectedPatTrigger",triggerObjects);  
  writeHLTObjects(names, triggerObjects);

  //////////////// Weight //////////////////
  const float luminosity = 1000.;
  baby.weight() = 1.;
  if(!isData) {
    edm::Handle<GenEventInfoProduct> gen_event_info;
    iEvent.getByLabel("generator", gen_event_info);
    if (gen_event_info->weight() < 0) baby.weight() *= -1.;
    baby.weight() *= xsec*luminosity / static_cast<double>(nevents_sample);

  }

  ////////////////////// Primary vertices /////////////////////
  edm::Handle<reco::VertexCollection> vtx;
  iEvent.getByLabel("offlineSlimmedPrimaryVertices", vtx);
  edm::Handle<std::vector< PileupSummaryInfo > >  pu_info;
  if(!isData) iEvent.getByLabel("addPileupInfo", pu_info);
  writeVertices(vtx, pu_info);

  ////////////////////// Leptons /////////////////////
  // pfcands, to be used in calculating isolation
  edm::Handle<pat::PackedCandidateCollection> pfcands;
  iEvent.getByLabel("packedPFCandidates", pfcands);

  // Event energy density, to be used in calculating isolation and JECs
  edm::Handle<double> rhoEventCentral_h;
  iEvent.getByLabel("fixedGridRhoFastjetCentralNeutral", rhoEventCentral_h);
  double rhoEventCentral = static_cast<double>(*rhoEventCentral_h);

  vCands sig_leps, veto_leps, sig_mus, veto_mus, sig_els, veto_els;
  edm::Handle<pat::MuonCollection> allmuons;
  iEvent.getByLabel("slimmedMuons", allmuons);
  sig_mus = writeMuons(allmuons, pfcands, vtx, veto_mus, rhoEventCentral);
  edm::Handle<pat::ElectronCollection> allelectrons;
  iEvent.getByLabel("slimmedElectrons", allelectrons);
  sig_els = writeElectrons(allelectrons, pfcands, vtx, veto_els, rhoEventCentral);
  
  writeDiLep(sig_mus, sig_els, veto_mus, veto_els);

  // Putting muons and electrons together
  sig_leps = sig_mus;
  sig_leps.insert(sig_leps.end(), sig_els.begin(), sig_els.end());
  writeLeptons(sig_leps);
  veto_leps = veto_mus;
  veto_leps.insert(veto_leps.end(), veto_els.begin(), veto_els.end());

  //////////////////////////// MET/JETs with JECs ///////////////////////////
  edm::Handle<pat::JetCollection> alljets;
  iEvent.getByLabel(jets_label, alljets);
  edm::Handle<double> rhoEvent_h;
  iEvent.getByLabel( "fixedGridRhoFastjetAll", rhoEvent_h);
  jetTool->getJetCorrections(alljets, *rhoEvent_h);

  /// MET
  edm::Handle<pat::METCollection> mets;
  iEvent.getByLabel(met_label, mets);
  edm::Handle<pat::METCollection> mets_nohf;
  iEvent.getByLabel(met_nohf_label, mets_nohf);
  writeMET(mets, mets_nohf);

  /// Jets
  vector<LVector> jets;
  jets = writeJets(alljets, sig_leps, veto_leps);
  writeFatJets(jets);

  ////////////////////// mT, dphi /////////////////////
  // It requires previous storing of MET
  if(sig_leps.size()>0){
    float wx = baby.met()*cos(baby.met_phi()) + sig_leps[0]->px();
    float wy = baby.met()*sin(baby.met_phi()) + sig_leps[0]->py();
    float wphi = atan2(wy, wx);
    baby.dphi_wlep() = deltaPhi(wphi, sig_leps[0]->phi());

    baby.mt() = sqrt(2.*baby.met()*sig_leps[0]->pt()*(1.-cos(sig_leps[0]->phi()-baby.met_phi())));
    baby.mt_nohf() = sqrt(2.*baby.met_nohf()*sig_leps[0]->pt()*(1.-cos(sig_leps[0]->phi()-baby.met_nohf_phi())));
  }   
    

  ///////////////////// Filters ///////////////////////
  edm::Handle<edm::TriggerResults> filterBits;
  std::string processLabel = isData ? "RECO":"PAT"; // prompt reco runs in the "RECO" process
  iEvent.getByLabel(edm::InputTag("TriggerResults","",processLabel),filterBits);  
  // re-recoed data will have the process label "PAT" rather than "RECO";
  // if the attempt to find data with "RECO" process fails, try "PAT"
  if(!filterBits.isValid() && isData) 
      iEvent.getByLabel(edm::InputTag("TriggerResults", "", "PAT"),filterBits);  
  const edm::TriggerNames &fnames = iEvent.triggerNames(*filterBits);
  // the HBHE noise filter needs to be recomputed in early 2015 data
  edm::Handle<bool> filter_hbhe;
  if(isData && iEvent.getByLabel("HBHENoiseFilterResultProducer","HBHENoiseFilterResult",filter_hbhe)) { 
    if(*filter_hbhe) baby.pass_hbhe() = true;
    else baby.pass_hbhe() = false;
  }
  writeFilters(fnames, filterBits, vtx);

  ///////////////////// Truth Info ///////////////////////
  if (!isData) {
    edm::Handle<LHEEventProduct> lhe_info;
    iEvent.getByLabel("externalLHEProducer", lhe_info);
    if(!lhe_info.isValid()) iEvent.getByLabel("source", lhe_info);
    writeGenInfo(lhe_info);
    edm::Handle<reco::GenParticleCollection> genParticles;
    iEvent.getByLabel("prunedGenParticles", genParticles);
    writeMC(genParticles);
  }

  ////////////////// Filling the tree //////////////////
  baby.Fill();

  reportTime(iEvent);

}


/*
______                      _                     _ _   _             
| ___ \                    | |                   (_) | (_)            
| |_/ /_ __ __ _ _ __   ___| |__   __      ___ __ _| |_ _ _ __   __ _ 
| ___ \ '__/ _` | '_ \ / __| '_ \  \ \ /\ / / '__| | __| | '_ \ / _` |
| |_/ / | | (_| | | | | (__| | | |  \ V  V /| |  | | |_| | | | | (_| |
\____/|_|  \__,_|_| |_|\___|_| |_|   \_/\_/ |_|  |_|\__|_|_| |_|\__, |
                                                                 __/ |
                                                                |___/ 
*/

// Requires having called jetTool->getJetCorrections(alljets, rhoEvent_) beforehand
void bmaker_basic::writeMET(edm::Handle<pat::METCollection> mets, edm::Handle<pat::METCollection> mets_nohf){
  jetTool->getMETWithJEC(mets, baby.met(), baby.met_phi());
  jetTool->getMETRaw(mets, baby.met_raw(), baby.met_raw_phi());
  baby.met_mini() = mets->at(0).pt();
  baby.met_mini_phi() = mets->at(0).phi();
  if(mets_nohf.isValid()){
    baby.met_nohf() = mets_nohf->at(0).pt();
    baby.met_nohf_phi() = mets_nohf->at(0).phi();
  }
  if(!isData){
    baby.met_tru() = mets->at(0).genMET()->pt();
    baby.met_tru_phi() = mets->at(0).genMET()->phi();
  }
}

// Requires having called jetTool->getJetCorrections(alljets, rhoEvent_) beforehand
vector<LVector> bmaker_basic::writeJets(edm::Handle<pat::JetCollection> alljets, vCands &sig_leps, vCands &veto_leps){
  vector<LVector> jets;
  baby.njets() = 0; baby.nbl() = 0; baby.nbm() = 0;  baby.nbt() = 0;  
  baby.ht() = 0.; baby.ht_hlt() = 0.;
  baby.njets_ra2() = 0; baby.nbm_ra2() = 0; baby.ht_ra2() = 0.; 
  baby.pass_jets() = true; baby.pass_jets_nohf() = true; baby.pass_jets_ra2() = true; 
  float mht_px(0.), mht_py(0.);
  for (size_t ijet(0); ijet < alljets->size(); ijet++) {
    const pat::Jet &jet = (*alljets)[ijet];

    LVector jetp4(jetTool->corrJet[ijet]);
    // Saving good jets and jets corresponding to signal leptons
    bool isLep = jetTool->leptonInJet(jet, sig_leps);
    bool isLep_ra2 = jetTool->leptonInJet(jet, veto_leps);
    bool goodID = jetTool->idJet(jet);
    bool goodPtEta = jetp4.pt() > jetTool->JetPtCut && fabs(jet.eta()) <= jetTool->JetEtaCut;
    bool goodJet = (!isLep) && goodID && goodPtEta;
    float csv(jet.bDiscriminator("pfCombinedInclusiveSecondaryVertexV2BJetTags"));
    if(goodPtEta && !isLep && !goodID) baby.pass_jets_nohf() = false;
    if(jetp4.pt() > jetTool->JetPtCut && !isLep && !goodID) baby.pass_jets() = false;
    if(jetp4.pt() > jetTool->JetPtCut && !isLep_ra2 && !goodID) baby.pass_jets_ra2() = false;
    if(goodID && goodPtEta) {
      baby.njets_ra2()++;
      baby.ht_ra2() += jetp4.pt();
      if(csv > jetTool->CSVMedium) baby.nbm_ra2()++;
    }
    if(goodID && jetp4.pt() > jetTool->JetPtCut && fabs(jet.eta()) <= jetTool->JetMHTEtaCut){
      mht_px -= jet.px();
      mht_py -= jet.py();
    }
    if(jetp4.pt() > jetTool->JetHLTPtCut && fabs(jet.eta()) <= jetTool->JetHLTEtaCut) baby.ht_hlt() += jetp4.pt();

    if(!isLep && !goodJet) continue;

    // Filling branches
    baby.jets_pt().push_back(jetp4.pt());
    baby.jets_eta().push_back(jet.eta());
    baby.jets_phi().push_back(jet.phi());
    baby.jets_m().push_back(jetp4.mass());
    baby.jets_islep().push_back(isLep);
    
    baby.jets_csv().push_back(csv);

    jets.push_back(jetp4);

    if(goodJet){
      baby.njets()++;
      baby.ht() += jetp4.pt();
      if(csv > jetTool->CSVLoose)  baby.nbl()++;
      if(csv > jetTool->CSVMedium) baby.nbm()++;
      if(csv > jetTool->CSVTight)  baby.nbt()++;
    }
  } // Loop over jets  

  baby.mht_ra2() = hypot(mht_px, mht_py);
  return jets; // Returning jets that pass acceptance and ID, regardless of whether they're leptons
} 

void bmaker_basic::writeFatJets(vector<LVector> &jets){
  clusterFatJets(baby.nfjets(), baby.mj(),
		 baby.fjets_pt(), baby.fjets_eta(),
		 baby.fjets_phi(), baby.fjets_m(),
		 baby.fjets_nconst(),
		 baby.fjets_sumcsv(), baby.fjets_poscsv(),
		 baby.fjets_btags(), baby.jets_fjet_index(),
		 1.2, jets);
  clusterFatJets(baby.nfjets08(), baby.mj08(),
                 baby.fjets08_pt(), baby.fjets08_eta(),
                 baby.fjets08_phi(), baby.fjets08_m(),
                 baby.fjets08_nconst(),
                 baby.fjets08_sumcsv(), baby.fjets08_poscsv(),
                 baby.fjets08_btags(), baby.jets_fjet08_index(),
                 0.8, jets);

}
void bmaker_basic::clusterFatJets(int &nfjets, float &mj,
				  vector<float> &fjets_pt, 
				  vector<float> &fjets_eta,
				  vector<float> &fjets_phi, 
				  vector<float> &fjets_m,
				  vector<int> &fjets_nconst,
				  vector<float> &fjets_sumcsv,
				  vector<float> &fjets_poscsv,
				  vector<int> &fjets_btags,
				  vector<int> &jets_fjet_index,
				  double radius,
				  vector<LVector> &jets){

  vector<fastjet::PseudoJet> sjets(0);
  vector<float> csvs(0);
  for(size_t ijet(0); ijet < jets.size(); ijet++){
    const fastjet::PseudoJet this_pj(jets[ijet].px(), jets[ijet].py(), jets[ijet].pz(), jets[ijet].energy());
    sjets.push_back(this_pj);
    csvs.push_back(baby.jets_csv()[ijet]); // This require to have the same jets in baby and jets
  } // Loop over skinny jets
  fastjet::JetDefinition jet_def(fastjet::antikt_algorithm, radius);
  fastjet::ClusterSequence cs(sjets, jet_def);
  vector<fastjet::PseudoJet> fjets = cs.inclusive_jets();
  sort(fjets.begin(), fjets.end(), greaterM);
  nfjets = 0;
  mj = 0.;
  fjets_pt.resize(fjets.size());
  fjets_eta.resize(fjets.size());
  fjets_phi.resize(fjets.size());
  fjets_m.resize(fjets.size());
  fjets_nconst.resize(fjets.size());
  fjets_sumcsv.resize(fjets.size());
  fjets_poscsv.resize(fjets.size());
  fjets_btags.resize(fjets.size());
  jets_fjet_index.resize(jets.size());

  for(size_t ipj = 0; ipj < fjets.size(); ++ipj){
    const fastjet::PseudoJet &pj = fjets.at(ipj);
    fjets_pt.at(ipj) = pj.pt();
    fjets_eta.at(ipj) = pj.eta();
    fjets_phi.at(ipj) = pj.phi_std();
    fjets_m.at(ipj) = pj.m();
    const vector<fastjet::PseudoJet> &cjets = pj.constituents();
    fjets_nconst.at(ipj) = cjets.size();
    mj += pj.m();
    ++nfjets;
    fjets_btags.at(ipj) = 0;
    fjets_sumcsv.at(ipj) = 0.;
    fjets_poscsv.at(ipj) = 0.;
    for(size_t ijet = 0; ijet < jets.size(); ++ijet){
      for(size_t cjet = 0; cjet < cjets.size(); ++ cjet){
        if((cjets.at(cjet) - sjets.at(ijet)).pt() < 0.0001){
          jets_fjet_index.at(ijet) = ipj;
          fjets_sumcsv.at(ipj) += csvs.at(ijet);
          if(csvs.at(ijet) > 0.){
            fjets_poscsv.at(ipj) += csvs.at(ijet);
          }
          if(csvs.at(ijet) > jetTool->CSVMedium){
            ++(fjets_btags.at(ipj));
          }
        }
      } // Loop over fat jet constituents
    } // Loop over skinny jets
  } // Loop over fat jets

}


vCands bmaker_basic::writeMuons(edm::Handle<pat::MuonCollection> muons, 
				edm::Handle<pat::PackedCandidateCollection> pfcands, 
				edm::Handle<reco::VertexCollection> vtx,
				vCands &veto_mus, double rhoEventCentral){
  vCands sig_mus; 
  veto_mus.clear();
  baby.nmus() = 0; baby.nvmus() = 0;
  for (size_t ilep(0); ilep < muons->size(); ilep++) {
    const pat::Muon &lep = (*muons)[ilep];    
    if(!lepTool->isVetoMuon(lep, vtx, -99.)) continue; // Storing leptons that pass all veto cuts except for iso

    double lep_iso(lepTool->getPFIsolation(pfcands, dynamic_cast<const reco::Candidate *>(&lep), 0.05, 0.2, 10., rhoEventCentral, false));
    double lep_reliso(lepTool->getRelIsolation(lep, rhoEventCentral));
    double dz(0.), d0(0.);
    lepTool->vertexMuon(lep, vtx, dz, d0); // Calculating dz and d0

    baby.mus_pt().push_back(lep.pt());
    baby.mus_eta().push_back(lep.eta());
    baby.mus_phi().push_back(lep.phi());
    baby.mus_dz().push_back(dz);
    baby.mus_d0().push_back(d0);
    baby.mus_charge().push_back(lep.charge());
    baby.mus_sigid().push_back(lepTool->idMuon(lep, vtx, lepTool->kMedium));
    baby.mus_tight().push_back(lepTool->idMuon(lep, vtx, lepTool->kTight));
    baby.mus_miniso().push_back(lep_iso);
    baby.mus_reliso().push_back(lep_reliso);

    if(lepTool->isVetoMuon(lep, vtx, lep_iso)) {
      baby.nvmus()++;
      veto_mus.push_back(dynamic_cast<const reco::Candidate *>(&lep));
    }
    if(lepTool->isSignalMuon(lep, vtx, lep_iso)) {
      baby.nmus()++;
      sig_mus.push_back(dynamic_cast<const reco::Candidate *>(&lep));
    }
  } // Loop over muons
  
  return sig_mus;
}


vCands bmaker_basic::writeElectrons(edm::Handle<pat::ElectronCollection> electrons, 
				    edm::Handle<pat::PackedCandidateCollection> pfcands, 
				    edm::Handle<reco::VertexCollection> vtx,
				    vCands &veto_els, double rhoEventCentral){
  vCands sig_els; 
  veto_els.clear();
  baby.nels() = 0; baby.nvels() = 0;
  for (size_t ilep(0); ilep < electrons->size(); ilep++) {
    const pat::Electron &lep = (*electrons)[ilep];    
    if(!lepTool->isVetoElectron(lep, vtx, -99.)) continue; // Storing leptons that pass all veto cuts except for iso

    double lep_iso(lepTool->getPFIsolation(pfcands, dynamic_cast<const reco::Candidate *>(&lep), 0.05, 0.2, 10., rhoEventCentral, false));
    double lep_reliso(lepTool->getRelIsolation(lep, rhoEventCentral));
    double dz(0.), d0(0.);
    lepTool->vertexElectron(lep, vtx, dz, d0); // Calculating dz and d0

    baby.els_pt().push_back(lep.pt());
    baby.els_sceta().push_back(lep.superCluster()->position().eta());
    baby.els_eta().push_back(lep.eta());
    baby.els_phi().push_back(lep.phi());
    baby.els_dz().push_back(dz);
    baby.els_d0().push_back(d0);
    baby.els_charge().push_back(lep.charge());
    baby.els_sigid().push_back(lepTool->idElectron(lep, vtx, lepTool->kMedium));
    baby.els_ispf().push_back(lep.numberOfSourceCandidatePtrs()==2 && abs(lep.sourceCandidatePtr(1)->pdgId())==11);
    baby.els_tight().push_back(lepTool->idElectron(lep, vtx, lepTool->kTight));
    baby.els_miniso().push_back(lep_iso);
    baby.els_reliso().push_back(lep_reliso);

    if(lepTool->isVetoElectron(lep, vtx, lep_iso)){
      baby.nvels()++;
      veto_els.push_back(dynamic_cast<const reco::Candidate *>(&lep));
    }
    if(lepTool->isSignalElectron(lep, vtx, lep_iso)) {
      baby.nels()++;
      sig_els.push_back(dynamic_cast<const reco::Candidate *>(&lep));
    }
  } // Loop over electrons

  return sig_els;
}

void bmaker_basic::writeLeptons(vCands &leptons){ 
  baby.nleps() = baby.nmus() + baby.nels();
  baby.nvleps() = baby.nvmus() + baby.nvels();
  sort(leptons.begin(), leptons.end(), greaterPt);
  for(size_t ind(0); ind < leptons.size(); ind++) {
    baby.leps_pt().push_back(leptons[ind]->pt());
    baby.leps_eta().push_back(leptons[ind]->eta());
    baby.leps_phi().push_back(leptons[ind]->phi());
    baby.leps_id().push_back(leptons[ind]->pdgId());
  } // Loop over leptons
}

void bmaker_basic::writeDiLep(vCands &sig_mus, vCands &sig_els, vCands &veto_mus, vCands &veto_els){
  setDiLepMass(sig_mus,  &baby_base::mumu_m,  &baby_base::mumu_pt1,  &baby_base::mumu_pt2,  &baby_base::mumu_zpt);
  setDiLepMass(veto_mus, &baby_base::mumuv_m, &baby_base::mumuv_pt1, &baby_base::mumuv_pt2, &baby_base::mumuv_zpt);
  setDiLepMass(sig_els,  &baby_base::elel_m,  &baby_base::elel_pt1,  &baby_base::elel_pt2,  &baby_base::elel_zpt);
  setDiLepMass(veto_els, &baby_base::elelv_m, &baby_base::elelv_pt1, &baby_base::elelv_pt2, &baby_base::elelv_zpt);
}

void bmaker_basic::setDiLepMass(vCands leptons, baby_float ll_m, baby_float ll_pt1, baby_float ll_pt2, baby_float ll_zpt){
  for(size_t lep1(0); lep1 < leptons.size(); lep1++){
    for(size_t lep2(lep1+1); lep2 < leptons.size(); lep2++){
      if(leptons[lep1]->charge()*leptons[lep2]->charge()<0){
	LVector z_p4(leptons[lep1]->p4()); 
	z_p4 += leptons[lep2]->p4();
	(baby.*ll_m)()   = z_p4.mass();
	(baby.*ll_zpt)() = z_p4.pt();
	(baby.*ll_pt1)() = max(leptons[lep1]->pt(), leptons[lep2]->pt()); 
	(baby.*ll_pt2)() = min(leptons[lep1]->pt(), leptons[lep2]->pt());

	return; // We only set it with the first good ll combination
      }
    } // Loop over lep2
  } // Loop over lep1
}


bool bmaker_basic::writeTriggers(const edm::TriggerNames &names, 
                                 edm::Handle<edm::TriggerResults> triggerBits, 
                                 edm::Handle<pat::PackedTriggerPrescales> triggerPrescales){
  bool keep(false);
  baby.trig().resize(trig_name.size(), false);
  baby.trig_prescale().resize(trig_name.size(), -1.);
  for (size_t itrig(0); itrig < triggerBits->size(); itrig++) {
    for(size_t itn(0); itn < trig_name.size(); itn++){
      if(names.triggerName(itrig).find(trig_name[itn])!=string::npos){
        baby.trig()[itn] = triggerBits->accept(itrig);
        baby.trig_prescale()[itn] = triggerPrescales->getPrescaleForIndex(itrig);
	if(baby.trig()[itn]) keep = true;
      }
    }
  }

  return keep;
}

void bmaker_basic::writeHLTObjects(const edm::TriggerNames &names, 
                                   edm::Handle<pat::TriggerObjectStandAloneCollection> triggerObjects){
  for (pat::TriggerObjectStandAlone obj : *triggerObjects) {
    obj.unpackPathNames(names);
    TString name(obj.collection());
    float objpt(obj.pt());
    if(name=="hltPFMETProducer::HLT") baby.onmet() = objpt;
    else if(name=="hltPFHT::HLT") baby.onht() = objpt; // There's 2 of these, and we want the last one
    else if(name=="hltL3MuonCandidates::HLT" && baby.onmaxmu()<objpt) baby.onmaxmu() = objpt;
    else if(name=="hltEgammaCandidates::HLT" && baby.onmaxel()<objpt) baby.onmaxel() = objpt;
  }
}

void bmaker_basic::writeFilters(const edm::TriggerNames &fnames,
                                  edm::Handle<edm::TriggerResults> filterBits,
                                  edm::Handle<reco::VertexCollection> vtx){
  for (size_t i(0); i < filterBits->size(); ++i) {
    string name = fnames.triggerName(i);
    bool pass = static_cast<bool>(filterBits->accept(i));
    if (name=="Flag_goodVertices") baby.pass_goodv() = pass;
    else if (name=="Flag_CSCTightHaloFilter") baby.pass_cschalo() = pass;
    else if (name=="Flag_eeBadScFilter") baby.pass_eebadsc() = pass;
    //else if (name=="Flag_HBHENoiseFilter") baby.pass_hbhe() = pass;
  }

  baby.pass_goodv() &= hasGoodPV(vtx);

  baby.pass() = baby.pass_goodv() && baby.pass_eebadsc() && baby.pass_jets();
  baby.pass_nohf() = baby.pass_goodv() && baby.pass_eebadsc() && baby.pass_jets_nohf();
}

void bmaker_basic::writeVertices(edm::Handle<reco::VertexCollection> vtx,
				 edm::Handle<std::vector< PileupSummaryInfo > >  pu_info){  
  baby.npv() = vtx->size();
  if(pu_info.isValid()){
    for(size_t bc(0); bc<pu_info->size(); ++bc){
      if(pu_info->at(bc).getBunchCrossing()==0){
	baby.ntrupv() = pu_info->at(bc).getPU_NumInteractions();
	baby.ntrupv_mean() = pu_info->at(bc).getTrueNumInteractions();
	break;
      }
    } // Loop over true PVs
  } // if pu_info is valid
}

void bmaker_basic::writeGenInfo(edm::Handle<LHEEventProduct> lhe_info){
  baby.nisr_me()=0; baby.ht_isr_me()=0.; 
  for ( unsigned int icount = 0 ; icount < (unsigned int)lhe_info->hepeup().NUP; icount++ ) {
    unsigned int pdgid = abs(lhe_info->hepeup().IDUP[icount]);
    int status = lhe_info->hepeup().ISTUP[icount];
    int mom1id = abs(lhe_info->hepeup().IDUP[lhe_info->hepeup().MOTHUP[icount].first-1]);
    int mom2id = abs(lhe_info->hepeup().IDUP[lhe_info->hepeup().MOTHUP[icount].second-1]);
    float px = (lhe_info->hepeup().PUP[icount])[0];
    float py = (lhe_info->hepeup().PUP[icount])[1];
    float pt = sqrt(px*px+py*py);

    if(status==1 && (pdgid<6 || pdgid==21) && mom1id!=6 && mom1id!=24 && mom2id!=6 && mom2id!=24) {
       baby.nisr_me()++;
       baby.ht_isr_me() += pt;
    }

  } // Loop over generator particles
} // writeGenInfo

void bmaker_basic::writeMC(edm::Handle<reco::GenParticleCollection> genParticles){
  LVector isr_p4;
  baby.ntruleps()=0; baby.ntrumus()=0; baby.ntruels()=0; baby.ntrutaush()=0; baby.ntrutausl()=0;  
  for (size_t imc(0); imc < genParticles->size(); imc++) {
    const reco::GenParticle &mc = (*genParticles)[imc];

    size_t id(abs(mc.pdgId())), momid(0);
    if(mc.mother()) momid = abs(mc.mother()->pdgId());
    bool lastTop(mcTool->isLast(mc,6));
    bool lastGluino(mcTool->isLast(mc,1000021));
    bool lastZ(mcTool->isLast(mc,23));
    bool bFromTop(id==5 && momid==6);
    bool eFromTopZ(id==11 && (momid==24 || momid==23));
    bool muFromTopZ(id==13 && (momid==24 || momid==23));
    bool tauFromTop(id==15 && momid==24);
    bool fromW(momid==24);

    // Finding p4 of ME ISR system
    if((lastTop && outname.Contains("TTJets")) || (lastGluino && outname.Contains("SMS")) || 
       (lastZ && outname.Contains("DY"))) isr_p4 -= mc.p4();

    // Saving interesting true particles
    if(lastTop || lastGluino || lastZ || bFromTop || eFromTopZ || muFromTopZ || tauFromTop || fromW) {
      baby.mc_id().push_back(mc.pdgId());
      baby.mc_pt().push_back(mc.pt());
      baby.mc_eta().push_back(mc.eta());
      baby.mc_phi().push_back(mc.phi());
      baby.mc_mom().push_back(mc.mother()->pdgId());
    }
    // Counting true leptons
    if(muFromTopZ) baby.ntrumus()++;
    if(eFromTopZ) baby.ntruels()++;
    if(tauFromTop){
      const reco::GenParticle *tauDaughter(0);
      if(mcTool->decaysTo(mc, 11, tauDaughter) || mcTool->decaysTo(mc, 13, tauDaughter)){
	baby.mc_id().push_back(tauDaughter->pdgId());
	baby.mc_pt().push_back(tauDaughter->pt());
	baby.mc_eta().push_back(tauDaughter->eta());
	baby.mc_phi().push_back(tauDaughter->phi());
	baby.mc_mom().push_back(tauDaughter->mother()->pdgId());
	baby.ntrutausl()++;
      } else baby.ntrutaush()++;
    }

  } // Loop over genParticles
  baby.isr_pt() = isr_p4.pt();
  baby.isr_eta() = isr_p4.eta();
  baby.isr_phi() = isr_p4.phi();

  baby.ntruleps() = baby.ntrumus()+baby.ntruels()+baby.ntrutaush()+baby.ntrutausl();
} // writeMC

/*
 _____                 _                   _                 
/  __ \               | |                 | |                
| /  \/ ___  _ __  ___| |_ _ __ _   _  ___| |_ ___  _ __ ___ 
| |    / _ \| '_ \/ __| __| '__| | | |/ __| __/ _ \| '__/ __|
| \__/\ (_) | | | \__ \ |_| |  | |_| | (__| || (_) | |  \__ \
 \____/\___/|_| |_|___/\__|_|   \__,_|\___|\__\___/|_|  |___/
*/

bmaker_basic::bmaker_basic(const edm::ParameterSet& iConfig):
  outname(TString(iConfig.getParameter<string>("outputFile"))),
  inputfiles(iConfig.getParameter<vector<string> >("inputFiles")),
  jec_label(iConfig.getParameter<string>("jec")),
  met_label(iConfig.getParameter<edm::InputTag>("met")),
  met_nohf_label(iConfig.getParameter<edm::InputTag>("met_nohf")),
  jets_label(iConfig.getParameter<edm::InputTag>("jets")),
  nevents_sample(iConfig.getParameter<unsigned int>("nEventsSample")),
  nevents(0){
  
  time(&startTime);

  lepTool = new lepton_tools();
  jetTool = new jet_met_tools(jec_label);
  mcTool  = new mc_tools();

  outfile = new TFile(outname, "recreate");
  outfile->cd();
  baby.tree_.SetDirectory(outfile);

  xsec = crossSection(outname);

  trig_name = vector<TString>();
  if(outname.Contains("Run201")){
    trig_name.push_back("HLT_PFHT350_PFMET100_JetIdCleaned_v");			// 0 
    trig_name.push_back("HLT_Mu15_IsoVVVL_PFHT350_PFMET50_v");			// 1 
    trig_name.push_back("HLT_Mu15_IsoVVVL_PFHT600_v");				// 2
    trig_name.push_back("HLT_Mu15_IsoVVVL_BTagCSV0p72_PFHT400_v");		// 3
    trig_name.push_back("HLT_Mu15_IsoVVVL_PFHT350_v");				// 4 
    trig_name.push_back("HLT_Ele15_IsoVVVL_PFHT350_PFMET50_v");			// 5 
    trig_name.push_back("HLT_Ele15_IsoVVVL_PFHT600_v");				// 6
    trig_name.push_back("HLT_Ele15_IsoVVVL_BTagCSV0p72_PFHT400_v");		// 7
    trig_name.push_back("HLT_Ele15_IsoVVVL_PFHT350_v");				// 8 
    trig_name.push_back("HLT_DoubleMu8_Mass8_PFHT300_v");			// 9
    trig_name.push_back("HLT_DoubleEle8_CaloIdM_TrackIdM_Mass8_PFHT300_v");	// 10
    trig_name.push_back("HLT_PFHT475_v");					// 11
    trig_name.push_back("HLT_PFHT800_v");					// 12
    trig_name.push_back("HLT_PFMET120_JetIdCleaned_Mu5_v");			// 13
    trig_name.push_back("HLT_PFMET170_JetIdCleaned_v");				// 14
    trig_name.push_back("HLT_DoubleIsoMu17_eta2p1_v");		        	// 15
    trig_name.push_back("HLT_Mu17_TrkIsoVVL_Mu8_TrkIsoVVL_DZ_v"); 	        // 16
    trig_name.push_back("HLT_IsoMu18_v");					// 17
    trig_name.push_back("HLT_IsoMu20_v");					// 18
    trig_name.push_back("HLT_IsoMu24_eta2p1_v");				// 19
    trig_name.push_back("HLT_IsoMu27_v");					// 20
    trig_name.push_back("HLT_Mu50_v");						// 21
    trig_name.push_back("HLT_Ele27_eta2p1_WPLoose_Gsf_v");			// 22
    trig_name.push_back("HLT_Ele23_WPLoose_Gsf_v");         			// 23
    trig_name.push_back("HLT_Ele105_CaloIdVT_GsfTrkIdT_v");			// 24
    trig_name.push_back("HLT_DoubleEle33_CaloIdL_GsfTrkIdVL_v");		// 25
    trig_name.push_back("HLT_DoubleEle24_22_eta2p1_WPLoose_Gsf_v");		// 26
  } else {
    trig_name.push_back("HLT_PFHT350_PFMET120_NoiseCleaned_v");			// 0 
    trig_name.push_back("HLT_Mu15_IsoVVVL_PFHT400_PFMET70_v");			// 1 
    trig_name.push_back("HLT_Mu15_IsoVVVL_PFHT600_v");				// 2
    trig_name.push_back("HLT_Mu15_IsoVVVL_BTagCSV07_PFHT400_v");		// 3
    trig_name.push_back("HLT_Mu15_PFHT300_v");				        // 4 
    trig_name.push_back("HLT_Ele15_IsoVVVL_PFHT400_PFMET70_v");			// 5 
    trig_name.push_back("HLT_Ele15_IsoVVVL_PFHT600_v");				// 6
    trig_name.push_back("HLT_Ele15_IsoVVVL_BTagtop8CSV07_PFHT400_v");		// 7
    trig_name.push_back("HLT_Ele15_PFHT300_v");				        // 8 
    trig_name.push_back("HLT_DoubleMu8_Mass8_PFHT300_v");			// 9
    trig_name.push_back("HLT_DoubleEle8_CaloIdM_TrackIdM_Mass8_PFHT300_v");	// 10
    trig_name.push_back("HLT_PFHT350_v");					// 11
    trig_name.push_back("HLT_PFHT900_v");					// 12
    trig_name.push_back("HLT_PFMET120_NoiseCleaned_Mu5_v");			// 13
    trig_name.push_back("HLT_PFMET170_NoiseCleaned_v");				// 14
    trig_name.push_back("HLT_DoubleIsoMu17_eta2p1_v");		        	// 15
    trig_name.push_back("HLT_Mu17_TrkIsoVVL_Mu8_TrkIsoVVL_DZ_v"); 	        // 16
    trig_name.push_back("HLT_IsoMu17_eta2p1_v");        			// 17
    trig_name.push_back("HLT_IsoMu20_v");					// 18
    trig_name.push_back("HLT_IsoMu24_eta2p1_v");				// 19
    trig_name.push_back("HLT_IsoMu27_v");					// 20
    trig_name.push_back("HLT_Mu50_v");						// 21
    trig_name.push_back("HLT_Ele27_eta2p1_WP75_Gsf_v");	        		// 22
    trig_name.push_back("HLT_Ele22_eta2p1_WP75_Gsf_v");        			// 23
    trig_name.push_back("HLT_Ele105_CaloIdVT_GsfTrkIdT_v");			// 24
    trig_name.push_back("HLT_DoubleEle33_CaloIdL_GsfTrkIdVL_v");		// 25
    trig_name.push_back("HLT_DoubleEle24_22_eta2p1_WP75_Gsf_v");		// 26
  }

}


bmaker_basic::~bmaker_basic(){
  outfile->cd();
  baby.tree_.SetDirectory(outfile);
  baby.Write();

  string commit_s = execute("git rev-parse HEAD");
  while(!commit_s.empty() && commit_s.at(commit_s.length()-1) == '\n') commit_s.erase(commit_s.length()-1);
  TString commit = commit_s;
  TString type = baby.Type();
  TString root_version = gROOT->GetVersion();
  TString root_tutorial_dir = gROOT->GetTutorialsDir();
  TString user(getenv("USER"));
  TString cmssw(getenv("CMSSW_BASE"));
  time_t curTime;
  time(&curTime);
  char time_c[100];
  struct tm * timeinfo = localtime(&curTime);
  strftime(time_c,100,"%Y-%m-%d %H:%M:%S",timeinfo);
  TString date(time_c);

  int seconds(floor(difftime(curTime,startTime)+0.5));
  TTree treeglobal("treeglobal", "treeglobal");
  treeglobal.Branch("nev_sample", &nevents_sample);
  treeglobal.Branch("nev_file", &nevents);
  treeglobal.Branch("runtime_seconds", &seconds);
  treeglobal.Branch("git_commit", &commit);
  // treeglobal.Branch("model", &model);
  treeglobal.Branch("baby_type", &type);
  treeglobal.Branch("root_version", &root_version);
  treeglobal.Branch("root_tutorial_dir", &root_tutorial_dir);
  treeglobal.Branch("trig_names", &trig_name);
  treeglobal.Branch("xsec", &xsec);
  treeglobal.Branch("user", &user);
  treeglobal.Branch("cmssw", &cmssw);
  treeglobal.Branch("jec", &jec_label);
  treeglobal.Branch("date", &date);
  treeglobal.Branch("inputfiles", &inputfiles);
  treeglobal.Fill();
  treeglobal.SetDirectory(outfile);
  treeglobal.Write();
  
  outfile->Close();

  int minutes((seconds/60)%60), hours(seconds/3600);
  TString runtime("");
  if(hours<10) runtime += "0";
  runtime += hours; runtime += ":";
  if(minutes<10) runtime += "0";
  runtime += minutes; runtime += ":";
  if((seconds%60)<10) runtime += "0";
  runtime += seconds%60; 
  float hertz(nevents); hertz /= seconds;
  cout<<endl<<"BABYMAKER: Written "<<nevents<<" events in "<<outname<<". It took "<<seconds<<" seconds to run ("<<runtime<<"), "
      <<roundNumber(hertz,1)<<" Hz, "<<roundNumber(1000,2,hertz)<<" ms per event"<<endl<<endl;
  cout<<"BABYMAKER: *********** List of input files ***********"<<endl;
  for(size_t ifile(0); ifile < inputfiles.size(); ifile++)
    cout<<"BABYMAKER: "<<inputfiles[ifile].c_str()<<endl;
  cout<<endl;

  delete outfile;

  delete lepTool;
  delete jetTool;
  delete mcTool;
}

void bmaker_basic::reportTime(const edm::Event& iEvent){
  // Time reporting
  if(nevents==1) {
    time_t curTime;
    time(&curTime);
    cout<<endl<<"BABYMAKER: Took "<<roundNumber(difftime(curTime,startTime),1)<<" seconds for set up and run first event"
	<<endl<<endl;
    time(&startTime);
  }
  if((nevents<100&&nevents%10==0) || (nevents<1000&&nevents%100==0) 
     || (nevents<10000&&nevents%1000==0) || nevents%10000==0) {
    time_t curTime;
    time(&curTime);
    float seconds(difftime(curTime,startTime));
    cout<<"BABYMAKER: Run "<<iEvent.id().run()<<", Event "<< setw(7)<<iEvent.id().event()
	<<", LumiSection "<< setw(4)<< iEvent.luminosityBlock()
	<<". Ran "<<nevents<<" events in "<<seconds<<" seconds -> "<<roundNumber(nevents-1,1,seconds)<<" Hz, "
	<<roundNumber(seconds*1000,2,nevents-1)<<" ms per event"<<endl;
  }
}

// ------------ method called once each job just before starting event loop  ------------
void bmaker_basic::beginJob() {
}

// ------------ method called once each job just after ending the event loop  ------------
void bmaker_basic::endJob() {
}

// ------------ method called when starting to processes a run  ------------
/*
void 
bmaker_basic::beginRun(edm::Run const&, edm::EventSetup const&)
{
}
*/

// ------------ method called when ending the processing of a run  ------------
/*
void 
bmaker_basic::endRun(edm::Run const&, edm::EventSetup const&)
{
}
*/

// ------------ method called when starting to processes a luminosity block  ------------
/*
void 
bmaker_basic::beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&)
{
}
*/

// ------------ method called when ending the processing of a luminosity block  ------------
/*
void 
bmaker_basic::endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&)
{
}
*/

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
bmaker_basic::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(bmaker_basic);

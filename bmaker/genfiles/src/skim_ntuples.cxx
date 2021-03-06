// skim_ntuples.cxx: Skims reduced trees
// USAGE: ./plot/skim_ntuples.exe infolder outfolder [cuts=\"ht>500&&met>200\"] [njobs=-1] [ijob=-1]


#include "utilities.hh"

#include <ctime>
#include <fstream>
#include <iostream>
#include <cmath>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>     /* atoi */

#include "TChain.h"
#include "TFile.h"
#include "TString.h"
#include "TMath.h"
#include "TSystem.h"
#include "TDirectory.h"

using namespace std;
using std::cout;
using std::endl;

void onefile_skim(TString infiles, TString outfolder, TString cuts, TString tag);

int main(int argc, char *argv[]){
  time_t startTime;
  time(&startTime);

  if(argc < 3) {
    cout<<endl<<"Required at least 2 arguments: "
	<<"./plot/skim_ntuples.exe infolder outfolder [cuts=\"nleps==1&&ht>500&&met>200&&njets>=6&&nbm>=1&&mj14>250&&nveto==0\"] "
	<<"[njobs=0] [ijob=0] [filetag=\"\"]"<<endl<<endl;;
    return 1;
  }
  TString folder(argv[1]), outfolder(argv[2]), cuts="nleps==1&&ht>500&&met>200&&njets>=6&&nbm>=1&&mj14>250&&nveto==0";
  if(argc >= 4) cuts = argv[3]; 
  unsigned njobs(0), ijob(0);
  if(argc >= 6) {
    njobs = atoi(argv[4]);
    ijob  = atoi(argv[5]);
  }
  TString filetag="";
  if(argc >= 7) filetag = argv[6];

  TString tag = cuts; // Using tag to avoid file names too long for TFile
  if(cuts=="standard") cuts="nleps>=1&&ht>500&&met>200";
  if(cuts=="met150") cuts="nleps>=1&&ht>500&&met>150&&met<=200&&njets>=5";
  if(cuts=="abcd") cuts="nleps==1&&ht>500&&met>200&&njets>=6&&nbm>=1&&mj14>250&&nveto==0";
  if(cuts=="baseline") cuts="nleps==1&&ht>500&&met>200&&njets>=6&&nbm>=1";
  if(cuts=="sys_abcd") 
    cuts = "nleps==1&&max(ht,Max$(sys_ht))>500&&max(met,Max$(sys_met))>200&&max(njets,Max$(sys_njets))>=6&&max(nbm,Max$(sys_nbm))>=1&&max(mj,Max$(sys_mj))>250";
  if(cuts=="zisr")
    cuts = "nvleps==2&&nleps>=1&&Max$(leps_pt)>30&&((elelv_m>80&&elelv_m<100)||(mumuv_m>80&&mumuv_m<100))";
  if(cuts=="dy_ht300")
    cuts = "nvleps==2&&nleps>=1&&Max$(leps_pt)>30&&((elelv_m>80&&elelv_m<100)||(mumuv_m>80&&mumuv_m<100))&&ht>300";
  if(cuts=="ttisr")
    cuts = "nvleps==2&&nleps>=1&&max(Max$(mus_pt*(mus_tight&&mus_reliso<.1)),Max$(els_pt*(els_tight&&els_reliso<.1)))>30&&nbm==2";
  if(cuts=="wisr")
    cuts = "met>100&&max(Max$(mus_pt*(mus_tight&&mus_reliso<.1)),Max$(els_pt*(els_tight&&els_reliso<.1)))>30&&nbl==0";
  if(cuts=="wisrht200")
    cuts = "ht>200&&met>100&&max(Max$(mus_pt*(mus_tight&&mus_reliso<.1)),Max$(els_pt*(els_tight&&els_reliso<.1)))>30&&nbl==0";
  if(cuts=="ttdilep_ht300")
    cuts = "nels==1&&nmus==1&&Max$(leps_pt)>30&&ht>300&&met>100&&nbm>=1";
  if(cuts=="qcd")
    cuts = "ht>1000&&met<50&&(nvmus+nvels)==0";
  if(cuts=="qcd_njet10")
     cuts = "ht>1000&&met<50&&(nvmus+nvels)==0&&njets>=10";
  if(cuts=="mm_std") cuts="Sum$(mm_nleps>=1&&mm_ht>500.&&mm_met>200.)>0";	     
  if(cuts=="mm_std_nj5mj250") cuts="Sum$(mm_nleps>=1&&mm_ht>500&&mm_met>200&&mm_njets>=5&&mm_mj14_lep>250)>0||Sum$(mm_nleps>=1&&mm_ht>500&&mm_met>200&&mm_njets>=5&&mm_mj14_nolep>250)>0";

  //// RA2/b skims
  TString pass="globalTightHalo2016Filter==1&&HBHENoiseFilter==1&&HBHEIsoNoiseFilter==1&&eeBadScFilter==1";
  pass += "&&EcalDeadCellTriggerPrimitiveFilter==1&&BadChargedCandidateFilter&&BadPFMuonFilter&&NVtx>0&&JetID";
  if(cuts=="ra2_qcd") cuts = pass+"&&(@Electrons.size()+@Muons.size())==0&&NJets>=3";	     
  if(cuts=="ra2_ht300") cuts = pass+"&&HT>300";	     
  if(cuts=="ra2_eht300") cuts = pass+"&&Max$(Electrons.Pt()*(abs(Electrons.Eta())<2))>35&&HT>300";	     
  if(cuts=="ra2_zmht200") cuts = pass+"&&@ZCandidates.size()>=1&&MHT>200";	     
	     

  vector<TString> files = dirlist(folder, "*"+filetag+"*.root");
  unsigned nfiles(files.size()), ini(0), end(nfiles);
  if(njobs>0){
    if(ijob<1 || ijob>njobs){
      cout<<endl<<"You need to set the 5th argument between 1 and "<<njobs<<endl<<endl;
      return 1;
    }
    unsigned jobfiles = (nfiles+njobs-1)/njobs;
    unsigned nbigjobs = (nfiles+njobs-1)%njobs+1;
    if(ijob <= nbigjobs){
      ini = jobfiles*(ijob-1);
      end = ini + jobfiles;
    } else {
      ini = nbigjobs*jobfiles+(jobfiles-1)*(ijob-1-nbigjobs);
      end = ini + jobfiles-1;
    }
  }
  cout<<"Doing files "<<ini+1<<" to "<<end<<" out of "<<nfiles<<endl;
  for(unsigned file(ini); file < end; file++){
    cout<<file+1<<"/"<<nfiles<<": ";
    onefile_skim(folder+"/"+files[file], outfolder, cuts, tag);
  }

  time_t curTime;
  time(&curTime);
  int seconds = difftime(curTime,startTime);
  cout<<endl<<"Took "<< seconds << " seconds ("<<hoursMinSec(seconds)<<") to skim "<< end-ini<<" files."<<endl<<endl;
}

void onefile_skim(TString infiles, TString outfolder, TString cuts, TString tag){
  TString folder(infiles), outfile(infiles);
  folder.Remove(folder.Last('/')+1, folder.Length());

  // Finding outfile name
  outfile.Remove(0, outfile.Last('/')); outfile.ReplaceAll("*","");
  if(outfile.Contains(".root")) outfile.ReplaceAll(".root","_"+tag+".root");
  else outfile += ("_"+tag+".root");
  outfile.ReplaceAll(">=","GE"); outfile.ReplaceAll("<=","SE"); outfile.ReplaceAll("&","_");
  outfile.ReplaceAll(">","G"); outfile.ReplaceAll("<","S"); outfile.ReplaceAll("=","");
  outfile.ReplaceAll("(",""); outfile.ReplaceAll(")",""); outfile.ReplaceAll("+","");
  outfile.ReplaceAll("[",""); outfile.ReplaceAll("]",""); outfile.ReplaceAll("|","_");
  outfile.ReplaceAll("$",""); outfile.ReplaceAll(",","_"); outfile.ReplaceAll("!","NOT");
  outfile.ReplaceAll(" ",""); outfile.ReplaceAll("@",""); 
  outfile = outfolder+outfile;

  // Checking if output file exists
  TString outname(outfile);
  outname.ReplaceAll(outfolder, ""); outname.ReplaceAll("/", "");
  vector<TString> outfiles = dirlist(outfolder, outname);
  if(outfiles.size()>0) {
    cout<<"File "<<outfile<<" exists. Exiting"<<endl;
    return;
  }

  gSystem->mkdir(outfolder, kTRUE);
  TFile out_rootfile(outfile, "CREATE");
  if(out_rootfile.IsZombie() || !out_rootfile.IsOpen()) return;
  out_rootfile.cd();
  TChain tree("tree");
  int nfiles = tree.Add(infiles);
  TChain treeglobal("treeglobal");
  treeglobal.Add(infiles);

  //cout<<"Skimming the "<<nfiles<<" files in "<<infiles<<endl;
  long nentries(tree.GetEntries());
  TTree *ctree = tree.CopyTree(cuts);
  long centries=ctree->GetEntries();
  if(ctree) ctree->Write();
  else {
    cout<<"Could not find tree in "<<infiles<<endl;
    return;
  }

  //// treeglobal
  TFile infile(infiles);
  if(infile.Get("treeglobal") != 0){
    out_rootfile.cd();
    TTree *ctreeglobal = treeglobal.CopyTree("1");
    if(ctreeglobal)   ctreeglobal->Write();
    //else cout<<"Could not find treeglobal in "<<infiles<<endl;
  }
  out_rootfile.Close();
  cout<<"Written "<<centries<<" entries to "<<outfile<<" from "<<nfiles<<" files and "<<nentries<<" entries."<<endl;
}


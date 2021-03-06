// Common utilities
 
#include <cmath>
#include <iostream>
#include "babymaker/bmaker/interface/utilities.hh"
#include "babymaker/bmaker/interface/lester_mt2_bisect.h"
#include "TMath.h"

using namespace std;

namespace utilities{

  float sumMass(const LVector &a, const LVector &b){
    return sqrt(a.mass2()+b.mass2()+2.*(a.E()*b.E()-a.Px()*b.Px()-a.Py()*b.Py()-a.Pz()*b.Pz()));
  }

  float dPhi(float phi1, float phi2){
    float delphi = TMath::Abs(TMath::Abs(TMath::Abs(phi1 - phi2) - TMath::Pi())-TMath::Pi());
    return delphi;
  }

  float dR(float phi1, float phi2, float eta1, float eta2){
    float dr = sqrt(pow(eta1-eta2,2)+pow(dPhi(phi1,phi2),2));
    return dr;
    
  }


  bool greaterPt(const reco::Candidate *a, const reco::Candidate *b) {
    return a->pt() > b->pt();
  }

  bool greaterM(const fastjet::PseudoJet &a, const fastjet::PseudoJet &b){
    return a.m() > b.m();
  }

  float getMT(float pt1, float phi1, float pt2, float phi2){
    //Faster calculation of mT in massless 
    return sqrt(2.*pt1*pt2*(1.-cos(phi2-phi1)));
  }

  float getMT(float m1, float pt1, float phi1,
	      float m2, float pt2, float phi2){
    return sqrt(m1*m1+m2*m2+2.*(sqrt((m1*m1+pt1*pt1)*(m2*m2+pt2*pt2))-pt1*pt2*cos(phi1-phi2)));
  }

  float getMT2(float pt1, float phi1, float pt2, float phi2, float met, float met_phi){
    return getMT2(0., pt1, phi1, 0., pt2, phi2, met, met_phi);
  }

  float getMT2(float m1, float pt1, float phi1,
	       float m2, float pt2, float phi2,
	       float met, float met_phi){
    asymm_mt2_lester_bisect::disableCopyrightMessage();
    double mVisA = m1; // mass of visible object on side A.  Must be >=0.
    double pxA = pt1*cos(phi1); // x momentum of visible object on side A.
    double pyA = pt1*sin(phi1); // y momentum of visible object on side A.
 
    double mVisB = m2; // mass of visible object on side B.  Must be >=0.
    double pxB = pt2*cos(phi2); // x momentum of visible object on side B.
    double pyB = pt2*sin(phi2); // y momentum of visible object on side B.
 
    double pxMiss = met*cos(met_phi); // x component of missing transverse momentum.
    double pyMiss = met*sin(met_phi); // y component of missing transverse momentum.
 
    double chiA = 0; // hypothesised mass of invisible on side A.  Must be >=0.
    double chiB = 0; // hypothesised mass of invisible on side B.  Must be >=0.
    double desiredPrecisionOnMt2 = 0; // Must be >=0.  If 0 alg aims for machine precision.  if >0, MT2 computed to supplied absolute precision.

    double MT2 =  asymm_mt2_lester_bisect::get_mT2(
						   mVisA, pxA, pyA,
						   mVisB, pxB, pyB,
						   pxMiss, pyMiss,
						   chiA, chiB,
						   desiredPrecisionOnMt2);



    return MT2;
  }


  string execute(const string &cmd){
    FILE *pipe = popen(cmd.c_str(), "r");
    if(!pipe) throw runtime_error("Could not open pipe.");
    const size_t buffer_size = 128;
    char buffer[buffer_size];
    string result = "";
    while(!feof(pipe)){
      if(fgets(buffer, buffer_size, pipe) != NULL) result += buffer;
    }

    pclose(pipe);
    return result;
  }

  TString roundNumber(double num, int decimals, double denom){
    if(denom==0) return " - ";
    double neg = 1; if(num*denom<0) neg = -1;
    num /= neg*denom; num += 0.5*pow(10.,-decimals);
    long num_int = static_cast<long>(num);
    long num_dec = static_cast<long>((1+num-num_int)*pow(10.,decimals));
    TString s_dec = ""; s_dec += num_dec; s_dec.Remove(0,1);
    TString result="";
    if(neg<0) result+="-";
    result+= num_int;
    if(decimals>0) {
      result+="."; result+=s_dec;
    }

    TString afterdot = result;
    afterdot.Remove(0,afterdot.First(".")+1);
    for(int i=0; i<decimals-afterdot.Length(); i++)
      result += "0";
    return result;
  }

  TString addCommas(double num){
    TString result(""); result += num;
    int posdot(result.First('.'));
    if(posdot==-1) posdot = result.Length();
    for(int ind(posdot-3); ind > 0; ind -= 3)
      result.Insert(ind, ",");
    return result;
  }

}

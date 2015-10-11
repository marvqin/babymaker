//// PHYS_OBJECTS: Common physics objects definitions
//// Function names follow the first-lowercase, following words-uppercase. No underscores

// System include files
#include <algorithm>

// FW include files
#include "FWCore/Framework/interface/ESHandle.h"
#include "DataFormats/Math/interface/deltaR.h"

// User include files
#include "babymaker/bmaker/interface/phys_objects.hh"
#include "babymaker/bmaker/interface/in_json.hh"

using namespace std;

namespace phys_objects{

  const std::vector<std::vector<int> > VRunLumi2015golden(MakeVRunLumi("2015golden"));
  const std::vector<std::vector<int> > VRunLumi2015nohfgolden(MakeVRunLumi("2015nohfgolden"));
  const std::vector<std::vector<int> > VRunLumi2015dcs(MakeVRunLumi("2015dcs"));

  bool isInJSON(string type, int run, int lumiblock){
    if(type=="golden") return inJSON(VRunLumi2015golden, run, lumiblock);
    if(type=="nohf_golden") return inJSON(VRunLumi2015nohfgolden, run, lumiblock);
    if(type=="dcs") return inJSON(VRunLumi2015dcs, run, lumiblock);

    return true;
  }

  bool hasGoodPV(edm::Handle<reco::VertexCollection> vtx){
    bool one_good_pv(false);
    for(unsigned ipv(0); ipv < vtx->size(); ipv++){
      const double pv_rho(sqrt(pow(vtx->at(ipv).x(),2) + pow(vtx->at(ipv).y(),2)));
      if(vtx->at(ipv).ndof()>4 && fabs(vtx->at(ipv).z())<24. && pv_rho<2.0 && vtx->at(ipv).isFake()==0){
        one_good_pv = true;
        break;
      }
    } // Loop over vertices
    return one_good_pv;
  }
}

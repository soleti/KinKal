// 
// test derivatives of Traj class
//
#include "KinKal/TPoca.hh"

#include <iostream>
#include <stdio.h>
#include <iostream>
#include <getopt.h>
#include <typeinfo>

#include "TROOT.h"
#include "TH1F.h"
#include "TFile.h"
#include "TSystem.h"
#include "THelix.h"
#include "TPolyLine3D.h"
#include "TArrow.h"
#include "TAxis3D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TVector3.h"
#include "TPolyLine3D.h"
#include "TPolyMarker3D.h"
#include "TLegend.h"
#include "TGraph.h"
#include "TRandom3.h"
#include "TH2F.h"
#include "TDirectory.h"
#include "TProfile.h"
#include "TProfile2D.h"

using namespace KinKal;
using namespace std;

void print_usage() {
  printf("Usage: KTrajDerivs  --momentum f --costheta f --azimuth f --particle i --charge i --zorigin f --torigin --dmin f --dmax f --ttest f --By f \n");
}

template <class KTRAJ>
int test(int argc, char **argv) {
  typedef typename KTRAJ::PDATA PDATA;
  typedef typename KTRAJ::DVEC DVEC;
  gROOT->SetBatch(kTRUE);
  // save canvases
  int opt;
  double mom(105.0), cost(0.7), phi(0.5);
  double masses[5]={0.511,105.66,139.57, 493.68, 938.0};
  int imass(0), icharge(-1);
  double pmass, oz(100.0), ot(0.0), ttest(5.0);
  double dmin(-1e-2), dmax(1e-2);
  double By(0.0);

  static struct option long_options[] = {
    {"momentum",     required_argument, 0, 'm' },
    {"costheta",     required_argument, 0, 'c'  },
    {"azimuth",     required_argument, 0, 'a'  },
    {"particle",     required_argument, 0, 'p'  },
    {"charge",     required_argument, 0, 'q'  },
    {"zorigin",     required_argument, 0, 'z'  },
    {"torigin",     required_argument, 0, 'o'  },
    {"dmin",     required_argument, 0, 's'  },
    {"dmax",     required_argument, 0, 'e'  },
    {"ttest",     required_argument, 0, 't'  },
    {"By",     required_argument, 0, 'y'  },
    {NULL, 0,0,0}


  };

  int long_index =0;
  while ((opt = getopt_long_only(argc, argv,"", 
	  long_options, &long_index )) != -1) {
    switch (opt) {
      case 'm' : mom = atof(optarg); 
		 break;
      case 'c' : cost = atof(optarg);
		 break;
      case 'a' : phi = atof(optarg);
		 break;
      case 'p' : imass = atoi(optarg);
		 break;
      case 'q' : icharge = atoi(optarg);
		 break;
      case 'z' : oz = atof(optarg);
		 break;
      case 'o' : ot = atof(optarg);
		 break;
      case 's' : dmin = atof(optarg);
		 break;
      case 'e' : dmax = atof(optarg);
		 break;
      case 't' : ttest = atof(optarg);
		 break;
      case 'y' : By = atof(optarg);
		 break;
      default: print_usage(); 
	       exit(EXIT_FAILURE);
    }
  }
  // construct original helix from parameters
  Vec3 bnom(0.0,By,1.0);
  Vec4 origin(0.0,0.0,oz,ot);
  double sint = sqrt(1.0-cost*cost);
  // reference helix
  pmass = masses[imass];
  Mom4 momv(mom*sint*cos(phi),mom*sint*sin(phi),mom*cost,pmass);
  KTRAJ refhel(origin,momv,icharge,bnom);
  cout << "Reference " << refhel << endl;
  Vec4 refpos4;
  refpos4.SetE(ttest);
  refhel.position(refpos4);
  cout << "origin position " << origin << " test position " << refpos4 << endl;
  Mom4 refmom = refhel.momentum(ttest);
  int ndel(50);
  // graphs to compare parameter change
  std::vector<TGraph*> pgraphs[3];
  // graphs to compare momentum change
  TGraph* momgraph[3];
  // gaps
  TGraph* gapgraph[3][3];
  // canvases
  TCanvas* dhcan[3];
  TCanvas* dmomcan[3];
  std::string tfname = KTRAJ::trajName() + "Derivs.root";
  TFile lhderiv(tfname.c_str(),"RECREATE");
  // loop over derivative directions
  double del = (dmax-dmin)/(ndel-1);
  for(int idir=0;idir<3;++idir){
    LocalBasis::LocDir tdir =static_cast<LocalBasis::LocDir>(idir);
//    cout << "testing direction " << LocalBasis::directionName(tdir) << endl;
    // parameter change
    pgraphs[idir] = std::vector<TGraph*>(KTRAJ::NParams(),0); 
    for(size_t ipar = 0; ipar < KTRAJ::NParams(); ipar++){
      pgraphs[idir][ipar] = new TGraph(ndel);
      string title = KTRAJ::paramName(typename KTRAJ::ParamIndex(ipar));
      title += ";exact;1st derivative";
      pgraphs[idir][ipar]->SetTitle(title.c_str());
    }
    momgraph[idir] = new TGraph(ndel);
    momgraph[idir]->SetTitle("Momentum Direction;exact;1st derivative");
    for(int jdir=0;jdir < 3;jdir++){
      LocalBasis::LocDir tjdir =static_cast<LocalBasis::LocDir>(jdir);
      gapgraph[idir][jdir] = new TGraph(ndel);
      string title = "Gap in " + LocalBasis::directionName(tjdir) + ";Fractional change;Gap value (mm)";
      gapgraph[idir][jdir]->SetTitle(title.c_str());
    }
    // scan range of change
    for(int id=0;id<ndel;++id){
      double delta = dmin + del*id;
//      cout << "Delta = " << delta << endl;
      // compute 1st order change in parameters
      Vec3 dmomdir = refhel.direction(ttest,tdir);
      DVEC pder = refhel.momDeriv(ttest,tdir);
      //  compute exact altered params
      Vec3 newmom = refmom.Vect() + delta*dmomdir*mom;
      Mom4 momv(newmom.X(),newmom.Y(),newmom.Z(),pmass);
      KTRAJ xhel(refpos4,momv,icharge,bnom);
//      cout << "derivative vector" << pder << endl;
      DVEC dvec = refhel.params().parameters() + delta*pder;
      PDATA pdata(dvec,refhel.params().covariance());
      KTRAJ dhel(pdata,refhel);
      // test
      Vec4 xpos, dpos;
      xpos.SetE(ttest);
      dpos.SetE(ttest);
      xhel.position(xpos);
      dhel.position(dpos);
//      cout << " exa pos " << xpos << endl
//      << " del pos " << dpos << endl;
      Mom4 dmom = dhel.momentum(ttest);
//      cout << "Exact change" << xhel << endl;
//      cout << "Derivative  " << dhel << endl;
      Vec4 gap = dpos - refpos4;
      // project along 3 directions
      for(int jdir=0;jdir < 3;jdir++){
	LocalBasis::LocDir tjdir =static_cast<LocalBasis::LocDir>(jdir);
	Vec3 jmomdir = refhel.direction(ttest,tjdir);
	pder = refhel.momDeriv(ttest,tjdir);
	gapgraph[idir][jdir]->SetPoint(id,delta,gap.Vect().Dot(jmomdir));
      }
      // parameter diff
      for(size_t ipar = 0; ipar < KTRAJ::NParams(); ipar++){
	pgraphs[idir][ipar]->SetPoint(id,xhel.paramVal(ipar)-refhel.paramVal(ipar),dhel.paramVal(ipar)-refhel.paramVal(ipar));
      }
      // compare momenta after change
      //
      Vec3 dxmom = momv.Vect() - refmom.Vect();
      Vec3 ddmom = dmom.Vect() - refmom.Vect();
      momgraph[idir]->SetPoint(id,dxmom.Dot(dmomdir),ddmom.Dot(dmomdir));
    }
    char gtitle[80];
    char gname[80];
    snprintf(gname,80,"dh%s",LocalBasis::directionName(tdir).c_str());
    snprintf(gtitle,80,"Helix Change %s",LocalBasis::directionName(tdir).c_str());
    dhcan[idir] = new TCanvas(gname,gtitle,1200,800);
    dhcan[idir]->Divide(3,2);
    for(size_t ipar = 0; ipar < KTRAJ::NParams(); ipar++){
      dhcan[idir]->cd(ipar+1);
      pgraphs[idir][ipar]->Draw("AC*");
    }
    dhcan[idir]->Draw();
    dhcan[idir]->Write();

    snprintf(gname,80,"dm%s",LocalBasis::directionName(tdir).c_str());
    snprintf(gtitle,80,"Mom Change %s",LocalBasis::directionName(tdir).c_str());
    dmomcan[idir] = new TCanvas(gname,gtitle,800,800);
    dmomcan[idir]->Divide(2,2);
    dmomcan[idir]->cd(1);
    momgraph[idir]->Draw("AC*");
    for(int jdir=0;jdir < 3;jdir++){
      dmomcan[idir]->cd(2+jdir);
      gapgraph[idir][jdir]->Draw("AC*");
    }
    dmomcan[idir]->Draw();
    dmomcan[idir]->Write();
  }

  // test parameter<->phase space translation
  auto dPdX = refhel.dPardX(ttest);  
  auto dPdM = refhel.dPardM(ttest);  
  auto dXdP = refhel.dXdPar(ttest);  
  auto dMdP = refhel.dMdPar(ttest);  
  auto dPdS = refhel.dPardState(ttest);
  auto dSdP = refhel.dStatedPar(ttest);
  auto ptest = dPdS*dSdP;
  for(size_t irow=0;irow<KTRAJ::NParams();irow++) {
    for(size_t icol=0;icol<KTRAJ::NParams();icol++) {
      double val(0.0);
      if(irow==icol)val = 1.0;
      if(fabs(ptest(irow,icol) - val) > 1e-9)  cout <<"Error in parameter derivative test" << endl;
    }
  }
  auto xtest = dXdP*dPdX;
  for(size_t irow=0;irow<3;irow++) {
    for(size_t icol=0;icol<3;icol++) {
      double val(0.0);
      if(irow==icol)val = 1.0;
      if(fabs(xtest(irow,icol) - val) > 1e-9) cout <<"Error in position derivative test" << endl;
    }
  }
  auto mtest = dMdP*dPdM;
  for(size_t irow=0;irow<3;irow++) {
    for(size_t icol=0;icol<3;icol++) {
      double val(0.0);
      if(irow==icol)val = 1.0;
      if(fabs(mtest(irow,icol) - val) > 1e-9) cout <<"Error in momentum derivative test" << endl;
    }
  }

// test changes due to BField
  TCanvas* dbcan[3]; // 3 directions
  std::vector<TGraph*> bpgraphs[3];
  std::array<Vec3,3> basis = {Vec3(1.0,0.0,0.0), Vec3(0.0,1.0,0.0), Vec3(0.0,0.0,1.0) };
  std::array<std::string,3> anames = {"X", "Y", "Z"};
  // gaps
  TGraph* bgapgraph[3];
  for(int idir=0;idir<3;++idir){
    bpgraphs[idir] = std::vector<TGraph*>(KTRAJ::NParams(),0); 
    for(size_t ipar = 0; ipar < KTRAJ::NParams(); ipar++){
      bpgraphs[idir][ipar] = new TGraph(ndel);
      string title = KTRAJ::paramName(typename KTRAJ::ParamIndex(ipar));
      title += ";exact;1st derivative";
      bpgraphs[idir][ipar]->SetTitle(title.c_str());
    }
    bgapgraph[idir] = new TGraph(ndel);
    string title = "Gap for #Delta B in " + anames[idir] + ";Fractional change;Gap value (mm)";
    bgapgraph[idir]->SetTitle(title.c_str());
    for(int id=0;id<ndel;++id){
      // construct exact helix for this field and the corresponding exact parameter change
      double delta = dmin + del*id;
      Vec3 bf = bnom + basis[idir]*delta;
      auto state = refhel.measurementState(ttest);
      // exact traj given the full state
      KTRAJ newbfhel(state,ttest,refhel.mass(),refhel.charge(),bf);
      auto newstate = newbfhel.measurementState(ttest);
      for(size_t ipar=0;ipar < StateVector::dimension(); ipar++){
	if(fabs(state.stateVector().state()[ipar] - newstate.stateVector().state()[ipar])>1.0e-6) cout << "State vector " << ipar << " doesn't match: original "
	  << state.stateVector().state()[ipar] << " rotated " << newstate.stateVector().state()[ipar]  << endl;
      }
      DVEC dpx = newbfhel.params().parameters() - refhel.params().parameters();
      // 1st order change trajectory
      KTRAJ dbtraj(refhel,bf,ttest);
      DVEC dpdb = dbtraj.params().parameters() - refhel.params().parameters();
      for(size_t ipar = 0; ipar < KTRAJ::NParams(); ipar++){
	bpgraphs[idir][ipar]->SetPoint(id,dpx[ipar], dpdb[ipar]);
      }
      bgapgraph[idir]->SetPoint(id,delta,(dbtraj.position(ttest)-newbfhel.position(ttest)).R());
    }
    char gtitle[80];
    char gname[80];
    snprintf(gname,80,"db%s",anames[idir].c_str());
    snprintf(gtitle,80,"BField Change %s",anames[idir].c_str());
    dbcan[idir] = new TCanvas(gname,gtitle,1200,800);
    dbcan[idir]->Divide(3,2);
    for(size_t ipar = 0; ipar < KTRAJ::NParams(); ipar++){
      dbcan[idir]->cd(ipar+1);
      bpgraphs[idir][ipar]->Draw("AC*");
    }
    dbcan[idir]->Draw();
    dbcan[idir]->Write();

  }
  TCanvas* dbgcan = new TCanvas("dbgcan","DB Gap",800,800);
  dbgcan->Divide(2,2);
  for(int idir=0;idir<3;++idir){
    dbgcan->cd(idir+1);
    bgapgraph[idir]->Draw("AC*");
  }
  dbgcan->Draw();
  dbgcan->Write();

  lhderiv.Write();
  lhderiv.Close();
  return 0;
}


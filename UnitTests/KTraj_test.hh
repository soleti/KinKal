// 
// test basic functions of KTraj class
//
#include "KinKal/TLine.hh"
#include "KinKal/TPoca.hh"
#include "CLHEP/Units/PhysicalConstants.h"

#include <iostream>
#include <stdio.h>
#include <iostream>
#include <getopt.h>

#include "TH1F.h"
#include "TSystem.h"
#include "THelix.h"
#include "TPolyLine3D.h"
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
// avoid confusion with root
using KinKal::TLine;

void print_usage() {
  printf("Usage: LHelix  --momentum f --costheta f --azimuth f --particle i --charge i --zorigin f --torigin --tmin f--tmax f --ltime f --By f --invert i\n");
}

struct MomVec {
  TPolyLine3D* arrow;
  TPolyMarker3D *start, *end;
  MomVec() : arrow(new TPolyLine3D(2)), start(new TPolyMarker3D(1,21)), end(new TPolyMarker3D(1,22)) {}
};

void drawMom(Vec3 const& start, Vec3 const& momvec,int momcolor,MomVec& mom) {
  mom.arrow->SetPoint(0,start.X(),start.Y(),start.Z());
  auto end = start + momvec;
  mom.arrow->SetPoint(1,end.X(),end.Y(),end.Z());
  mom.arrow->SetLineColor(momcolor);
  mom.arrow->Draw();
  mom.start->SetPoint(1,start.X(),start.Y(),start.Z());
  mom.start->SetMarkerColor(momcolor);
  mom.start->Draw();
  mom.end->SetPoint(1,end.X(),end.Y(),end.Z());
  mom.end->SetMarkerColor(momcolor);
  mom.end->Draw();
}

template <class KTRAJ>
int test(int argc, char **argv) {
  int opt;
  double mom(105.0), cost(0.7), phi(0.5);
  double masses[5]={0.511,105.66,139.57, 493.68, 938.0};
  int imass(0), icharge(-1);
  double pmass, oz(100.0), ot(0.0);
  double tmin(-5.0), tmax(5.0);
  double ltime(3.0), vprop(0.8), gap(2.0);
  double hlen(500.0); // half-length of the wire
  double By(0.0);
  int invert(0);

  static struct option long_options[] = {
    {"momentum",     required_argument, 0, 'm' },
    {"costheta",     required_argument, 0, 'c'  },
    {"azimuth",     required_argument, 0, 'a'  },
    {"particle",     required_argument, 0, 'p'  },
    {"charge",     required_argument, 0, 'q'  },
    {"zorigin",     required_argument, 0, 'z'  },
    {"torigin",     required_argument, 0, 't'  },
    {"tmin",     required_argument, 0, 's'  },
    {"tmax",     required_argument, 0, 'e'  },
    {"ltime",     required_argument, 0, 'l'  },
    {"By",     required_argument, 0, 'y'  },
    {"invert",     required_argument, 0, 'I'  },
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
      case 't' : ot = atof(optarg);
		 break;
      case 's' : tmin = atof(optarg);
		 break;
      case 'e' : tmax = atof(optarg);
		 break;
      case 'l' : ltime = atof(optarg);
		 break;
      case 'y' : By = atof(optarg);
		 break;
      case 'I' : invert = atoi(optarg);
		 break;
      default: print_usage(); 
	       exit(EXIT_FAILURE);
    }
  }

  pmass = masses[imass];

  printf("Testing KTraj with momentum = %f, costheta = %f, phi = %f, mass = %f, charge = %i, z = %f, t = %f \n",mom,cost,phi,pmass,icharge,oz,ot);
// define the BF (tesla)
  Vec3 bnom(0.0,By,1.0);
  Vec4 origin(0.0,0.0,oz,ot);
  double sint = sqrt(1.0-cost*cost);
  Mom4 momv(mom*sint*cos(phi),mom*sint*sin(phi),mom*cost,pmass);
  KTRAJ lhel(origin,momv,icharge,bnom,TRange(-10,10));
  if(invert)lhel.invertCT();
  Mom4 testmom = lhel.momentum(ot);
  cout << "KTRAJ with momentum " << momv << " position " << origin << " has parameters: " << lhel << endl;
  Vec3 tvel, tdir;
  double ttime;
  double tstp = lhel.range().range()/9;
  for(int istep=0;istep<10;istep++){
    ttime = lhel.range().low() + istep*tstp;
    tvel = lhel.velocity(ttime);
    tdir = lhel.direction(ttime);
    testmom = lhel.momentum(ttime);
    cout << "velocity " << tvel << " direction " << tdir << " momentum " << testmom << endl;
//    cout << "momentum beta =" << testmom.Beta() << " KTRAJ beta = " << lhel.beta() << " momentum gamma  = " << testmom.Gamma() << 
//      " KTRAJ gamma = " << lhel.gamma() << " scalar mom " << lhel.momentum(ot) << endl;
  }
  Vec3 mdir = lhel.direction(ot);
  // create the helix at tmin and tmax 
  Mom4 tmom;
  Vec4 tpos; 
  tmom = lhel.momentum(tmax);
  tpos.SetE(tmax);
  lhel.position(tpos);
  KTRAJ lhelmax(tpos,tmom,icharge,bnom);
  tmom = lhel.momentum(tmin);
  tpos.SetE(tmin);
  lhel.position(tpos);
  KTRAJ lhelmin(tpos,tmom,icharge,bnom);

  cout << "KTRAJ at tmax has parameters : " << lhelmax << endl;
  cout << "KTRAJ at tmin has parameters : " << lhelmin << endl;

// now graph this as a polyline over the specified time range.
  double tstep = 0.1; // nanoseconds
  double trange = tmax-tmin;
  int nsteps = (int)rint(trange/tstep);
// create Canvase
  TCanvas* hcan = new TCanvas("hcan","Helix",1000,1000);
//TPolyLine to graph the result
  TPolyLine3D* hel = new TPolyLine3D(nsteps+1);
  Vec4 hpos;
  for(int istep=0;istep<nsteps+1;++istep){
  // compute the position from the time
    hpos.SetE(tmin + tstep*istep);
    lhel.position(hpos);
    // add these positions to the TPolyLine3D
    hel->SetPoint(istep, hpos.X(), hpos.Y(), hpos.Z());
  }
  // draw the helix
  if(icharge > 0)
    hel->SetLineColor(kBlue);
  else
    hel->SetLineColor(kRed);
  hel->Draw();
// inversion test
  TPolyLine3D* ihel = new TPolyLine3D(nsteps+1);
  auto ilhel = lhel;
  ilhel.invertCT();
  for(int istep=0;istep<nsteps+1;++istep){
  // compute the position from the time
    hpos.SetE(tmin + tstep*istep);
    ilhel.position(hpos);
    // add these positions to the TPolyLine3D
    ihel->SetPoint(istep, hpos.X(), hpos.Y(), hpos.Z());
  }
  // draw the helix
  ihel->SetLineColor(kBlue);
  ihel->SetLineStyle(kDashDotted);
  ihel->Draw();
// now draw momentum vectors at reference, start and end
  MomVec imstart,imend,imref;
  Vec3 imompos = ilhel.position(ot);
  mdir =ilhel.direction(ot);
  Vec3 imomvec =mom*mdir;
  drawMom(imompos,imomvec,kBlack,imref);
  //
  imompos  = lhel.position(tmin);
  mdir = lhel.direction(tmin);
  imomvec =mom*mdir;
  drawMom(imompos,imomvec,kBlue,imstart);
  //
  imompos = lhel.position(tmax);
  mdir = lhel.direction(tmax);
  imomvec =mom*mdir;
  drawMom(imompos,imomvec,kGreen,imend);
  //

  // draw the origin and axes
  TAxis3D* rulers = new TAxis3D();
  rulers->GetXaxis()->SetAxisColor(kBlue);
  rulers->GetXaxis()->SetLabelColor(kBlue);
  rulers->GetYaxis()->SetAxisColor(kCyan);
  rulers->GetYaxis()->SetLabelColor(kCyan);
  rulers->GetZaxis()->SetAxisColor(kOrange);
  rulers->GetZaxis()->SetLabelColor(kOrange);
  rulers->Draw();

// now draw momentum vectors at reference, start and end
  MomVec mstart,mend,mref;
  Vec3 mompos = lhel.position(ot);
  mdir = lhel.direction(ot);
  Vec3 momvec =mom*mdir;
  drawMom(mompos,momvec,kBlack,mref);
  //
  mompos = lhel.position(tmin);
  mdir = lhel.direction(tmin);
  momvec =mom*mdir;
  drawMom(mompos,momvec,kBlue,mstart);
  //
  mompos = lhel.position(tmax);
  mdir = lhel.direction(tmax);
  momvec =mom*mdir;
  drawMom(mompos,momvec,kGreen,mend);
  //
  TLegend* leg = new TLegend(0.8,0.8,1.0,1.0);
  char title[80];
  snprintf(title,80,"KTRAJ, q=%1i, mom=%3.1g MeV/c",icharge,mom);
  leg->AddEntry(hel,title,"L");
  snprintf(title,80,"Ref. Momentum, t=%4.2g ns",ot);
  leg->AddEntry(mref.arrow,title,"L");
  snprintf(title,80,"Start Momentum, t=%4.2g ns",ot+tmin);
  leg->AddEntry(mstart.arrow,title,"L");
  snprintf(title,80,"End Momentum, t=%4.2g ns",ot+tmax);
  leg->AddEntry(mend.arrow,title,"L");
  leg->Draw();

  // create a TLine near this helix, and draw it and the TPoca vector
  Vec3 pos = lhel.position(ltime);
  Vec3 dir = lhel.direction(ltime);
  // rotate the direction
  double lhphi = atan2(dir.Y(),dir.X());
  double pphi = lhphi + M_PI/2.0;
  Vec3 pdir(cos(pphi),sin(pphi),0.0);
  double pspeed = CLHEP::c_light*vprop; // vprop is relative to c
  Vec3 pvel = pdir*pspeed;
  // shift the position
  Vec3 perpdir(-sin(phi),cos(phi),0.0);
  Vec3 ppos = pos + gap*perpdir;
// time range;
  TRange prange(ltime-hlen/pspeed, ltime+hlen/pspeed);
  TLine tline(ppos, pvel,ltime,prange);
// find TPoca
//  TPoca<KTRAJ,TLine> tp(lhel,tline);
//  cout << "TPoca status " << tp.statusName() << " doca " << tp.doca() << " dt " << tp.deltaT() << endl;
//  if(tp.status() == TPocaBase::converged) {
//    // draw the line and TPoca
//    TPolyLine3D* line = new TPolyLine3D(2);
//    Vec3 plow, phigh;
//    tline.position(tline.range().low(),plow);
//    tline.position(tline.range().high(),phigh);
//    line->SetPoint(0,plow.X(),plow.Y(), plow.Z());
//    line->SetPoint(1,phigh.X(),phigh.Y(), phigh.Z());
//    line->SetLineColor(kOrange);
//    line->Draw();
//    TPolyLine3D* poca = new TPolyLine3D(2);
//    poca->SetPoint(0,tp.particlePoca().X() ,tp.particlePoca().Y() ,tp.particlePoca().Z());
//    poca->SetPoint(1,tp.sensorPoca().X() ,tp.sensorPoca().Y() ,tp.sensorPoca().Z());
//    poca->SetLineColor(kBlack);
//    poca->Draw();
//  }

  std::string tfname = KTRAJ::trajName() + ".root";
  cout << "Saving canvas to " << title << endl;
  hcan->SaveAs(tfname.c_str()); 

  exit(EXIT_SUCCESS);
}


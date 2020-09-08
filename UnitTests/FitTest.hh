//
// ToyMC test of fitting an KTRAJ-based KKTrk
//
#include "MatEnv/MatDBInfo.hh"
#include "MatEnv/DetMaterial.hh"
#include "KinKal/PKTraj.hh"
#include "KinKal/TLine.hh"
#include "KinKal/StrawHit.hh"
#include "KinKal/StrawMat.hh"
#include "KinKal/ScintHit.hh"
#include "KinKal/BField.hh"
#include "KinKal/Vectors.hh"
#include "KinKal/KKConfig.hh"
#include "KinKal/KKHit.hh"
#include "KinKal/KKMHit.hh"
#include "KinKal/DXing.hh"
#include "KinKal/KKTrk.hh"
#include "UnitTests/ToyMC.hh"
#include "UnitTests/KKHitInfo.hh"
#include "UnitTests/KKMatInfo.hh"
#include "UnitTests/KKBFieldInfo.hh"
#include "UnitTests/KTrajInfo.hh"
#include "CLHEP/Units/PhysicalConstants.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <typeinfo>
#include <vector>
#include <cmath>
#include <ctime>
#include <chrono>
#include <cfenv>
#include <memory>
#include <cstdlib>
#include <cstring>

#include "TH1F.h"
#include "TTree.h"
#include "TBranch.h"
#include "TSystem.h"
#include "THelix.h"
#include "TPolyLine3D.h"
#include "TAxis3D.h"
#include "TFile.h"
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
#include "Math/VectorUtil.h"
#include <limits>

using namespace MatEnv;
using namespace KinKal;
using namespace std;
// avoid confusion with root
using KinKal::TLine;
void print_usage() {
  printf("Usage: FitTest  --momentum f --simparticle i --fitparticle i--charge i --nhits i --hres f --seed i -maxniter i --deweight f --ambigdoca f --ntries i --simmat i--fitmat i --ttree i --Bz f --dBx f --dBy f --dBz f--Bgrad f --tolerance f--TFile c --PrintBad i --PrintDetail i --ScintHit i --bfcorr i --invert i --Schedule a --ssmear i\n");
}

// utility function to compute transverse distance between 2 similar trajectories.  Also
// return the difference in time at the comparison point
template <class KTRAJ>
double dTraj(KTRAJ const& kt1, KTRAJ const& kt2, double t1, double& t2) {
  double dt = numeric_limits<float>::max();
  Vec3 pos1 = kt1.position(t1);
  Vec3 dir1 = kt1.direction(t1);
  t2 = t1;
  Vec3 delta, v2;
  while(fabs(dt) > 1e-5){
    v2 = kt2.velocity(t2);
    delta = kt2.position(t2) - pos1;
    dt = delta.Dot(v2)/v2.Mag2();
    t2 -= dt;
  }
  return (delta.Cross(dir1)).R();
}

template <class KTRAJ>
int FitTest(int argc, char **argv) {
  struct KTRAJPars{
    Float_t pars_[NParams()];
    static std::string leafnames() {
      std::string names;
      for(size_t ipar=0;ipar<NParams();ipar++){
	names +=KTRAJ::paramName(static_cast<typename KTRAJ::ParamIndex>(ipar)) + string("/f");
	if(ipar < NParams()-1)names += ":";
      }
      return names;
    }
  };

  using KKEFF = KKEff<KTRAJ>;
  using KKHIT = KKHit<KTRAJ>;
  using KKMAT = KKMat<KTRAJ>;
  using KKMHIT = KKMHit<KTRAJ>;
  using KKBF = KKBField<KTRAJ>;
  using KKEND = KKEnd<KTRAJ>;
  using PKTRAJ = PKTraj<KTRAJ>;
  using THIT = THit<KTRAJ>;
  using THITPTR = std::shared_ptr<THIT>;
  using THITCOL = std::vector<THITPTR>;
  using DXING = DXing<KTRAJ>;
  using DXINGPTR = std::shared_ptr<DXING>;
  using DXINGCOL = std::vector<DXINGPTR>;
  using KKBFIELD = KKBField<KTRAJ>;
  using KKTRK = KKTrk<KTRAJ>;
  using KKCONFIGPTR = std::shared_ptr<KKConfig>;
  using STRAWHIT = StrawHit<KTRAJ>;
  using STRAWHITPTR = std::shared_ptr<STRAWHIT>;
  using SCINTHIT = ScintHit<KTRAJ>;
  using SCINTHITPTR = std::shared_ptr<SCINTHIT>;
  using Clock = std::chrono::high_resolution_clock;

  // enable throw on FPE; not working with clang!
  fetestexcept(FE_ALL_EXCEPT );
  // parameters
  int opt;
  double mom(105.0);
  double masses[5]={0.511,105.66,139.57, 493.68, 938.0};
  int isimmass(0), ifitmass(0), icharge(-1);
  double simmass, fitmass;
  unsigned maxniter(5);
  double dwt(1.0e6);
  unsigned ntries(100);
  bool ttree(true), printbad(false);
  string tfname("FitTest.root"), sfile("Schedule.txt");
  int detail(KKConfig::minimal), invert(0);
  double ambigdoca(-1.0);// minimum doca to set ambiguity, default sets for all hits
  KKConfig::BFieldCorr bfcorr(KKConfig::nocorr);
  bool fitmat(true);
  vector<double> sigmas = { 3.0, 3.0, 3.0, 3.0, 0.1, 3.0}; // base sigmas for parameter plots
  BField *BF(0);
  double Bgrad(0.0), dBx(0.0), dBy(0.0), dBz(0.0), Bz(1.0);
  double zrange(3000);
  double tol(0.1);
  int iseed(123421);
  unsigned nhits(40);
  unsigned nsteps(200); // steps for traj comparison
  bool simmat(true), lighthit(true), seedsmear(true);

  static struct option long_options[] = {
    {"momentum",     required_argument, 0, 'm' },
    {"simparticle",     required_argument, 0, 'S'  },
    {"fitparticle",     required_argument, 0, 'F'  },
    {"charge",     required_argument, 0, 'q'  },
    {"seed",     required_argument, 0, 's'  },
    {"hres",     required_argument, 0, 'h'  },
    {"nhits",     required_argument, 0, 'n'  },
    {"maxniter",     required_argument, 0, 'i'  },
    {"deweight",     required_argument, 0, 'w'  },
    {"simmat",     required_argument, 0, 'b'  },
    {"fitmat",     required_argument, 0, 'f'  },
    {"ambigdoca",     required_argument, 0, 'd'  },
    {"ntries",     required_argument, 0, 'N'  },
    {"ttree",     required_argument, 0, 'r'  },
    {"tolerance",     required_argument, 0, 't'  },
    {"TFile",     required_argument, 0, 'T'  },
    {"dBx",     required_argument, 0, 'x'  },
    {"dBy",     required_argument, 0, 'y'  },
    {"dBz",     required_argument, 0, 'Z'  },
    {"Bz",     required_argument, 0, 'z'  },
    {"Bgrad",     required_argument, 0, 'g'  },
    {"PrintBad",     required_argument, 0, 'P'  },
    {"PrintDetail",     required_argument, 0, 'D'  },
    {"ScintHit",     required_argument, 0, 'L'  },
    {"bfcorr",     required_argument, 0, 'B'  },
    {"invert",     required_argument, 0, 'I'  },
    {"Schedule",     required_argument, 0, 'u'  },
    {"seedsmear",     required_argument, 0, 'M' },
    {NULL, 0,0,0}
  };

  int long_index =0;
  while ((opt = getopt_long_only(argc, argv,"",
	  long_options, &long_index )) != -1) {
    switch (opt) {
      case 'm' : mom = atof(optarg);
		 break;
      case 'S' : isimmass = atoi(optarg);
		 break;
      case 'F' : ifitmass = atoi(optarg);
		 break;
      case 'q' : icharge = atoi(optarg);
		 break;
      case 'z' : Bz = atof(optarg);
		 break;
      case 'n' : nhits = atoi(optarg);
		 break;
      case 's' : iseed = atoi(optarg);
		 break;
      case 'i' : maxniter = atoi(optarg);
		 break;
      case 'w' : dwt = atof(optarg);
		 break;
      case 'b' : simmat = atoi(optarg);
		 break;
      case 'f' : fitmat = atoi(optarg);
		 break;
      case 'L' : lighthit = atoi(optarg);
		 break;
      case 'B' : bfcorr = KKConfig::BFieldCorr(atoi(optarg));
		 break;
      case 'r' : ttree = atoi(optarg);
		 break;
      case 'd' : ambigdoca = atof(optarg);
		 break;
      case 'M' : seedsmear = atoi(optarg);
		 break;
      case 'N' : ntries = atoi(optarg);
		 break;
      case 'x' : dBx = atof(optarg);
		 break;
      case 'y' : dBy = atof(optarg);
		 break;
      case 'Z' : dBz = atof(optarg);
		 break;
      case 'g' : Bgrad = atof(optarg);
		 break;
      case 'P' : printbad = atoi(optarg);
		 break;
      case 'D' : detail = atoi(optarg);
		 break;
      case 'I' : invert = atoi(optarg);
		 break;
      case 't' : tol = atof(optarg);
		 break;
      case 'T' : tfname = optarg;
		 break;
      case 'u' : sfile = optarg;
		 break;
      default: print_usage();
	       exit(EXIT_FAILURE);
    }
  }

  // construct BField
  Vec3 bnom;
  if(Bgrad != 0){
    BF = new GradBField(Bz-0.5*Bgrad,Bz+0.5*Bgrad,-0.5*zrange,0.5*zrange);
    bnom = BF->fieldVect(Vec3(0.0,0.0,0.0));
  } else {
    Vec3 bsim(dBx,dBy,Bz+dBz);
    BF = new UniformBField(bsim);
    bnom = Vec3(0.0,0.0,Bz);
  }
  // create ToyMC
  simmass = masses[isimmass];
  fitmass = masses[ifitmass];
  KKTest::ToyMC<KTRAJ> toy(*BF, mom, icharge, zrange, iseed, nhits, simmat, lighthit, ambigdoca, simmass );
  // generate hits
  THITCOL thits; // this program shares hit ownership with KKTrk
  DXINGCOL dxings; // this program shares det xing ownership with KKTrk
  PKTRAJ tptraj;
  toy.simulateParticle(tptraj, thits, dxings);
  toy.setSmearSeed(seedsmear);
  cout << "True initial " << tptraj.front() << endl;
//  cout << "vector of hit points " << thits.size() << endl;
//  cout << "True " << tptraj << endl;
  double startmom = tptraj.momentumMag(tptraj.range().begin());
  double endmom = tptraj.momentumMag(tptraj.range().end());
  Vec3 end, bend;
  bend = tptraj.front().direction(tptraj.range().end());
  end = tptraj.back().direction(tptraj.range().end());
  double angle = ROOT::Math::VectorUtil::Angle(bend,end);
  cout << "total momentum change = " << endmom-startmom << " total angle change = " << angle << endl;
  // create the fit seed by randomizing the parameters at the middle.  Overrwrite to use the fit BField
  auto const& midhel = tptraj.nearestPiece(0.0);
  auto seedmom = midhel.momentum(0.0);
  seedmom.SetM(fitmass);
  // buffer the seed range
  TRange seedrange(tptraj.range().begin()-0.5,tptraj.range().end()+0.5);
  KTRAJ seedtraj(midhel.pos4(0.0),seedmom,midhel.charge(),bnom,seedrange);
  if(invert) seedtraj.invertCT(); // for testing wrong propagation direction
  toy.createSeed(seedtraj);
  cout << "Seed params " << seedtraj.params().parameters() <<" covariance " << endl << seedtraj.params().covariance() << endl;
  // Create the KKTrk from these hits
  //
  KKCONFIGPTR configptr = make_shared<KKConfig>(*BF);
  configptr->dwt_ = dwt;
  configptr->maxniter_ = maxniter;
  configptr->bfcorr_ = bfcorr;
  configptr->addmat_ = fitmat;
  configptr->tol_ = tol;
  configptr->plevel_ = (KKConfig::printLevel)detail;
  // read the schedule from the file
  string fullfile;
  if(strncmp(sfile.c_str(),"/",1) == 0) {
    fullfile = string(sfile);
  } else {
    if(const char* source = std::getenv("PACKAGE_SOURCE")){
      fullfile = string(source) + string("/UnitTests/") + string(sfile);
    } else {
      cout << "PACKAGE_SOURCE not defined" << endl;
      return -1;
    }
  }
  std::ifstream ifs (fullfile, std::ifstream::in);
  string line;
  unsigned nmiter(0);
  while (getline(ifs,line)){ 
    if(strncmp(line.c_str(),"#",1)!=0){
      istringstream ss(line);
      MIConfig mconfig(ss);
      mconfig.miter_ = nmiter++;
      configptr->schedule_.push_back(mconfig);
    }
  }
  cout << *configptr << endl;
// create and fit the track
  KKTRK kktrk(configptr,seedtraj,thits,dxings);
//  kktrk.print(cout,detail);
  TFile fitfile((KTRAJ::trajName() + tfname).c_str(),"RECREATE");
  // tree variables
  KTRAJPars ftpars_, btpars_, spars_, ffitpars_, ffiterrs_, bfitpars_, bfiterrs_;
  float chisq_, btmom_, ftmom_, ffmom_, bfmom_, ffmomerr_, bfmomerr_, chiprob_;
  float fft_,eft_;
  int ndof_, niter_, status_, igap_, nmeta_, nkkbf_, nkkhit_, nkkmat_;
  float maxgap_, avgap_;

  if(ntries <=0 ){
    // draw the fit result
    TCanvas* pttcan = new TCanvas("pttcan","PieceKTRAJ",1000,1000);
    auto const& fithel = kktrk.fitTraj();
    unsigned np = fithel.range().range()*fithel.speed(fithel.range().mid());
    TPolyLine3D* fitpl = new TPolyLine3D(np);
    fitpl->SetLineColor(kBlue);
    fitpl->SetLineStyle(kSolid);
    double ts = fithel.range().range()/(np-1);
    for(unsigned ip=0;ip<np;ip++){
      double tp = fithel.range().begin() + ip*ts;
      Vec3 ppos = fithel.position(tp);
      fitpl->SetPoint(ip,ppos.X(),ppos.Y(),ppos.Z());
    }
    fitpl->Draw();
// now draw the truth
    TPolyLine3D* ttpl = new TPolyLine3D(np);
    ttpl->SetLineColor(kGreen);
    ttpl->SetLineStyle(kDashDotted);
    ts = tptraj.range().range()/(np-1);
    for(unsigned ip=0;ip<np;ip++){
      double tp = tptraj.range().begin() + ip*ts;
      Vec3 ppos = tptraj.position(tp);
      ttpl->SetPoint(ip,ppos.X(),ppos.Y(),ppos.Z());
    }
    ttpl->Draw();
    // draw the hits
    std::vector<TPolyLine3D*> htpls;
    for(auto const& thit : thits) {
      TPolyLine3D* line = new TPolyLine3D(2);
      Vec3 plow, phigh;
      STRAWHITPTR shptr = std::dynamic_pointer_cast<STRAWHIT> (thit); 
      SCINTHITPTR lhptr = std::dynamic_pointer_cast<SCINTHIT> (thit);
      if(shptr.use_count() > 0){
	auto const& tline = shptr->wire();
	plow = tline.position(tline.range().begin());
	phigh = tline.position(tline.range().end());
	line->SetLineColor(kRed);
      } else if (lhptr.use_count() > 0){
	auto const& tline = lhptr->sensorAxis();
	plow = tline.position(tline.range().begin());
	phigh = tline.position(tline.range().end());
	line->SetLineColor(kCyan);
      }
      line->SetPoint(0,plow.X(),plow.Y(), plow.Z());
      line->SetPoint(1,phigh.X(),phigh.Y(), phigh.Z());
      line->Draw();
      htpls.push_back(line);
    }

    // draw the origin and axes
    TAxis3D* rulers = new TAxis3D();
    rulers->GetXaxis()->SetAxisColor(kBlue);
    rulers->GetXaxis()->SetLabelColor(kBlue);
    rulers->GetYaxis()->SetAxisColor(kCyan);
    rulers->GetYaxis()->SetLabelColor(kCyan);
    rulers->GetZaxis()->SetAxisColor(kOrange);
    rulers->GetZaxis()->SetLabelColor(kOrange);
    rulers->Draw();
    pttcan->Write();

  } else {
    TTree* ftree(0);
    KKHIV hinfovec;
    KKBFIV bfinfovec;
    KKMIV minfovec;
    KTIV tinfovec;
    if(ttree){
      ftree = new TTree("fit","fit");
      ftree->Branch("ftpars.", &ftpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("btpars.", &btpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("spars.", &spars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("ffpars.", &ffitpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("fferrs.", &ffiterrs_,KTRAJPars::leafnames().c_str());
      ftree->Branch("bfpars.", &bfitpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("bferrs.", &bfiterrs_,KTRAJPars::leafnames().c_str());
      ftree->Branch("chisq", &chisq_,"chisq/F");
      ftree->Branch("ndof", &ndof_,"ndof/I");
      ftree->Branch("nkkbf", &nkkbf_,"nkkbf/I");
      ftree->Branch("nkkmat", &nkkmat_,"nkkmat/I");
      ftree->Branch("nkkhit", &nkkhit_,"nkkhit/I");
      ftree->Branch("chiprob", &chiprob_,"chiprob/F");
      ftree->Branch("niter", &niter_,"niter/I");
      ftree->Branch("nmeta", &nmeta_,"nmeta/I");
      ftree->Branch("status", &status_,"status/I");
      ftree->Branch("ftmom", &ftmom_,"ftmom/F");
      ftree->Branch("btmom", &btmom_,"btmom/F");
      ftree->Branch("ffmom", &ffmom_,"ffmom/F");
      ftree->Branch("bfmom", &bfmom_,"bfmom/F");
      ftree->Branch("ffmomerr", &ffmomerr_,"ffmomerr/F");
      ftree->Branch("bfmomerr", &bfmomerr_,"bfmomerr/F");
      ftree->Branch("fft", &fft_,"fft/F");
      ftree->Branch("eft", &eft_,"eft/F");
      ftree->Branch("maxgap", &maxgap_,"maxgap/F");
      ftree->Branch("avgap", &avgap_,"avgap/F");
      ftree->Branch("igap", &igap_,"igap/I");
      ftree->Branch("hinfovec",&hinfovec);
      ftree->Branch("bfinfovec",&bfinfovec);
      ftree->Branch("minfovec",&minfovec);
      ftree->Branch("tinfovec",&tinfovec);
    }
    // now repeat this to gain statistics
    vector<TH1F*> fdp(NParams());
    vector<TH1F*> bdp(NParams());
    vector<TH1F*> fpull(NParams());
    vector<TH1F*> bpull(NParams());
    vector<TH1F*> fiterrh(NParams());
    TH1F* hniter = new TH1F("niter", "Total Iterations", 50,-0.5,49.5);
    TH1F* hnmeta = new TH1F("nmeta", "Meta Iterations", 10,-0.5,9.5);
    TH1F* hnfail = new TH1F("nfail", "Failed Iterations", 50,-0.5,49.5);
    TH1F* hndiv = new TH1F("ndiv", "Diverged Iterations", 50,-0.5,49.5);
    hnfail->SetLineColor(kRed);
    hndiv->SetLineColor(kOrange);
    TH1F* ndof = new TH1F("ndof", "N Degree of Freedom", 100,-0.5,99.5);
    TH1F* chisq = new TH1F("chisq", "Chisquared", 100,0,100);
    TH1F* chisqndof = new TH1F("chisqndof", "Chisquared per NDOF", 100,0,10.0);
    TH1F* chisqprob = new TH1F("chisqprob", "Chisquared probability", 100,0,1.0);
    TH1F* logchisqprob = new TH1F("logchisqprob", "Log10 of Chisquared probability", 100,-10,0.0);
    string htitle, hname;
    TH2F* corravg = new TH2F("corravg","Average correlation matrix magnitudes",NParams(),-0.5,NParams()-0.5,NParams(), -0.5,NParams()-0.5);
    TAxis* xax = corravg->GetXaxis();
    TAxis* yax = corravg->GetYaxis();
    double nsig(10.0);
    double pscale = nsig/sqrt(nhits);
    for(size_t ipar=0;ipar< NParams(); ipar++){
      auto tpar = static_cast<typename KTRAJ::ParamIndex>(ipar);
      hname = string("fd") + KTRAJ::paramName(tpar);
      htitle = string("Front #Delta ") + KTRAJ::paramTitle(tpar);
      fdp[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-pscale*sigmas[ipar],pscale*sigmas[ipar]);
      hname = string("bd") + KTRAJ::paramName(tpar);
      htitle = string("Back #Delta ") + KTRAJ::paramTitle(tpar);
      bdp[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-pscale*sigmas[ipar],pscale*sigmas[ipar]);
      hname = string("fp") + KTRAJ::paramName(tpar);
      htitle = string("Front Pull ") + KTRAJ::paramTitle(tpar);
      fpull[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-nsig,nsig);
      hname = string("bp") + KTRAJ::paramName(tpar);
      htitle = string("Back Pull ") + KTRAJ::paramTitle(tpar);
      bpull[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-nsig,nsig);
      hname = string("e") + KTRAJ::paramName(tpar);
      htitle = string("Error ") + KTRAJ::paramTitle(tpar);
      fiterrh[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,0.0,pscale*sigmas[ipar]);
      xax->SetBinLabel(ipar+1,KTRAJ::paramName(tpar).c_str());
      yax->SetBinLabel(ipar+1,KTRAJ::paramName(tpar).c_str());
    }
    TH1F* fmompull = new TH1F("fmompull","Front Momentum Pull;#Delta P/#sigma _{p}",100,-nsig,nsig);
    TH1F* bmompull = new TH1F("bmompull","Back Momentum Pull;#Delta P/#sigma _{p}",100,-nsig,nsig);
    double duration (0.0);
    unsigned nfail(0), ndiv(0);

    configptr->plevel_ = KKConfig::none;
    for(unsigned itry=0;itry<ntries;itry++){
    // create a random true initial helix with hits and material interactions from this.  This also handles BField inhomogeneity truth tracking
      PKTRAJ tptraj;
      thits.clear();
      dxings.clear();
      toy.simulateParticle(tptraj,thits,dxings);
      double tmid = tptraj.range().mid();
      auto const& midhel = tptraj.nearestPiece(tmid);
      auto seedmom = midhel.momentum(tmid);
      seedmom.SetM(fitmass);
      TRange seedrange(tptraj.range().begin()-0.5,tptraj.range().end()+0.5);
      KTRAJ seedtraj(midhel.pos4(tmid),seedmom,midhel.charge(),bnom,seedrange);
      if(invert)seedtraj.invertCT();
      toy.createSeed(seedtraj);
      auto start = Clock::now();
      KKTRK kktrk(configptr,seedtraj,thits,dxings);
      auto stop = Clock::now();
      duration += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
      auto const& fstat = kktrk.fitStatus();
      if(fstat.status_ == FitStatus::failed)nfail++;
      if(fstat.status_ == FitStatus::diverged)ndiv++;
      niter_ = 0;
      for(auto const& fstat: kktrk.history()){
	if(fstat.status_ != FitStatus::unfit)niter_++;
      }
      // accumulate chisquared info
      chisq_ = fstat.chisq_;
      ndof_ = fstat.ndof_;
      niter_ = fstat.iter_;
      nmeta_ = fstat.miter_;
      status_ = fstat.status_;
      chiprob_ = fstat.prob_;
      ndof->Fill(fstat.ndof_);
      chisq->Fill(fstat.chisq_);
      chisqndof->Fill(fstat.chisq_/fstat.ndof_);
      chisqprob->Fill(chiprob_);
      if(chiprob_ > 0.0) logchisqprob->Fill(log10(chiprob_));
      hniter->Fill(niter_);
      hnmeta->Fill(nmeta_);
      // truth parameters, front and back
      double ttlow = tptraj.range().begin();
      double tthigh = tptraj.range().end();
      KTRAJ const& fttraj = tptraj.nearestPiece(ttlow);
      KTRAJ const& bttraj = tptraj.nearestPiece(tthigh);
      for(size_t ipar=0;ipar<6;ipar++){
	spars_.pars_[ipar] = seedtraj.params().parameters()[ipar];
	ftpars_.pars_[ipar] = fttraj.params().parameters()[ipar];
	btpars_.pars_[ipar] = bttraj.params().parameters()[ipar];
      }
      ftmom_ = tptraj.momentumMag(ttlow);
      btmom_ = tptraj.momentumMag(tthigh);
      // reset some fit parameters, to signal failed filts
      chiprob_ = -1.0;
      maxgap_ = avgap_ = -1;
      igap_ = -1;
      // fill effect information
      nkkbf_ = 0; nkkhit_ = 0; nkkmat_ = 0;
      hinfovec.clear();
      bfinfovec.clear();
      minfovec.clear();
      tinfovec.clear();
      for(auto const& eff: kktrk.effects()) {
	const KKHIT* kkhit = dynamic_cast<const KKHIT*>(eff.get());
	const KKBF* kkbf = dynamic_cast<const KKBF*>(eff.get());
	const KKMAT* kkmat = dynamic_cast<const KKMAT*>(eff.get());
	const KKMHIT* kkmhit = dynamic_cast<const KKMHIT*>(eff.get());
	if(kkmhit != 0){
	  kkhit = &kkmhit->hit();
	  kkmat = &kkmhit->mat();
	}
	if(kkhit != 0){
	  nkkhit_++;
	  KKHitInfo hinfo;
	  hinfo.active_ = kkhit->isActive();
	  hinfo.time_ = kkhit->time();
	  hinfo.resid_ = kkhit->refResid().value();
	  hinfo.residvar_ = kkhit->refResid().variance();
	  hinfo.fitchi_ = kkhit->fitChi();
	  hinfovec.push_back(hinfo);
	}
	if(kkmat != 0){
	  nkkmat_++;
	  KKMatInfo minfo;
	  minfo.time_ = kkmat->time();
	  minfo.active_ = kkmat->isActive();
	  minfo.nxing_ = kkmat->detXing().matXings().size();
	  std::array<double,3> dmom = {0.0,0.0,0.0}, momvar = {0.0,0.0,0.0};
	  kkmat->detXing().materialEffects(kkmat->refKTraj(),TDir::forwards, dmom, momvar);
	  minfo.dmomf_ = dmom[MomBasis::momdir_];
	  minfo.momvar_ = momvar[MomBasis::momdir_];
	  minfo.perpvar_ = momvar[MomBasis::perpdir_];
	  minfovec.push_back(minfo);
	}
	if(kkbf != 0){
	  nkkbf_++;
	  KKBFieldInfo bfinfo;
	  bfinfo.active_ = kkbf->isActive();
	  bfinfo.dp_ = kkbf->time();
	  bfinfo.dp_ = kkbf->deltaP().R();
	  bfinfo.range_ = kkbf->range().range();
	  bfinfovec.push_back(bfinfo);
	}
      }

      if(fstat.usable()){
	// step through the fit traj and compare to the truth
	double deltat0(0.0);
	double dt = kktrk.fitTraj().range().range()/nsteps;
	for(unsigned istep=0;istep < nsteps;istep++){
	  double tstep = kktrk.fitTraj().range().begin()+dt*istep;
	  double ttrue;
	  double dperp = dTraj(kktrk.fitTraj(),tptraj,tstep,ttrue);
	  KTrajInfo ktinfo;
	  ktinfo.time_ = tstep;
	  ktinfo.dperp_ = dperp;
	  ktinfo.dt_= tstep-ttrue;
	  tinfovec.push_back(ktinfo);
	  deltat0 += tstep-ttrue;
	}
	deltat0/= float(nsteps);
	// compare parameters at the first traj of both true and fit
	// correct the true parameters in case the BField isn't nominal
	// correct the sampling time for the t0 difference
	double ftlow = ttlow - deltat0;
	double fthigh = tthigh - deltat0;
	KTRAJ fftraj = KTRAJ(kktrk.fitTraj().nearestPiece(ftlow),tptraj.bnom(ttlow),ftlow);
	KTRAJ bftraj = KTRAJ(kktrk.fitTraj().nearestPiece(fthigh),tptraj.bnom(tthigh),fthigh);
	// fit parameters
	auto const& ffpars = fftraj.params();
	auto const& bfpars = bftraj.params();
	double maxgap, avgap;
	size_t igap;
	kktrk.fitTraj().gaps(maxgap, igap, avgap);
	maxgap_ = maxgap;
	avgap_ = avgap;
	igap_ = igap;

	PData ftpars, btpars;
	ftpars = fttraj.params();
	btpars = bttraj.params();

	// accumulate parameter difference and pull
	vector<double> fcerr(6,0.0), bcerr(6,0.0);
	for(size_t ipar=0;ipar< NParams(); ipar++){
	  fcerr[ipar] = sqrt(ffpars.covariance()[ipar][ipar]);
	  bcerr[ipar] = sqrt(bfpars.covariance()[ipar][ipar]);
	  fdp[ipar]->Fill(ffpars.parameters()[ipar]-ftpars.parameters()[ipar]);
	  bdp[ipar]->Fill(bfpars.parameters()[ipar]-btpars.parameters()[ipar]);
	  fpull[ipar]->Fill((ffpars.parameters()[ipar]-ftpars.parameters()[ipar])/fcerr[ipar]);
	  bpull[ipar]->Fill((bfpars.parameters()[ipar]-btpars.parameters()[ipar])/bcerr[ipar]);
	  fiterrh[ipar]->Fill(fcerr[ipar]);
	}
	// accumulate average correlation matrix
	auto const& cov = ffpars.covariance();
	//    auto cormat = cov;
	for(unsigned ipar=0; ipar <NParams();ipar++){
	  for(unsigned jpar=ipar;jpar < NParams(); jpar++){
	    double corr = cov[ipar][jpar]/(fcerr[ipar]*fcerr[jpar]);
	    //	cormat[ipar][jpar] = corr;
	    corravg->Fill(ipar,jpar,fabs(corr));
	  }
	}
	// extract fit parameters and errors
	for(size_t ipar=0;ipar<6;ipar++){
	  ffitpars_.pars_[ipar] = fftraj.params().parameters()[ipar];
	  bfitpars_.pars_[ipar] = bftraj.params().parameters()[ipar];
	  ffiterrs_.pars_[ipar] = sqrt(fftraj.params().covariance()(ipar,ipar));
	  bfiterrs_.pars_[ipar] = sqrt(bftraj.params().covariance()(ipar,ipar));
	}
	ffmom_ = kktrk.fitTraj().momentumMag(ftlow);
	bfmom_ = kktrk.fitTraj().momentumMag(fthigh);
	ffmomerr_ = sqrt(kktrk.fitTraj().momentumVar(ftlow));
	bfmomerr_ = sqrt(kktrk.fitTraj().momentumVar(fthigh));
	fft_ = kktrk.fitTraj().range().begin();
	eft_ = kktrk.fitTraj().range().end();
	fmompull->Fill((ffmom_-ftmom_)/ffmomerr_);
	bmompull->Fill((bfmom_-btmom_)/bfmomerr_);
	// state space parameter difference and errors
//	StateVectorMeasurement tslow = tptraj.state(tlow);
//	StateVectorMeasurement tshigh = tptraj.state(thigh);
//	StateVectorMeasurement slow = kktrk.fitTraj().measurementState(tlow);
//	StateVectorMeasurement shigh = kktrk.fitTraj().measurementState(thigh);

	// test
      } else if(printbad){
	cout << "Bad Fit try " << itry << " status " << kktrk.fitStatus() << endl;
	cout << "True Traj " << tptraj << endl;
	cout << "Seed Traj " << seedtraj << endl;
	kktrk.print(cout,detail);
      }
      if(ttree)ftree->Fill();
    }
    cout << nfail << " Failed fits and " << ndiv << " Diverged fits " << endl;
    hnfail->Fill(nfail);
    hndiv->Fill(ndiv);
    cout <<"Time/fit = " << duration/double(ntries) << " Nanoseconds " << endl;
    // fill canvases
    TCanvas* fdpcan = new TCanvas("fdpcan","fdpcan",800,600);
    fdpcan->Divide(3,2);
    for(size_t ipar=0;ipar<NParams();++ipar){
      fdpcan->cd(ipar+1);
      fdp[ipar]->Fit("gaus","q");
    }
    fdpcan->Write();
    TCanvas* bdpcan = new TCanvas("bdpcan","bdpcan",800,600);
    bdpcan->Divide(3,2);
    for(size_t ipar=0;ipar<NParams();++ipar){
      bdpcan->cd(ipar+1);
      bdp[ipar]->Fit("gaus","q");
    }
    bdpcan->Write();
    TCanvas* fpullcan = new TCanvas("fpullcan","fpullcan",800,600);
    fpullcan->Divide(3,3);
    for(size_t ipar=0;ipar<NParams();++ipar){
      fpullcan->cd(ipar+1);
      fpull[ipar]->Fit("gaus","q");
    }
    fpullcan->cd(NParams()+1);
    fmompull->Fit("gaus","q");
    fpullcan->Write();
    TCanvas* bpullcan = new TCanvas("bpullcan","bpullcan",800,600);
    bpullcan->Divide(3,3);
    for(size_t ipar=0;ipar<NParams();++ipar){
      bpullcan->cd(ipar+1);
      bpull[ipar]->Fit("gaus","q");
    }
    bpullcan->cd(NParams()+1);
    bmompull->Fit("gaus","q");
    bpullcan->Write();
    TCanvas* perrcan = new TCanvas("perrcan","perrcan",800,600);
    perrcan->Divide(3,2);
    for(size_t ipar=0;ipar<NParams();++ipar){
      perrcan->cd(ipar+1);
      fiterrh[ipar]->Draw();
    }
    perrcan->Write();
    TCanvas* corrcan = new TCanvas("corrcan","corrcan",600,600);
    corrcan->Divide(1,1);
    corrcan->cd(1);
    corravg->Scale(1.0/double(ntries));
    corravg->SetStats(0);
    gPad->SetLogz();
    corravg->Draw("colorztext0");
    corrcan->Write();

    TCanvas* statuscan = new TCanvas("statuscan","statuscan",800,600);
    statuscan->Divide(3,2);
    statuscan->cd(1);
    hniter->Draw();
    hnfail->Draw("same");
    hndiv->Draw("same");
    statuscan->cd(2);
    ndof->Draw();
    statuscan->cd(3);
    chisq->Draw();
    statuscan->cd(4);
    chisqndof->Draw();
    statuscan->cd(5);
    chisqprob->Draw();
    statuscan->cd(6);
    logchisqprob->Draw();
    statuscan->Write();
  }
  fitfile.Write();
  fitfile.Close();

  if (kktrk.fitStatus().status_ != KinKal::FitStatus::converged)
  {
    exit(1);
  }

  exit(EXIT_SUCCESS);
}

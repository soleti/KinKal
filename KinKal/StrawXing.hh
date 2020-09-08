#ifndef KinKal_StrawXing_hh
#define KinKal_StrawXing_hh
//
//  Describe the material effects of a kinematic trajectory crossing a straw
//  Used in the kinematic Kalman fit
//
#include "KinKal/DXing.hh"
#include "KinKal/StrawMat.hh"
#include "KinKal/TLine.hh"
#include "KinKal/PTPoca.hh"

namespace KinKal {
  template <class KTRAJ> class StrawXing : public DXing<KTRAJ> {
    public:
      using PKTRAJ = PKTraj<KTRAJ>;
      using DXING = DXing<KTRAJ>;
      using PTPOCA = PTPoca<KTRAJ,TLine>;

      // construct from a trajectory and a time:
      StrawXing(PKTRAJ const& pktraj,double xtime, StrawMat const& smat, TLine const& axis) : DXING(xtime), smat_(smat), axis_(axis) {
	update(pktraj); } 
      // construct from PTPOCA (for use with hits)
      StrawXing(PTPOCA const& tpoca, StrawMat const& smat) : DXING(tpoca.particleToca()) , smat_(smat), axis_(tpoca.sensorTraj()) {
	update(tpoca); }
      virtual ~StrawXing() {}
      // DXing interface
      virtual void update(PKTRAJ const& pktraj) override;
      virtual void update(PKTRAJ const& pktraj, double xtime) override;
      // specific interface: this xing is based on PTPOCA
      void update(PTPOCA const& tpoca);
      virtual void print(std::ostream& ost=std::cout,int detail=0) const override;
      // accessors
      StrawMat const& strawMat() const { return smat_; }
    private:
      StrawMat const& smat_;
      TLine axis_; // straw axis, expressed as a timeline
  };

  template <class KTRAJ> void StrawXing<KTRAJ>::update(PTPOCA const& tpoca) {
    if(tpoca.usable()){
      DXING::mxings_.clear();
      smat_.findXings(tpoca.doca(),sqrt(tpoca.docaVar()),tpoca.dirDot(),DXING::mxings_);
      DXING::xtime_ = tpoca.particleToca();
    } else
      throw std::runtime_error("POCA failure");
  }

  template <class KTRAJ> void StrawXing<KTRAJ>::update(PKTRAJ const& pktraj, double xtime) {
  // update the time to use the current estimate
    DXING::xtime_ = xtime;
    update(pktraj);
  }

  template <class KTRAJ> void StrawXing<KTRAJ>::update(PKTRAJ const& pktraj) {
    // use current xing time create a hint to the POCA calculation: this speeds it up
    TPocaHint tphint(DXING::xtime_, DXING::xtime_);
    PTPOCA tpoca(pktraj,axis_,tphint);
    update(tpoca);
  }

  template <class KTRAJ> void StrawXing<KTRAJ>::print(std::ostream& ost,int detail) const {
    ost <<"Straw Xing time " << this->crossingTime();
    if(detail > 0){
      for(auto const& mxing : this->matXings()){
	ost << " " << mxing.dmat_.name() << " pathLen " << mxing.plen_;
      }
    }
    if(detail > 1){
      ost << " Axis ";
      axis_.print(ost,0);
    }
    ost << std::endl;
  }

}
#endif

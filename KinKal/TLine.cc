#include "KinKal/TLine.hh"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;
using namespace ROOT::Math;

namespace KinKal {
  vector<string> TLine::paramTitles_ = {
    "Transverse DOCA to Z Axis",
    "Azimuth of POCA"
    "Z at POCA",
    "Cos Theta",
    "Time at POCA"}; 
  vector<string> TLine::paramNames_ = {
  "D0","Phi0","Z0","CTheta","Time0"};
  std::vector<std::string> const& TLine::paramNames() { return paramNames_; }
  std::vector<std::string> const& TLine::paramTitles() { return paramTitles_; }
  std::string const& TLine::paramName(ParamIndex index) { return paramNames_[static_cast<size_t>(index)];}
  std::string const& TLine::paramTitle(ParamIndex index) { return paramTitles_[static_cast<size_t>(index)];}

  TLine::TLine(Vec4 const& pos0, Vec3 const& svel, TRange const& range,bool forcerange) : TLine(pos0.Vect(), svel, pos0.T(), range, forcerange) {}
  TLine::TLine(Vec3 const& pos0, Vec3 const& svel, double tmeas, TRange const& range, bool forcerange)  : trange_(range), 
  speed_(sqrt(svel.Mag2())), pos0_(pos0), dir_(svel.Unit()), forcerange_(forcerange) {
    static const Vec3 zdir(0.0,0.0,1.0);
    double zddot = zdir.Dot(dir_);
    param(cost_) = zddot;
    param(d0_) = pos0_.Rho();
    param(phi0_) = atan2(pos0_.Y(),pos0_.X());
    param(z0_) = pos0_.Z();
    param(t0_) = tmeas;
  }

  void TLine::position(Vec4& pos) const {
    Vec3 pos3 = position(pos.T());
    pos.SetXYZT(pos3.X(),pos3.Y(),pos3.Z(),pos.T());
  }

  Vec3 TLine::position(double time) const {
    if(forceRange()) range().forceRange(time);
    return pos0() + ((time-t0())*speed())*dir_;
  }

  Vec3 TLine::velocity(double time) const {
    return dir_*speed();
  }

  double TLine::speed(double time) const {
    return speed();
  }

  double TLine::TOCA(Vec3 point) const {
    double s = (point - pos0()).Dot(dir_);
    return s/speed_ - t0();
  }

  void TLine::print(std::ostream& ost, int detail) const {
    ost << " TLine " <<  range() << " parameters: ";
    for(size_t ipar=0;ipar < TLine::npars_;ipar++){
      ost << TLine::paramName(static_cast<TLine::ParamIndex>(ipar) ) << " : " << param(ipar);
      if(ipar < TLine::npars_-1) ost << " , ";
    }
    ost << endl;
  }

  std::ostream& operator <<(std::ostream& ost, TLine const& tline) {
    tline.print(ost,0);
    return ost;
  }

}

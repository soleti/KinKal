#include "KinKal/IPHelix.hh"
#include "KinKal/BField.hh"
#include "KinKal/BFieldUtils.hh"
#include "Math/AxisAngle.h"
#include <math.h>
#include <stdexcept>

using namespace std;
using namespace ROOT::Math;

namespace KinKal {
  vector<string> IPHelix::paramTitles_ = {
    "Distance of closest approach d_{0}",
    "Angle in the xy plane at closest approach #phi_{0}",
    "xy plane curvature of the track #omega",
    "Distance from the closest approach to the origin z_{0}",
    "Tangent of the track dip angle in the #rho - z projection tan#lambda",
    "Time at Z=0 Plane"};
  vector<string> IPHelix::paramNames_ = {
  "d_{0}","#phi_{0}","#omega","z_{0}","tan#lambda","t_{0}"};
  vector<string> IPHelix::paramUnits_ = {
      "mm", "rad", "rad", "mm", "", "ns"};
  string IPHelix::trajName_("IPHelix");  
  std::vector<std::string> const& IPHelix::paramNames() { return paramNames_; }
  std::vector<std::string> const& IPHelix::paramUnits() { return paramUnits_; }
  std::vector<std::string> const& IPHelix::paramTitles() { return paramTitles_; }
  std::string const& IPHelix::paramName(ParamIndex index) { return paramNames_[static_cast<size_t>(index)];}
  std::string const& IPHelix::paramUnit(ParamIndex index) { return paramUnits_[static_cast<size_t>(index)]; }
  std::string const& IPHelix::paramTitle(ParamIndex index) { return paramTitles_[static_cast<size_t>(index)];}
  string const& IPHelix::trajName() { return trajName_; }

  IPHelix::IPHelix( Vec4 const& pos0, Mom4 const& mom0, int charge, double bnom, TRange const& range) : IPHelix(pos0,mom0,charge,Vec3(0.0,0.0,bnom),range) {}
  IPHelix::IPHelix(Vec4 const &pos0, Mom4 const &mom0, int charge, Vec3 const &bnom,
                 TRange const &trange) : trange_(trange), mass_(mom0.M()), charge_(charge), bnom_(bnom)
  {
    // Transform into the system where Z is along the Bfield.  This is a pure rotation about the origin
    Vec4 pos(pos0);
    Mom4 mom(mom0);
    g2l_ = Rotation3D(AxisAngle(Vec3(sin(bnom_.Phi()),-cos(bnom_.Phi()),0.0),bnom_.Theta()));
    if(fabs(g2l_(bnom_).Theta()) > 1.0e-6)throw invalid_argument("Rotation Error");
    pos = g2l_(pos);
    mom = g2l_(mom);
    // create inverse rotation; this moves back into the original coordinate system
    l2g_ = g2l_.Inverse();
    double momToRad = 1.0/(BFieldUtils::cbar()*charge_*bnom_.R());
    mbar_ = -mass_ * momToRad;

    double pt = sqrt(mom.perp2());
    double radius = fabs(pt*momToRad);

    double lambda = -mom.z()*momToRad;
    double amsign = copysign(1.0, mbar_);

    Vec3 center = Vec3(pos.x() + mom.y()*momToRad, pos.y() - mom.x()*momToRad, 0.0);
    double rcent = sqrt(center.perp2());
    double fcent = center.phi();
    double centerx = rcent*cos(fcent);
    double centery = rcent*sin(fcent);

    param(omega_) = amsign/radius;
    param(tanDip_) = amsign*lambda/radius;
    param(d0_) = amsign*(rcent - radius);
    param(phi0_) = atan2(-amsign * centerx, amsign * centery);

    Vec3 pos3 = Vec3(pos.x(), pos.y(), pos.z());
    double fz0 = (pos3 - center).phi() - pos3.z() / lambda;
    deltaPhi(fz0);
    double refphi = fz0+amsign*M_PI_2;
    double phival = phi0();
    double dphi = deltaPhi(phival, refphi);
    param(z0_) = dphi * tanDip() / omega();

    param(t0_) = pos.T() - (pos.Z() - param(z0_)) / (sinDip() * CLHEP::c_light * beta());
    vt_ = CLHEP::c_light * pt / mom.E();
    vz_ = CLHEP::c_light * mom.z() / mom.E();
    // test position and momentum function
    Vec4 testpos(pos0);
    // std::cout << "Testpos " << testpos << std::endl;
    position(testpos);
    Mom4 testmom = momentum(testpos.T());
    auto dp = testpos.Vect() - pos0.Vect();
    auto dm = testmom.Vect() - mom0.Vect();
    if(dp.R() > 1.0e-5 || dm.R() > 1.0e-5)throw invalid_argument("Rotation Error");
  }

  double IPHelix::deltaPhi(double &phi, double refphi) const
  {
    double dphi = phi - refphi;
    static const double twopi = 2 * M_PI;
    while (dphi > M_PI)
    {
      dphi -= twopi;
      phi -= twopi;
    }
    while (dphi <= -M_PI)
    {
      dphi += twopi;
      phi += twopi;
    }
    return dphi;
  }

  IPHelix::IPHelix(IPHelix const& other, Vec3 const& bnom, double trot) : IPHelix(other) {
    pars_.parameters() += other.dPardB(trot,bnom);
    g2l_ = Rotation3D(AxisAngle(Vec3(sin(bnom_.Phi()),-cos(bnom_.Phi()),0.0),bnom_.Theta()));
    l2g_ = g2l_.Inverse();
  }

  IPHelix::IPHelix(PDATA const &pdata, IPHelix const& other) : IPHelix(other) {
    pars_ = pdata;
  }

  IPHelix::IPHelix(StateVector const& pstate, double time, double mass, int charge, Vec3 const& bnom, TRange const& range) :
    IPHelix(Vec4(pstate.position().X(),pstate.position().Y(),pstate.position().Z(),time),
	Mom4(pstate.momentum().X(),pstate.momentum().Y(),pstate.momentum().Z(),mass),
	charge,bnom,range) 
  {}

  IPHelix::IPHelix(StateVectorMeasurement const& pstate, double time, double mass, int charge, Vec3 const& bnom, TRange const& range) :
  IPHelix(pstate.stateVector(),time,mass,charge,bnom,range) {
  // derive the parameter space covariance from the global state space covariance
    DPDS dpds = dPardState(time);
    pars_.covariance() = ROOT::Math::Similarity(dpds,pstate.stateCovariance());
  }

  void IPHelix::position(Vec4 &pos) const
 {
    Vec3 pos3 = position(pos.T());
    pos.SetPx(pos3.X());
    pos.SetPy(pos3.Y());
    pos.SetPz(pos3.Z());
  }

  Vec4 IPHelix::pos4(double time) const
 {
    Vec3 pos3 = position(time);
    return Vec4( pos3.X(), pos3.Y(), pos3.Z(), time);
  }

  Vec3 IPHelix::position(double time) const
  {
    double cDip = cosDip();
    double phi00 = phi0();
    double l = CLHEP::c_light * beta() * (time - t0()) * cDip;
    double ang = phi00 + l * omega();
    double cang = cos(ang);
    double sang = sin(ang);
    double sphi0 = sin(phi00);
    double cphi0 = cos(phi00);

    return l2g_(Vec3((sang - sphi0) / omega() - d0() * sphi0, -(cang - cphi0) / omega() + d0() * cphi0, z0() + l * tanDip()));
  }

  Mom4 IPHelix::momentum(double time) const
  {

    Vec3 dir = direction(time);
    double bgm = betaGamma()*mass_;
    return Mom4(bgm*dir.X(), bgm*dir.Y(), bgm*dir.Z(), mass_);
  }

  double IPHelix::angle(const double &f) const
  {
    return phi0() + arc(f);
  }

  Vec3 IPHelix::velocity(double time) const
  {
    Mom4 mom = momentum(time);
    return mom.Vect() * (CLHEP::c_light * fabs(Q() / ebar()));
  }

  Vec3 IPHelix::direction(double time,LocalBasis::LocDir mdir) const
  {
    double cosval = cosDip();
    double sinval = sinDip();
    double phival = phi(time);

    switch ( mdir ) {
      case LocalBasis::perpdir:
        return l2g_(Vec3(-sinval * cos(phival), -sinval * sin(phival), cosval));
      case LocalBasis::phidir:
        return l2g_(Vec3(-sin(phival), cos(phival), 0.0));
      case LocalBasis::momdir:
        return l2g_(Vec3(Q() / omega() * cos(phival),
                         Q() / omega() * sin(phival),
                         Q() / omega() * tanDip()).Unit());
      default:
        throw std::invalid_argument("Invalid direction");
    }
  }

  IPHelix::DPDV LHelix::dPardMLoc(double time) const {
    // euclidean space is column, parameter space is row
    double omval = omega();
    double dt = time-t0();
    double dphi = omval*dt;
    double phival = dphi + phi0();
    double sphi = sin(phival);
    double cphi = cos(phival);
    double inve2 = 1.0/ebar2();
    SVec3 T2(-sphi,cphi,0.0);
    SVec3 T3(cphi,sphi,0.0);
    SVec3 zdir(0.0,0.0,1.0);

    SVec3 mdir = rad()*T3 + lam()*zdir;
    SVec3 dR_dM = T3;
    SVec3 dL_dM = zdir;
    SVec3 dCx_dM (0.0,-1.0,0.0);
    SVec3 dCy_dM (1.0,0.0,0.0);
    SVec3 dphi0_dM = T2/rad() + (dphi/lam())*zdir;
    SVec3 dt0_dM = -dt*(inve2*mdir - zdir/lam());
    SVec3 domega_DM(-)
    IPHelix::DPDV dPdM;
    dPdM.Place_in_row(dd0_dM,d0_,0);
    dPdM.Place_in_row(dphi0_dM,phi0_,0);
    dPdM.Place_in_row(domega_DM,omega_,0);
    dPdM.Place_in_row(dz0_dM,z0_,0);
    dPdM.Place_in_row(dtanDip_dM,tanDip_,0);
    dPdM.Place_in_row(dt0_dM,t0_,0);
    dPdM *= 1.0/Q();
    return dPdM;
  }

  // derivatives of momentum projected along the given basis WRT the 6 parameters, and the physical direction associated with that
  IPHelix::DVEC LHelix::momDeriv(double time, LocalBasis::LocDir mdir) const {
    typedef ROOT::Math::SVector<double,3> SVec3;
    DPDV dPdM = dPardM(time);
    auto dir = direction(time,mdir);
    double mommag = momentumMag(time);
    return mommag*(dPdM*SVec3(dir.X(), dir.Y(), dir.Z()));
  }

  std::ostream& operator <<(std::ostream& ost, IPHelix const& hhel) {
    ost << " IPHelix parameters: ";
    for(size_t ipar=0;ipar < IPHelix::npars_;ipar++){
      ost << IPHelix::paramName(static_cast<IPHelix::ParamIndex>(ipar) ) << " : " << hhel.paramVal(ipar);
      if(ipar < IPHelix::npars_-1) ost << " , ";
    }
    return ost;
  }

} // KinKal namespace

#ifndef KinKal_IPHelix_hh
#define KinKal_IPHelix_hh
//
// class desribing the looping helix basis for the kinematic Kalman fit
// It provides geometric, kinematic, and algebraic representation of
// a particule executing a multi-loop helix in a constant magnetic field.
// Original Author Roberto Soleti (LBNL) 1/2020
//

#include "KinKal/Vectors.hh"
#include "KinKal/TRange.hh"
#include "KinKal/PData.hh"
#include "KinKal/StateVector.hh"
#include "KinKal/LocalBasis.hh"
#include "KinKal/BField.hh"
#include "CLHEP/Units/PhysicalConstants.h"
#include "Math/Rotation3D.h"
#include <vector>
#include <string>
#include <ostream>

namespace KinKal {

  class IPHelix {
    public:
      // This class must provide the following to be used to instantiate the 
      // classes implementing the Kalman fit
      // define the indices and names of the parameters
      enum ParamIndex {d0_=0,phi0_=1,omega_=2,z0_=3,tanDip_=4,t0_=5,npars_=6};
      constexpr static ParamIndex t0Index() { return t0_; }
      constexpr static size_t NParams() { return npars_; }
      typedef PData<npars_> PDATA; // Data payload for this class
      typedef typename PDATA::DVEC DVEC; // derivative of parameters type
      typedef ROOT::Math::SMatrix<double,npars_,3,ROOT::Math::MatRepStd<double,npars_,3> > DPDV; // parameter derivatives WRT space dimension type
      typedef ROOT::Math::SMatrix<double,3,npars_,ROOT::Math::MatRepStd<double,3,npars_> > DVDP; // space dimension derivatives WRT parameter type
      static std::vector<std::string> const &paramNames();
      static std::vector<std::string> const &paramUnits();
      static std::vector<std::string> const& paramTitles();
      static std::string const& paramName(ParamIndex index);
      static std::string const& paramUnit(ParamIndex index);
      static std::string const& paramTitle(ParamIndex index);
      static std::string const& trajName();

      // interface needed for KKTrk instantiation
      // construct from momentum, position, and particle properties.
      // This also requires the nominal BField, which can be a vector (3d) or a scalar (B along z)
      IPHelix(Vec4 const& pos, Mom4 const& mom, int charge, Vec3 const& bnom, TRange const& range=TRange());
      IPHelix(Vec4 const& pos, Mom4 const& mom, int charge, double bnom, TRange const& range=TRange());
      // copy payload and adjust for a different BField and range 
      IPHelix(IPHelix const& other, Vec3 const& bnom, double trot);
      // copy and override parameters
      IPHelix(PDATA const &pdata, IPHelix const& other); 
      // construct from the particle state at a given time, plus mass and charge
      IPHelix(StateVector const& pstate, double time, double mass, int charge, Vec3 const& bnom, TRange const& range=TRange()); // TODO
      // same, including covariance information
      IPHelix(StateVectorMeasurement const& pstate, double time, double mass, int charge, Vec3 const& bnom, TRange const& range=TRange()); //TODO
      // particle position and momentum as a function of time
      void position(Vec4& pos) const; // time is input
      Vec4 pos4(double time) const;
      Vec3 position(double time) const; // time is input
      Mom4 momentum(double time) const;
      Vec3 velocity(double time) const;
      Vec3 direction(double time, LocalBasis::LocDir mdir= LocalBasis::momdir) const;
      // scalar momentum and energy in MeV/c units
      double momentumMag(double time) const  { return mass_ * pbar() / mbar_; }
      double momentumVar(double time) const  { return -1.0; }//TODO
      double energy(double time) const  { return mass_ * ebar() / mbar_; }
      // speed in mm/ns
      double speed(double time) const  { return CLHEP::c_light * beta(); }
      // local momentum direction basis
      void print(std::ostream& ost, int detail) const  {} // TODO
      TRange const& range() const { return trange_; }
      TRange& range() { return trange_; }
      void setRange(TRange const& trange) { trange_ = trange; }
      void setBNom(double time, Vec3 const& bnom) {} // TODO
      bool inRange(double time) const { return trange_.inRange(time); }

      // momentum change derivatives; this is required to instantiate a KalTrk using this KTraj
      DVEC momDeriv(double time, LocalBasis::LocDir mdir) const;
      double mass() const { return mass_;} // mass 
      int charge() const { return charge_;} // charge in proton charge units

      // named parameter accessors
      double paramVal(size_t index) const { return pars_.parameters()[index]; }
      PDATA const &params() const { return pars_; }
      PDATA &params() { return pars_; }
      double d0() const { return paramVal(d0_); }
      double phi0() const { return paramVal(phi0_); }
      double omega() const { return paramVal(omega_); } // rotational velocity, sign set by magnetic force
      double z0() const { return paramVal(z0_); }
      double tanDip() const { return paramVal(tanDip_); }
      double t0() const { return paramVal(t0_); }
      // express fit results as a state vector (global coordinates)
      StateVector state(double time) const { return StateVector(); } // TODO
      StateVectorMeasurement measurementState(double time) const { return StateVectorMeasurement(); } // TODO

      // simple functions
      double sign() const { return copysign(1.0,mbar_); } // combined bending sign including Bz and charge
      double pbar() const { return 1./ omega() * sqrt( 1 + tanDip() * tanDip() ); } // momentum in mm
      double ebar() const { return sqrt(pbar()*pbar() + mbar_ * mbar_); } // energy in mm
      double cosDip() const { return 1./sqrt(1.+ tanDip() * tanDip() ); }
      double sinDip() const { return tanDip()*cosDip(); }
      double mbar() const { return mbar_; } // mass in mm; includes charge information!
      double vt() const { return vt_; }
      double vz() const { return vz_; }
      double Q() const { return mass_/mbar_; } // reduced charge
      double beta() const { return fabs(pbar()/ebar()); } // relativistic beta
      double gamma() const { return fabs(ebar()/mbar_); } // relativistic gamma
      double betaGamma() const { return fabs(pbar()/mbar_); } // relativistic betagamma
      double dphi(double t) const { return omega()*vt()*(t - t0()); }
      double phi(double t) const { return dphi(t) + phi0(); }
      double deltaPhi(double &phi, double refphi=0.) const;
      double angle(const double &f) const;
      double translen(const double &f) const { return cosDip() * f; }
      double arc(const double &f) const { return translen(f) * omega(); }
      double ztime(double zpos) const { return t0() + zpos / vz(); }
      Vec3 const &bnom(double time=0.0) const { return bnom_; }
      double bnomR() const { return bnom_.R(); }
      DPDV dPardX(double time) const { return DPDV(); } // TODO
      DPDV dPardM(double time) const { return DPDV(); } // TODO
      DVDP dXdPar(double time) const { return DVDP(); } // TODO
      DVDP dMdPar(double time) const { return DVDP(); } // TODO
      DSDP dPardState(double time) const { return DPDS(); } // TODO
      DPDS dStatedPar(double time) const { return DSDP(); } // TODO
      // package the above for full (global) state
      // Parameter derivatives given a change in BField
      DVEC dPardB(double time) const { return DVEC(); } // TODO
      DVEC dPardB(double time, Vec3 const& BPrime) const { return DVEC(); } //TODO 
 
      // flip the helix in time and charge; it remains unchanged geometrically
      void invertCT()
      {
        mbar_ *= -1.0;
        charge_ *= -1;
        pars_.parameters()[t0_] *= -1.0;
      }
      //
    private :
      // local coordinate system functions, used internally
      Vec3 localDirection(double time, LocalBasis::LocDir mdir= LocalBasis::momdir) const;
      Vec3 localMomentum(double time) const;
      Vec3 localPosition(double time) const;
      DPDV dPardXLoc(double time) const; // return the derivative of the parameters WRT the local (unrotated) position vector
      DPDV dPardMLoc(double time) const; // return the derivative of the parameters WRT the local (unrotated) momentum vector
      DSDP dPardStateLoc(double time) const; // derivative of parameters WRT local state
      TRange trange_;
      PDATA pars_; // parameters
      double mass_;  // in units of MeV/c^2
      int charge_; // charge in units of proton charge
      double mbar_;  // reduced mass in units of mm, computed from the mass and nominal field
      Vec3 bnom_;    // nominal BField
      ROOT::Math::Rotation3D l2g_, g2l_; // rotations between local and global coordinates
      static std::vector<std::string> paramTitles_;
      static std::vector<std::string> paramNames_;
      static std::vector<std::string> paramUnits_;
      static std::string trajName_;
      double vt_; // transverse velocity
      double vz_; // z velocity
      // non-const accessors
      double &param(size_t index) { return pars_.parameters()[index]; }
  };
  std::ostream& operator <<(std::ostream& ost, IPHelix const& hhel);
}
#endif

#ifndef KinKal_BFieldUtils_hh
#define KinKal_BFieldUtils_hh
//
//  Utility functions for working with BField and KTRAJ objects.  Used in the
//  Kinematic Kalman filter fit
//
#include "KinKal/TRange.hh"
#include "KinKal/Vectors.hh"
#include "KinKal/BField.hh"
#include <algorithm>
#include <cmath>
#include <iostream>
namespace KinKal {
  namespace BFieldUtils {
      // speed of light in units to convert Tesla to mm (bending radius)
      static double constexpr cbar() { return CLHEP::c_light/1000.0; }
      // integrate the residual magentic force over the given KTRAJ and range, NOT described by the intrinsic bending, due to the DIFFERENCE
      // between the magnetic field and the nominal field used by the KTRAJ.  Returns the change in momentum
      // = integral of the 'external' force needed to keep the particle onto this trajectory over the specified range;
      template <class KTRAJ> Vec3 integrate(BField const& bfield, KTRAJ const& ktraj, TRange const& range);
      // estimate how long in time from the given start time the trajectory position will stay within the given tolerance
      // compared to the true particle motion, given the true magnetic field.  This measures the impact of the KTRAJ nominal field being
      // different from the true field
      template <class KTRAJ> double rangeInTolerance(double tstart, BField const& bfield, KTRAJ const& ktraj, double tol);

  }

  template<class KTRAJ> Vec3 BFieldUtils::integrate(BField const& bfield, KTRAJ const& ktraj, TRange const& trange) {
    // take a fixed number of steps.  This may fail for long ranges FIXME!
    unsigned nsteps(10);
    double dt = trange.range()/nsteps;
    // now integrate
    Vec3 dmom;
    for(unsigned istep=0; istep< nsteps; istep++){
      double tstep = trange.low() + istep*dt;
      Vec3 vel = ktraj.velocity(tstep);
      Vec3 db = bfield.fieldVect(ktraj.position(tstep)) - ktraj.bnom(tstep);
      dmom += cbar()*ktraj.charge()*dt*vel.Cross(db);
    }
    return dmom;
  }

  template<class KTRAJ> double BFieldUtils::rangeInTolerance(double tstart, BField const& bfield, KTRAJ const& ktraj, double tol) {
    // compute scaling factor
    double spd = ktraj.speed(tstart);
    double sfac = fabs(cbar()*ktraj.charge()*spd*spd/ktraj.momentumMag(tstart));
    // estimate step size from initial BField difference
    Vec3 tpos = ktraj.position(tstart);
    Vec3 bvec = bfield.fieldVect(tpos);
    auto db = (bvec - ktraj.bnom(tstart)).R();
    // estimate the step size for testing the position deviation.  This comes from 2 components:
    // the (static) difference in field, and the change in field along the trajectory
    double tstep(0.1); // nominal step
    // step increment from static difference from nominal field.  0.2 comes from sagitta geometry
    // protect against nominal field = exact field
    if(db > 1e-4) tstep = std::min(tstep,0.2*sqrt(tol/(sfac*db))); 
    Vec3 dBdt = bfield.fieldDeriv(tpos,ktraj.velocity(tstart));
    // the deviation goes as the cube root of the BField change.  0.5 comes from cosine expansion
    tstep = std::min(tstep, 0.5*std::cbrt(tol/(sfac*dBdt.R()))); //
    //
    // loop over the trajectory in fixed steps to compute integrals and domains.
    // step size is defined by momentum direction tolerance.
    double tend = tstart;
    double dx(0.0);
    // advance till spatial distortion exceeds position tolerance or we reach the range limit
    do{
      // increment the range
      tend += tstep;
      tpos = ktraj.position(tend);
      bvec = bfield.fieldVect(tpos);
      // BField diff with nominal
      auto db = (bvec - ktraj.bnom(tend)).R();
      // spatial distortion accumulation; this goes as the square of the time times the field difference
      dx += sfac*(tend-tstart)*tstep*db;
    } while(fabs(dx) < tol && tend < ktraj.range().high());
    //    std::cout << "tstep " << tstep << " trange " << drange.range() << std::endl;
    return tend;
  }

}
#endif

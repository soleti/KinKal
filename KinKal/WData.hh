#ifndef KinKal_WData_hh
#define KinKal_WData_hh
//
//  Data object describing weight-space information
//  used as part of the kinematic kalman fit
//
#include "KinKal/TData.hh"
#include <ostream>
namespace KinKal {
  class PData;
  class WData {
    public:
      // construct from vector and matrix
      WData(DVEC const& wvec, DMAT const& wmat) : tdata_(wvec,wmat) {}
      WData(DVEC const& wvec) : tdata_(wvec) {}
      WData(PData const& pdata);
      WData() {}
      // accessors; just re-interpret the base class accessors
      DVEC const& weightVec() const { return tdata_.vec(); }
      DMAT const& weightMat() const { return tdata_.mat(); }
      DVEC& weightVec() { return tdata_.vec(); }
      DMAT& weightMat() { return tdata_.mat(); }
      TData const& tData() const { return tdata_; }
      TData& tData() { return tdata_; }
      // addition: only works for other weights
      WData & operator +=(WData const& other) {
	tdata_ += other.tdata_;
	return *this;
      }
      WData & operator -=(WData const& other) {
	tdata_ -= other.tdata_;
	return *this;
      }
      void print(std::ostream& ost=std::cout,int detail=0) const {
	ost << "WData wVec " << weightVec() << std::endl;
	if(detail > 1)
	  ost << "weight " << weightMat() << std::endl;
      }
    private:
      TData tdata_; // data payload
  };
  std::ostream& operator << (std::ostream& ost, WData const& wdata);
}
#endif

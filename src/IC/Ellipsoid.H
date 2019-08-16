#ifndef IC_ELLIPSOID_H_
#define IC_ELLIPSOID_H_

#include "IC/IC.H"
#include "Util/Util.H"

/// \class Ellipsoid
/// \brief Initialize an ellipsoidal inclusion
namespace IC
{
class Ellipsoid : public IC
{
public:
	Ellipsoid (amrex::Vector<amrex::Geometry> &_geom) :
		IC(_geom)
	{
		amrex::ParmParse pp("ic");
		pp.queryarr("center",center);
		pp.queryarr("radius",radius);
		pp.query("dim",dim);
	}
  
	void Add(const int lev,
			amrex::Vector<amrex::MultiFab * > &a_field)
	{
		bool cellcentered = (a_field[0]->boxArray().ixType() == amrex::IndexType(amrex::IntVect::TheCellVector()));

		for (amrex::MFIter mfi(*a_field[lev],amrex::TilingIfNotGPU()); mfi.isValid(); ++mfi)
		{
			amrex::Box bx = mfi.tilebox();
			bx.grow(a_field[lev]->nGrow());

			amrex::Array4<Set::Scalar> const& field = a_field[lev]->array(mfi);
			amrex::ParallelFor (bx,[=] AMREX_GPU_DEVICE(int i, int j, int k) {

					Set::Scalar AMREX_D_DECL(x,y,z);
					if (cellcentered)
					{
						AMREX_D_TERM(x = geom[lev].ProbLo()[0] + ((amrex::Real)(i) + 0.5) * geom[lev].CellSize()[0];,
									 y = geom[lev].ProbLo()[1] + ((amrex::Real)(j) + 0.5) * geom[lev].CellSize()[1];,
									 z = geom[lev].ProbLo()[2] + ((amrex::Real)(k) + 0.5) * geom[lev].CellSize()[2];);
					}
					else
					{
						AMREX_D_TERM(x = geom[lev].ProbLo()[0] + (amrex::Real)(i) * geom[lev].CellSize()[0];,
									 y = geom[lev].ProbLo()[1] + (amrex::Real)(j) * geom[lev].CellSize()[1];,
									 z = geom[lev].ProbLo()[2] + (amrex::Real)(k) * geom[lev].CellSize()[2];);
					}
					Set::Scalar rsq = 0.0;

					AMREX_D_TERM(if (dim >= 1) rsq += (x-center[0])*(x-center[0])/(radius[0]*radius[0]);,
								 if (dim >= 2) rsq += (y-center[1])*(y-center[1])/(radius[1]*radius[1]);,
								 if (dim >= 3) rsq += (z-center[2])*(z-center[2])/(radius[2]*radius[2]););
					if ( rsq < 1.0)
						field(i,j,k,0) = 0.0;     
					else
						field(i,j,k,0) = 1.;
				});
		}

	};
  
private:
	int dim = AMREX_SPACEDIM;
	amrex::Vector<amrex::Real> center;
	amrex::Vector<amrex::Real> radius;
};
}
#endif
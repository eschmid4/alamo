
#include <AMReX_ParallelDescriptor.H>
#include <AMReX_ParmParse.H>
#include <AMReX_MultiFabUtil.H>
#include <AMReX_FillPatchUtil.H>

#include <PFAmr.H>
#include <PFAmrBC.H>

using namespace amrex;

PFAmr::PFAmr ()
{
  ReadParameters();

  // Geometry on all levels has been defined already.

  // No valid BoxArray and DistributionMapping have been defined.
  // But the arrays for them have been resized.

  int nlevs_max = maxLevel() + 1;

  istep.resize(nlevs_max, 0);
  nsubsteps.resize(nlevs_max, 1);
  for (int lev = 1; lev <= maxLevel(); ++lev) {
    nsubsteps[lev] = MaxRefRatio(lev-1);
  }

  t_new.resize(nlevs_max, 0.0);
  t_old.resize(nlevs_max, -1.e100);
  dt.resize(nlevs_max, 1.e100);

  phi_new.resize(number_of_grains); // <-- resize for number of order parameters
  phi_old.resize(number_of_grains); //

  for (int n = 0; n < number_of_grains; n++)
    {
      phi_new[n].resize(nlevs_max);
      phi_old[n].resize(nlevs_max);
    }
}

PFAmr::~PFAmr ()
{
  
}

void
PFAmr::ReadParameters ()
{
  {
    ParmParse pp;  // Traditionally, max_step and stop_time do not have prefix.
    pp.query("max_step", max_step);
    pp.query("stop_time", stop_time);
    pp.query("timestep",timestep);
  }

  {
    ParmParse pp("amr"); // Traditionally, these have prefix, amr.

    pp.query("regrid_int", regrid_int);

    pp.query("check_file", check_file);
    pp.query("check_int", check_int);

    pp.query("plot_file", plot_file);
    pp.query("plot_int", plot_int);

    pp.query("restart", restart_chkfile);
  }
}

void
PFAmr::MakeNewLevelFromCoarse (int lev, Real time, const BoxArray& ba,
				const DistributionMapping& dm)
{
  for (int n = 0; n < number_of_grains; n++)
    {
      const int ncomp = phi_new[n][lev-1]->nComp();
      const int nghost = phi_new[n][lev-1]->nGrow();
    
      phi_new[n][lev].reset(new MultiFab(ba, dm, ncomp, nghost));
      phi_old[n][lev].reset(new MultiFab(ba, dm, ncomp, nghost));

      t_new[lev] = time;
      t_old[lev] = time - 1.e200;

      FillCoarsePatch(lev, time, *phi_new[n][lev], 0, ncomp);
    }
}


void
PFAmr::RemakeLevel (int lev, Real time, const BoxArray& ba,
		     const DistributionMapping& dm)
{
  amrex::Array<std::unique_ptr<MultiFab> > new_state(number_of_grains); 
  amrex::Array<std::unique_ptr<MultiFab> > old_state(number_of_grains);
  for (int n=0; n < number_of_grains; n++)
    {
      const int ncomp = phi_new[n][lev]->nComp();
      const int nghost = phi_new[n][lev]->nGrow();

      new_state[n].reset(new MultiFab(ba, dm, ncomp, nghost));
      old_state[n].reset(new MultiFab(ba, dm, ncomp, nghost));
      //std::unique_ptr<MultiFab> new_state(new MultiFab(ba, dm, ncomp, nghost));
      //std::unique_ptr<MultiFab> old_state(new MultiFab(ba, dm, ncomp, nghost));
    }

  FillPatch(lev, time, new_state, 0);

  for (int n=0; n < number_of_grains; n++)
    {
      std::swap(new_state[n], phi_new[n][lev]);
      std::swap(old_state[n], phi_old[n][lev]);

      t_new[lev] = time;
      t_old[lev] = time - 1.e200;
    }
}

void
PFAmr::ClearLevel (int lev)
{
  for (int n = 0; n < number_of_grains; n++)
    {
      phi_new[n][lev].reset(nullptr);
      phi_old[n][lev].reset(nullptr);
    }
}

long
PFAmr::CountCells (int lev)
{
  const int N = grids[lev].size();

  long cnt = 0;

  for (int i = 0; i < N; ++i)
    {
      cnt += grids[lev][i].numPts();
    }

  return cnt;
}

void
PFAmr::FillPatch (int lev, Real time, Array<std::unique_ptr<MultiFab> >& mf, int icomp)
{
  if (lev == 0)
    {
      Array<Array<MultiFab*> > smf;
      smf.resize(number_of_grains);
      Array<Real> stime;
      GetData(0, time, smf, stime);

      PFAmrPhysBC physbc;
      for (int n = 0; n<number_of_grains; n++)
	amrex::FillPatchSingleLevel(*mf[n], time, smf[n], stime, 0, icomp, mf[n]->nComp(),
				    geom[lev], physbc);
    }
  else
    {
      Array<Array<MultiFab*> > cmf, fmf;
      cmf.resize(number_of_grains);
      fmf.resize(number_of_grains);
      Array<Real> ctime, ftime;
      GetData(lev-1, time, cmf, ctime);
      GetData(lev  , time, fmf, ftime);

      PFAmrPhysBC cphysbc, fphysbc;
      Interpolater* mapper = &cell_cons_interp;

      int lo_bc[] = {INT_DIR, INT_DIR, INT_DIR}; // periodic boundaries
      int hi_bc[] = {INT_DIR, INT_DIR, INT_DIR};
      Array<BCRec> bcs(1, BCRec(lo_bc, hi_bc));

      for (int n = 0; n<number_of_grains; n++)
	amrex::FillPatchTwoLevels(*mf[n], time, cmf[n], ctime, fmf[n], ftime,
				  0, icomp, mf[n]->nComp(), geom[lev-1], geom[lev],
				  cphysbc, fphysbc, refRatio(lev-1),
				  mapper, bcs);
    }
}

void
PFAmr::FillCoarsePatch (int lev, Real time, MultiFab& mf, int icomp, int ncomp)
{
  BL_ASSERT(lev > 0);

  Array<Array<MultiFab*> > cmf;
  Array<Real> ctime;
  GetData(lev-1, time, cmf, ctime);
    
  for (int n = 0; n < number_of_grains; n++)
    if (cmf.size() != 1) 
      amrex::Abort("FillCoarsePatch: how did this happen?");


  PFAmrPhysBC cphysbc, fphysbc;
  Interpolater* mapper = &cell_cons_interp;
    
  int lo_bc[] = {INT_DIR, INT_DIR, INT_DIR}; // periodic boundaries
  int hi_bc[] = {INT_DIR, INT_DIR, INT_DIR};
  Array<BCRec> bcs(1, BCRec(lo_bc, hi_bc));

  for (int n = 0; n < number_of_grains; n++)
    amrex::InterpFromCoarseLevel(mf, time, *cmf[n][0], 0, icomp, ncomp, geom[lev-1], geom[lev],
				 cphysbc, fphysbc, refRatio(lev-1),
				 mapper, bcs);
}

void
PFAmr::GetData (int lev, Real time, Array<Array<MultiFab*> >& data, Array<Real>& datatime)
{
  for (int n = 0; n < number_of_grains; n++) data[n].clear();
  datatime.clear();

  const Real teps = (t_new[lev] - t_old[lev]) * 1.e-3;

  if (time > t_new[lev] - teps && time < t_new[lev] + teps)
    {
      for (int n = 0; n < number_of_grains; n++) data[n].push_back(phi_new[n][lev].get());
      datatime.push_back(t_new[lev]);
    }
  else if (time > t_old[lev] - teps && time < t_old[lev] + teps)
    {
      for (int n = 0; n < number_of_grains; n++) data[n].push_back(phi_old[n][lev].get());
      datatime.push_back(t_old[lev]);
    }
  else
    {
      for (int n = 0; n < number_of_grains; n++)
	{
	  data[n].push_back(phi_old[n][lev].get());
	  data[n].push_back(phi_new[n][lev].get());
	}
      datatime.push_back(t_old[lev]);
      datatime.push_back(t_new[lev]);
    }
}


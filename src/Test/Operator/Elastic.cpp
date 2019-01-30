#include <streambuf>
#include <map>

#include <AMReX.H>
#include <AMReX_ParmParse.H>
#include <AMReX_MLMG.H>
#include <AMReX_MLCGSolver.H>
#include <AMReX_MultiFabUtil.H>
#include <AMReX_PlotFileUtil.H>
#include <AMReX_MLNodeLaplacian.H>


#include <AMReX.H>
#include <AMReX_Vector.H>
#include <AMReX_Geometry.H>
#include <AMReX_BoxArray.H>
#include <AMReX_DistributionMapping.H>
#include <AMReX_MultiFab.H>
#include <AMReX_MLMG.H>
#include <AMReX_MLCGSolver.H>

#include "Elastic.H"
#include "Set/Set.H"
#include "IC/Trig.H"
#include "IC/Affine.H"
#include "Operator/Elastic.H"
#include "Model/Solid/LinearElastic/Laplacian.H"

namespace Test
{
namespace Operator
{
void Elastic::Define(int _ncells,
		     int _nlevels)
{

	ncells = _ncells;
 	nlevels = _nlevels;
	int max_grid_size = ncells/2;
	//std::string orientation = "h";
 	geom.resize(nlevels);
 	cgrids.resize(nlevels);
 	ngrids.resize(nlevels);
 	dmap.resize(nlevels);

 	solution_exact.resize(nlevels);
 	solution_numeric.resize(nlevels);
 	solution_error.resize(nlevels);
 	rhs_prescribed.resize(nlevels);
 	rhs_numeric.resize(nlevels);
 	rhs_exact.resize(nlevels);
 	res_numeric.resize(nlevels);
 	res_exact.resize(nlevels);
	ghost_force.resize(nlevels);

	amrex::RealBox rb({AMREX_D_DECL(0.,0.,0.)}, {AMREX_D_DECL(1.,1.,1.)});
	amrex::Geometry::Setup(&rb, 0);

	amrex::Box NDomain(amrex::IntVect{AMREX_D_DECL(0,0,0)},
			   amrex::IntVect{AMREX_D_DECL(ncells,ncells,ncells)},
			   amrex::IntVect::TheNodeVector());
	amrex::Box CDomain = amrex::convert(NDomain, amrex::IntVect::TheCellVector());

	amrex::Box domain = CDomain;
 	for (int ilev = 0; ilev < nlevels; ++ilev)
 		{
 			geom[ilev].define(domain);
 			domain.refine(ref_ratio);
 		}
	amrex::Box cdomain = CDomain;
 	for (int ilev = 0; ilev < nlevels; ++ilev)
 		{
 			cgrids[ilev].define(cdomain);
 			cgrids[ilev].maxSize(max_grid_size);
 			// if (orientation == "h")
			//cdomain.grow(amrex::IntVect(AMREX_D_DECL(0,-ncells/4,0))); 
 			// else if (orientation == "v")
 			// 	cdomain.grow(amrex::IntVect(AMREX_D_DECL(-ncells/4,0,0))); 
			cdomain.grow(amrex::IntVect(-ncells/4)); 
			
			//cdomain.growHi(1,-ncells/2); 

 			cdomain.refine(ref_ratio); 
 			ngrids[ilev] = cgrids[ilev];
 			ngrids[ilev].convert(amrex::IntVect::TheNodeVector());
 		}

 	int number_of_components = AMREX_SPACEDIM;
 	for (int ilev = 0; ilev < nlevels; ++ilev)
 		{
 			dmap   [ilev].define(cgrids[ilev]);
 			solution_numeric[ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1); 
 			solution_exact  [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1);
 			solution_error  [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1);
 			rhs_prescribed  [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1);
 			rhs_numeric     [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1);
 			rhs_exact       [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1);
 			res_numeric     [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1); 
 			res_exact       [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1); 
 			ghost_force     [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1); 
 		}

}

//
//
// Let Omega = [0, 1]^2
//
// Let u_i = u_i^{mn} sin(pi x m) sin(pi y n) and
//     b_i = b_i^{mn} sin(pi x m) sin(pi y n) and
// Then u_{i,jj} = - u_i^{mn} pi^2 (m^2 + n^2) sin(pi x m) sin(pi y n) 
//
// Governing equation
//   C_{ijkl} u_{k,jl} + b_i = 0
//
// Let C_{ijkl} = alpha delta_{ik} delta_{jl}
// then
//   C_{ijkl} u_{k,jl} = alpha delta_{ik} delta_{jl} u_{k,jl}
//                     = alpha u_{i,jj}
// so
//   - alpha u_i^{mn} pi^2 (m^2 + n^2) sin(pi x m) sin(pi y n) + b_i^{mn} sin(pi x m) sin(pi y n) = 0
// or
//     alpha u_i^{mn} pi^2 (m^2 + n^2) sin(pi x m) sin(pi y n) = b_i^{mn} sin(pi x m) sin(pi y n)
// using orthogonality
//     alpha u_i^{mn} pi^2 (m^2 + n^2) = b_i^{mn}
// or
//     u_i^{mn}  = b_i^{mn} / (alpha * pi^2 * (m^2 + n^2))
// 
//
int
Elastic::TrigTest(bool verbose, int component, int n, std::string plotfile)
{
	Set::Scalar tolerance = 0.01;

	int failed = 0;

	Set::Scalar alpha = 1.0;
	Model::Solid::LinearElastic::Laplacian model(alpha);
	amrex::Vector<amrex::FabArray<amrex::BaseFab<Model::Solid::LinearElastic::Laplacian> > >
		modelfab(nlevels); 

 	for (int ilev = 0; ilev < nlevels; ++ilev) modelfab[ilev].define(ngrids[ilev], dmap[ilev], 1, 1);
 	for (int ilev = 0; ilev < nlevels; ++ilev) modelfab[ilev].setVal(model);

	IC::Trig icrhs(geom,1.0,n,n);
	icrhs.SetComp(component);
	Set::Scalar dim = (Set::Scalar)(AMREX_SPACEDIM);
	IC::Trig icexact(geom,-(1./dim/Set::Constant::Pi/Set::Constant::Pi),n,n);
	icexact.SetComp(component);

	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
		solution_exact  [ilev].setVal(0.0);
		solution_numeric[ilev].setVal(0.0);
		solution_error  [ilev].setVal(0.0);
		rhs_prescribed  [ilev].setVal(0.0);
		rhs_exact       [ilev].setVal(0.0);
		rhs_numeric     [ilev].setVal(0.0);
		res_exact       [ilev].setVal(0.0);
		res_numeric     [ilev].setVal(0.0);
		ghost_force     [ilev].setVal(0.0);

		icrhs.Initialize(ilev,rhs_prescribed);
		icexact.Initialize(ilev,solution_exact);
		//icexact.Initialize(ilev,solution_numeric);
	}
	//rhs_prescribed[1].mult(0.5);

	amrex::LPInfo info;
 	info.setAgglomeration(1);
 	info.setConsolidation(1);
 	info.setMaxCoarseningLevel(0);
 	nlevels = geom.size();

	::Operator::Elastic<Model::Solid::LinearElastic::Laplacian> elastic;

 	elastic.define(geom, cgrids, dmap, info);
 	//elastic.setMaxOrder(linop_maxorder);
	
 	using bctype = ::Operator::Elastic<Model::Solid::LinearElastic::Laplacian>::BC;


 	for (int ilev = 0; ilev < nlevels; ++ilev) elastic.SetModel(ilev,modelfab[ilev]);
 	elastic.SetBC({{AMREX_D_DECL({AMREX_D_DECL(bctype::Displacement,bctype::Displacement,bctype::Displacement)},
				     {AMREX_D_DECL(bctype::Displacement,bctype::Displacement,bctype::Displacement)},
				     {AMREX_D_DECL(bctype::Displacement,bctype::Displacement,bctype::Displacement)})}},
 		      {{AMREX_D_DECL({AMREX_D_DECL(bctype::Displacement,bctype::Displacement,bctype::Displacement)},
				     {AMREX_D_DECL(bctype::Displacement,bctype::Displacement,bctype::Displacement)},
				     {AMREX_D_DECL(bctype::Displacement,bctype::Displacement,bctype::Displacement)})}});


	amrex::MLMG mlmg(elastic);
	mlmg.setMaxIter(100);
	mlmg.setMaxFmgIter(20);
 	if (verbose)
 	{
 		mlmg.setVerbose(2);
 		mlmg.setCGVerbose(2);
 	}
 	else
 	{
 		mlmg.setVerbose(0);
 		mlmg.setCGVerbose(0);
	}
 	mlmg.setBottomMaxIter(50);
 	mlmg.setFinalFillBC(false);	
 	mlmg.setBottomSolver(MLMG::BottomSolver::bicgstab);

#if 1
	// Solution	
	Set::Scalar tol_rel = 1E-6;
	Set::Scalar tol_abs = 0.0;
 	mlmg.solve(GetVecOfPtrs(solution_numeric), GetVecOfConstPtrs(rhs_prescribed), tol_rel, tol_abs);

	// Compute solution error
	for (int i = 0; i < nlevels; i++)
	{
		amrex::MultiFab::Copy(solution_error[i],solution_numeric[i],component,component,1,1);
		amrex::MultiFab::Subtract(solution_error[i],solution_exact[i],component,component,1,1);
	}
	

	// Compute numerical right hand side
	mlmg.apply(GetVecOfPtrs(rhs_numeric),GetVecOfPtrs(solution_numeric));

	// Compute exact right hand side
	mlmg.apply(GetVecOfPtrs(rhs_exact),GetVecOfPtrs(solution_exact));

	
	// Compute the numeric residual
	for (int i = 0; i < nlevels; i++)
	{
		amrex::MultiFab::Copy(res_numeric[i],rhs_numeric[i],component,component,1,0);
		amrex::MultiFab::Subtract(res_numeric[i],rhs_prescribed[i],component,component,1,0);
	}
	for (int ilev = nlevels-1; ilev > 0; ilev--)
	 	elastic.Reflux(0,
	 		       res_numeric[ilev-1], solution_numeric[ilev-1], rhs_prescribed[ilev-1],
	 		       res_numeric[ilev],   solution_numeric[ilev],   rhs_prescribed[ilev]);

	// Compute the exact residual
	for (int i = 0; i < nlevels; i++)
	{
		amrex::MultiFab::Copy(res_exact[i],rhs_exact[i],component,component,1,0);
		amrex::MultiFab::Subtract(res_exact[i],rhs_prescribed[i],component,component,1,0);
	}
	for (int ilev = nlevels-1; ilev > 0; ilev--)
	 	elastic.Reflux(0,
	 		       res_exact[ilev-1], solution_exact[ilev-1], rhs_prescribed[ilev-1],
	 		       res_exact[ilev],   solution_exact[ilev],   rhs_prescribed[ilev]);

	// Compute the "ghost force" that introduces the error
	mlmg.apply(GetVecOfPtrs(ghost_force),GetVecOfPtrs(solution_error));

#endif
	if (plotfile != "")
	{
		Util::Message(INFO,"Printing plot file to ",plotfile);
		const int output_comp = varname.size();

		Vector<MultiFab> plotmf(nlevels);
		for (int ilev = 0; ilev < nlevels; ++ilev)
		{
			//if (ilev==1) ngrids[ilev].growHi(1,1);
			plotmf			[ilev].define(ngrids[ilev], dmap[ilev], output_comp, 0);
			MultiFab::Copy(plotmf	[ilev], solution_exact [ilev], 0, 0,  2, 0); // 
			MultiFab::Copy(plotmf	[ilev], solution_numeric[ilev],0, 2,  2, 0); // 
			MultiFab::Copy(plotmf	[ilev], solution_error [ilev], 0, 4,  2, 0); // 
			MultiFab::Copy(plotmf	[ilev], rhs_prescribed [ilev], 0, 6,  2, 0); // 
			MultiFab::Copy(plotmf	[ilev], rhs_exact      [ilev], 0, 8,  2, 0); // 
			MultiFab::Copy(plotmf	[ilev], rhs_numeric    [ilev], 0, 10, 2, 0); // 
			MultiFab::Copy(plotmf	[ilev], res_exact      [ilev], 0, 12, 2, 0); // 
			MultiFab::Copy(plotmf	[ilev], res_numeric    [ilev], 0, 14, 2, 0); // 
			MultiFab::Copy(plotmf	[ilev], ghost_force    [ilev], 0, 16, 2, 0); // 
		}


		amrex::WriteMultiLevelPlotfile(plotfile, nlevels, amrex::GetVecOfConstPtrs(plotmf),
					       varname, geom, 0.0, Vector<int>(nlevels, 0),
					       Vector<IntVect>(nlevels, IntVect{ref_ratio}));
	}

	// Find maximum solution error
	std::vector<Set::Scalar> error_norm(nlevels);
	for (int i = 0; i < nlevels; i++) error_norm[i] = solution_error[0].norm0(component,0,false) / solution_exact[0].norm0(component,0,false);
	Set::Scalar maxnorm = fabs(*std::max_element(error_norm.begin(),error_norm.end()));

	if (verbose) Util::Message(INFO,"relative error = ", 100*maxnorm, " %");

	if (maxnorm > tolerance) failed += 1;

	return failed;
}

int Elastic::UniaxialTest(bool verbose,
		 int component,
		 int n,
		 std::string plotfile)
{
  /// \todo Copy and paste code from TrigTest to use as template
  /// \todo Run test to see if it works
  /// \todo Modify so that the test does the following:
  ///        - Dirichlet boundary conditions in the x direction, x displacement
  ///        - Traction-free boundary conditions elsewhere
  ///        - No body force
  /// \todo Find the analytic solution to this problem
  /// \todo Compare the numeric solution to the analytic solution
  /// \todo If they match, then return 0. Otherwise return 1.
  return 1;
}



int Elastic::RefluxTest(int verbose)
{
	int failed = 0;

	using model_type = Model::Solid::LinearElastic::Isotropic; model_type model(2.6,6.0); 

	amrex::Vector<amrex::FabArray<amrex::BaseFab<Model::Solid::LinearElastic::Isotropic> > >
		modelfab(nlevels); 
 	for (int ilev = 0; ilev < nlevels; ++ilev) modelfab[ilev].define(ngrids[ilev], dmap[ilev], 1, 1);
 	for (int ilev = 0; ilev < nlevels; ++ilev) modelfab[ilev].setVal(model);


 	LPInfo info;
 	info.setAgglomeration(1);
 	info.setConsolidation(1);
 	info.setMaxCoarseningLevel(0);
 	nlevels = geom.size();


	::Operator::Elastic<Model::Solid::LinearElastic::Isotropic> elastic;
 	elastic.define(geom, cgrids, dmap, info);
 	for (int ilev = 0; ilev < nlevels; ++ilev) elastic.SetModel(ilev,modelfab[ilev]);


	std::vector<int> comps = {{1}};
	std::vector<Set::Scalar> alphas = {{1.0}};
	std::vector<Set::Scalar> ms = {{1.0,2.0}};
	std::vector<Set::Vector> ns = {{Set::Vector(AMREX_D_DECL(1,0,0)),
					Set::Vector(AMREX_D_DECL(-1,0,0)),
					Set::Vector(AMREX_D_DECL(0,1,0)),
					Set::Vector(AMREX_D_DECL(0,-1,0)),
					Set::Vector(AMREX_D_DECL(1,1,0))}};
	std::vector<Set::Vector> bs = {{Set::Vector(AMREX_D_DECL(0,0.25,0))}};

	for (std::vector<int>::iterator comp = comps.begin(); comp != comps.end(); comp++)
	for (std::vector<Set::Scalar>::iterator m = ms.begin(); m != ms.end(); m++)
	for (std::vector<Set::Vector>::iterator n = ns.begin(); n != ns.end(); n++)
	for (std::vector<Set::Vector>::iterator b = bs.begin(); b != bs.end(); b++)
	{
		::Operator::Elastic<model_type> mlabec;
	
		mlabec.define(geom, cgrids, dmap, info);
		mlabec.setMaxOrder(2);
		for (int ilev = 0; ilev < nlevels; ++ilev) mlabec.SetModel(ilev,modelfab[ilev]);
		mlabec.SetBC({{AMREX_D_DECL(::Operator::Elastic<model_type>::BC::Displacement,
					    ::Operator::Elastic<model_type>::BC::Displacement,
					    ::Operator::Elastic<model_type>::BC::Displacement)}},
			{{AMREX_D_DECL(::Operator::Elastic<model_type>::BC::Displacement,
				       ::Operator::Elastic<model_type>::BC::Displacement,
				       ::Operator::Elastic<model_type>::BC::Displacement)}});


		solution_exact[0].setVal(0.0);
		solution_exact[1].setVal(0.0);
		rhs_prescribed[0].setVal(0.0);
		rhs_prescribed[1].setVal(0.0);

		IC::Affine ic(geom,*n,1.0,*b,true,*m);

		ic.SetComp(*comp);
		ic.Initialize(0,solution_exact);
		ic.Initialize(1,solution_exact);

		mlabec.FApply(0,0,rhs_prescribed[0],solution_exact[0]);
		mlabec.FApply(1,0,rhs_prescribed[1],solution_exact[1]);

		res_numeric[0].setVal(0.0);
		res_numeric[1].setVal(0.0);

		mlabec.BuildMasks();
		mlabec.Reflux(0,
		 	      res_numeric[0], solution_exact[0], rhs_prescribed[0],
		 	      res_numeric[1], solution_exact[1], rhs_prescribed[1]);

		
		Set::Scalar residual = res_numeric[0].norm0();

		if (rhs_prescribed[0].norm0() > 1E-15) residual /= rhs_prescribed[0].norm0();

		std::stringstream ss;
		ss << "n=["<< std::setw(5) << (n->transpose()) << "], "
		   << "b=["<< std::setw(5) << (b->transpose()) << "], "
		   << "m=" << *m<<", "
		   << "comp="<<(*comp);

		bool pass = fabs(residual) < 1E-12;

		if (verbose > 0) Util::Test::SubMessage(ss.str(), !pass);

		if (!pass) failed++;
	}
	
	return failed;

}


int SpatiallyVaryingCTest(int /*verbose*/)
{
	/*
	using model_type = Model::Solid::LinearElastic::Degradable::Isotropic;
	model_type model(400,300);

	amrex::Vector<amrex::Geometry>		geom;
	amrex::Vector<amrex::BoxArray> 		ngrids,cgrids;
	amrex::Vector<amrex::DistributionMapping>	dmap;

	amrex::Vector<amrex::MultiFab> 	u;
	amrex::Vector<amrex::MultiFab> 	rhs;
	amrex::Vector<amrex::MultiFab> 	res;
	amrex::Vector<amrex::MultiFab> 	stress;
	amrex::Vector<amrex::MultiFab> 	strain;
	amrex::Vector<amrex::MultiFab> 	energy;

	amrex::Vector<amrex::FabArray<amrex::BaseFab<model_type> > > modelfab;
	std::map<std::string,::Operator::Elastic<model_type>::BC > 			bc_map;
	amrex::Vector<std::string> AMREX_D_DECL(bc_x_lo_str,bc_y_lo_str,bc_z_lo_str);
	amrex::Vector<std::string> AMREX_D_DECL(bc_x_hi_str,bc_y_hi_str,bc_z_hi_str);
	std::array<::Operator::Elastic<model_type>::BC,AMREX_SPACEDIM> AMREX_D_DECL(bc_x_lo,bc_y_lo,bc_z_lo);
	std::array<::Operator::Elastic<model_type>::BC,AMREX_SPACEDIM> AMREX_D_DECL(bc_x_hi,bc_y_hi,bc_z_hi);

	bc_map["displacement"] = 	::Operator::Elastic<model_type>::BC::Displacement;
	bc_map["disp"] = 		::Operator::Elastic<model_type>::BC::Displacement;
	bc_map["traction"] = 		::Operator::Elastic<model_type>::BC::Traction;
	bc_map["trac"] = 		::Operator::Elastic<model_type>::BC::Traction;
	bc_map["periodic"] = 		::Operator::Elastic<model_type>::BC::Periodic;

	AMREX_D_TERM(	bc_x_lo_str = {AMREX_D_DECL("disp", "disp", "disp")};
					bc_x_hi_str = {AMREX_D_DECL("disp", "disp", "disp")};
					,
					bc_y_lo_str = {AMREX_D_DECL("trac", "trac", "trac")};
					bc_y_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};
					,
					bc_z_lo_str = {AMREX_D_DECL("trac", "trac", "trac")};
					bc_z_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};
				);

	AMREX_D_TERM(	bc_x_lo = {AMREX_D_DECL(bc_map[bc_x_lo_str[0]], bc_map[bc_x_lo_str[1]], bc_map[bc_x_lo_str[2]])};
					bc_x_hi = {AMREX_D_DECL(bc_map[bc_x_hi_str[0]], bc_map[bc_x_hi_str[1]], bc_map[bc_x_hi_str[2]])};
					,
					bc_y_lo = {AMREX_D_DECL(bc_map[bc_y_lo_str[0]], bc_map[bc_y_lo_str[1]], bc_map[bc_y_lo_str[2]])};
					bc_y_hi = {AMREX_D_DECL(bc_map[bc_y_hi_str[0]], bc_map[bc_y_hi_str[1]], bc_map[bc_y_hi_str[2]])};
					,
					bc_z_lo = {AMREX_D_DECL(bc_map[bc_z_lo_str[0]], bc_map[bc_z_lo_str[1]], bc_map[bc_z_lo_str[2]])};
					bc_z_hi = {AMREX_D_DECL(bc_map[bc_z_hi_str[0]], bc_map[bc_z_hi_str[1]], bc_map[bc_z_hi_str[2]])};);

	Set::Vector AMREX_D_DECL(bc_left,bc_bottom,bc_back);
	Set::Vector AMREX_D_DECL(bc_right,bc_top,bc_front);

	AMREX_D_TERM(	bc_left = {AMREX_D_DECL(0.,0.,0.)};
					bc_right = {AMREX_D_DECL(1.,0.,0.)};,
					bc_bottom = {AMREX_D_DECL(0.,0.,0.)};
					bc_top = {AMREX_D_DECL(0.,0.,0.)};,
					bc_back = {AMREX_D_DECL(0.,0.,0.)};
					bc_front = {AMREX_D_DECL(0.,0.,0.)};);

	
	int n_cell = 16;
 	int nlevels = 1;
	int ref_ratio = 2;
	int max_grid_size = 100000;
	Set::Vector body_force(AMREX_D_DECL(0.,-0.0001,0.));

 	
 	geom.resize(nlevels);	

 	ngrids.resize(nlevels);
 	cgrids.resize(nlevels);
 	dmap.resize(nlevels);

 	u.resize(nlevels);
 	res.resize(nlevels);
 	rhs.resize(nlevels);
 	stress.resize(nlevels);
 	strain.resize(nlevels);
 	energy.resize(nlevels);
	modelfab.resize(nlevels);

	amrex::RealBox rb({AMREX_D_DECL(0.,0.,0.)}, {AMREX_D_DECL(1.,1.,1.)});
	amrex::Geometry::Setup(&rb, 0);

	amrex::Box NDomain(amrex::IntVect{AMREX_D_DECL(0,0,0)},
			   amrex::IntVect{AMREX_D_DECL(n_cell,n_cell,n_cell)},
			   amrex::IntVect::TheNodeVector());
	amrex::Box CDomain = amrex::convert(NDomain, amrex::IntVect::TheCellVector());

	amrex::Box domain = CDomain;
 	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
		geom[ilev].define(domain);
		domain.refine(ref_ratio);
 	}
	amrex::Box cdomain = CDomain;
 	for (int ilev = 0; ilev < nlevels; ++ilev)
 	{
 		cgrids[ilev].define(cdomain);
 		cgrids[ilev].maxSize(max_grid_size);
 		cdomain.refine(ref_ratio); 

		ngrids[ilev] = cgrids[ilev];
		ngrids[ilev].convert(amrex::IntVect::TheNodeVector());
	}

	int number_of_components = AMREX_SPACEDIM;
	int number_of_stress_components = AMREX_SPACEDIM*AMREX_SPACEDIM;
 	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
 		dmap   [ilev].define(cgrids[ilev]);
 		u       [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1); 
 		res     [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1); 
 		rhs     [ilev].define(ngrids[ilev], dmap[ilev], number_of_components, 1);
 		stress     [ilev].define(ngrids[ilev], dmap[ilev], number_of_stress_components, 1);
 		strain     [ilev].define(ngrids[ilev], dmap[ilev], number_of_stress_components, 1);
 		energy     [ilev].define(ngrids[ilev], dmap[ilev], 1, 1);
		modelfab[ilev].define(ngrids[ilev], dmap[ilev], 1, 1);
 	}

 	for (int ilev = 0; ilev < nlevels; ++ilev)
 	{
 		u[ilev].setVal(0.0);
 		res[ilev].setVal(0.0);
 		rhs[ilev].setVal(0.0);
 		stress[ilev].setVal(0.0);
 		strain[ilev].setVal(0.0);
 		energy[ilev].setVal(0.0);
		modelfab[ilev].setVal(model);
 	}

 	LPInfo info;
 	info.setAgglomeration(1);
 	info.setConsolidation(1);
 	info.setMaxCoarseningLevel(0);
 	nlevels = geom.size();

 	std::vector<Set::Vector> normal = {{Set::Vector(AMREX_D_DECL(0.,1.,0.))}};
 	std::vector<Set::Vector> point = {{Set::Vector(AMREX_D_DECL(0.95,0.95,0.95))}};
 	std::vector<Set::Scalar> value = {0.1};
 	std::vector<Set::Scalar> exponent = {0.};

 	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
		for (amrex::MFIter mfi(modelfab[ilev],true); mfi.isValid(); ++mfi)
		{
			const amrex::Box& box = mfi.tilebox();
	 		amrex::BaseFab<model_type> &modelbox = (modelfab[ilev])[mfi];
	 		AMREX_D_TERM(for (int i = box.loVect()[0]-1; i<=box.hiVect()[0]+1; i++),
		 		     for (int j = box.loVect()[1]-1; j<=box.hiVect()[1]+1; j++),
		 		     for (int k = box.loVect()[2]-1; k<=box.hiVect()[2]+1; k++))
	 		{
	 			amrex::IntVect m(AMREX_D_DECL(i,j,k));
	 			std::vector<Set::Vector> corners = {	{AMREX_D_DECL(geom[ilev].ProbLo()[0],geom[ilev].ProbLo()[1],geom[ilev].ProbLo()[2])},
	 													{AMREX_D_DECL(geom[ilev].ProbHi()[0],geom[ilev].ProbLo()[1],geom[ilev].ProbLo()[2])}
	 													#if AMREX_SPACEDIM > 1
	 													,{AMREX_D_DECL(geom[ilev].ProbLo()[0],geom[ilev].ProbHi()[1],geom[ilev].ProbLo()[2])}
	 													,{AMREX_D_DECL(geom[ilev].ProbHi()[0],geom[ilev].ProbHi()[1],geom[ilev].ProbLo()[2])}
	 													#if AMREX_SPACEDIM > 2
	 													,{AMREX_D_DECL(geom[ilev].ProbLo()[0],geom[ilev].ProbLo()[1],geom[ilev].ProbHi()[2])}
	 													,{AMREX_D_DECL(geom[ilev].ProbHi()[0],geom[ilev].ProbLo()[1],geom[ilev].ProbHi()[2])}
	 													,{AMREX_D_DECL(geom[ilev].ProbLo()[0],geom[ilev].ProbHi()[1],geom[ilev].ProbHi()[2])}
	 													,{AMREX_D_DECL(geom[ilev].ProbHi()[0],geom[ilev].ProbHi()[1],geom[ilev].ProbHi()[2])}
	 													#endif
	 													#endif
	 													};
	 			Set::Scalar max_dist = 0.0;
	 			AMREX_D_TERM(	amrex::Real x1 = geom[ilev].ProbLo()[0] + ((amrex::Real)(i)) * geom[ilev].CellSize()[0];,
					     		amrex::Real x2 = geom[ilev].ProbLo()[1] + ((amrex::Real)(j)) * geom[ilev].CellSize()[1];,
					     		amrex::Real x3 = geom[ilev].ProbLo()[2] + ((amrex::Real)(k)) * geom[ilev].CellSize()[2];);
	 			
	 			Set::Vector x(AMREX_D_DECL(x1,x2,x3));

	 			for (std::vector<Set::Vector>::iterator corner = corners.begin(); corner != corners.end(); corner++)
	 				max_dist = std::max (max_dist, (*corner-point[0]).dot(normal[0]));
	 			
	 			Set::Scalar dist = (x-point[0]).dot(normal[0]);
	 			if(dist >= 0.0)
	 				modelbox(m).DegradeModulus({value[0]*std::pow((dist/max_dist),exponent[0])});

	 		}
		}
	}

	Elastic<model_type> mlabec;
	
	mlabec.define(geom, cgrids, dmap, info);
	mlabec.setMaxOrder(2);
	mlabec.SetBC({{AMREX_D_DECL(bc_x_lo,bc_y_lo,bc_z_lo)}},
		     				{{AMREX_D_DECL(bc_x_hi,bc_y_hi,bc_z_hi)}});

	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
		const Real* DX = geom[ilev].CellSize();
		Set::Scalar volume = AMREX_D_TERM(DX[0],*DX[1],*DX[2]);

		mlabec.SetModel(ilev,modelfab[ilev]);
		AMREX_D_TERM(rhs[ilev].setVal(body_force[0]*volume,0,1);,
					rhs[ilev].setVal(body_force[1]*volume,1,1);,
					rhs[ilev].setVal(body_force[2]*volume,2,1););
		for (amrex::MFIter mfi(rhs[ilev],true); mfi.isValid(); ++mfi)
		{
		 	const amrex::Box& box = mfi.tilebox();

		 	amrex::BaseFab<amrex::Real> &rhsfab = (rhs[ilev])[mfi];

		 	AMREX_D_TERM(for (int i = box.loVect()[0]; i<=box.hiVect()[0]; i++),
		 		     for (int j = box.loVect()[1]; j<=box.hiVect()[1]; j++),
		 		     for (int k = box.loVect()[2]; k<=box.hiVect()[2]; k++))
		 	{
		 		bool AMREX_D_DECL(xmin = false, ymin = false, zmin = false);
				bool AMREX_D_DECL(xmax = false, ymax = false, zmax = false);

		 		AMREX_D_TERM(	xmin = (i == geom[ilev].Domain().loVect()[0]);
		 						xmax = (i == geom[ilev].Domain().hiVect()[0]+1);
		 						,
		 						ymin = (j == geom[ilev].Domain().loVect()[1]);
		 						ymax = (j == geom[ilev].Domain().hiVect()[1]+1);
		 						,
		 						zmin = (k == geom[ilev].Domain().loVect()[2]);
		 						zmax = (k == geom[ilev].Domain().hiVect()[2]+1););
		 	
		 
		 		if (false || AMREX_D_TERM(xmin || xmax, || ymin || ymax, || zmin || zmax))
		 		{
		 			AMREX_D_TERM(	rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),0) = 0.0;,
 									rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),1) = 0.0;,
 									rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),2) = 0.0;);
		 			for(int l = 0; l<AMREX_SPACEDIM; l++)
					{
						AMREX_D_TERM(
							if(xmin && bc_x_lo[l]==Elastic<model_type>::BC::Displacement)
								rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),l) = bc_left[l];
							if(xmax && bc_x_hi[l]==Elastic<model_type>::BC::Displacement)
								rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),l) = bc_right[l];
							,
							if(ymin && bc_y_lo[l]==Elastic<model_type>::BC::Displacement)
								rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),l) = bc_bottom[l];
							if(ymax && bc_y_hi[l]==Elastic<model_type>::BC::Displacement)
								rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),l) = bc_top[l];
							,
							if(zmin && bc_z_lo[l]==Elastic<model_type>::BC::Displacement)
								rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),l) = bc_back[l];
							if(zmax && bc_z_hi[l]==Elastic<model_type>::BC::Displacement)
								rhsfab(amrex::IntVect(AMREX_D_DECL(i,j,k)),l) = bc_front[l];
							);
					}
		 		}
		 	}
		}
	}

	amrex::MLMG solver(mlabec);
	solver.setMaxIter(10000);
	solver.setMaxFmgIter(10000);
	solver.setVerbose(4);
	solver.setCGVerbose(4);
	solver.setBottomMaxIter(10000);
	solver.setBottomSolver(MLMG::BottomSolver::bicgstab);
	
	solver.solve(GetVecOfPtrs(u),
		GetVecOfConstPtrs(rhs),
		1E-8,
		1E-8);

	for (int lev = 0; lev < nlevels; lev++)
	{
		mlabec.Strain(lev,strain[lev],u[lev]);
		mlabec.Stress(lev,stress[lev],u[lev]);
		mlabec.Energy(lev,energy[lev],u[lev]);
	}
	*/
	Util::Abort(INFO,"Test is not fully implemente yet");
	return 1;
}


}
}
	     

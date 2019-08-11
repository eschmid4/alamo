#include "Fracture.H"

namespace Integrator
{
Fracture::Fracture() :
	Integrator()
{
	// Crack model
	amrex::ParmParse pp_crack("crack");
	std::string crack_type;
	pp_crack.query("type",crack_type);

	if(crack_type=="constant")
	{
		amrex::ParmParse pp_crack_constant("crack.constant");
		Set::Scalar G_c, zeta;
		pp_crack.query("G_c",G_c);
		pp_crack.query("zeta",zeta);
		boundary = new Model::Interface::Crack::Constant(G_c,zeta);
	}
	else
		Util::Abort(INFO,"This crack model hasn't been implemented yet");

	// ICs
	amrex::ParmParse pp("ic"); // Phase-field model parameters
	pp.query("type", ic_type);

	if(ic_type == "cuboid")
		ic = new IC::Cuboid(geom);
	else if(ic_type == "ellipsoid")
		ic = new IC::Ellipsoid(geom);
	else
		Util::Abort(INFO,"This type of IC hasn't been implemented yet");
	
	// BCs
	// Crack field should have a zero neumann BC. So we just code it up here.
	// In case this needs to change, we can add options to read it from input.
	amrex::Vector<std::string> bc_hi_str(AMREX_SPACEDIM), bc_lo_str(AMREX_SPACEDIM);
	amrex::Vector<Set::Scalar> AMREX_D_DECL(bc_lo_1,bc_lo_2,bc_lo_3);
	amrex::Vector<Set::Scalar> AMREX_D_DECL(bc_hi_1,bc_hi_2,bc_hi_3);

	// Below are conditions for full simulation.  If this doesn't work, we can
	// try symmetric simulation.
	bc_lo_str = {AMREX_D_DECL("Neumann", "Neumann", "Neumann")};
	bc_hi_str = {AMREX_D_DECL("Neumann", "Neumann", "Neumann")};
	/*/bc_lo_str = {AMREX_D_DECL("REFLECT_EVEN", "REFLECT_EVEN", "REFLECT_EVEN")};
	bc_hi_str = {AMREX_D_DECL("Neumann", "Neumann", "Neumann")};*/
	AMREX_D_TERM( 	bc_lo_1 = {0.}; bc_hi_1 = {0.};,
					bc_lo_2 = {0.}; bc_hi_2 = {0.};,
					bc_lo_3 = {0.}; bc_hi_3 = {0.};
	);
	mybc = new BC::Constant(bc_hi_str, bc_lo_str
				  ,AMREX_D_DECL(bc_lo_1, bc_lo_2, bc_lo_3)
				  ,AMREX_D_DECL(bc_hi_1, bc_hi_2, bc_hi_3));

	RegisterNewFab(m_c,     mybc, 1, number_of_ghost_cells, "c");
	RegisterNewFab(m_cold, 	mybc, 1, number_of_ghost_cells, "cold");


	// Material input
	amrex::ParmParse pp_material("material");
	pp_material.query("model",input_material);
	if(input_material == "isotropic")
	{
		//Util::Abort(INFO, "Check for model_type in PD.H");
		Set::Scalar lambda = 410.0;
		Set::Scalar mu = 305.0;
		amrex::ParmParse pp_material_isotropic("material.isotropic");
		pp_material_isotropic.query("lambda",lambda);
		pp_material_isotropic.query("mu",mu);
		if(lambda <=0) { Util::Warning(INFO,"Lambda must be positive. Resetting back to default value"); lambda = 410.0; }
		if(mu <= 0) { Util::Warning(INFO,"Mu must be positive. Resetting back to default value"); mu = 305.0; }
		modeltype = new model_type(lambda,mu);
	}
	else
		Util::Abort(INFO,"This model has not been implemented yet.");

	// Crack properties
	
	//pp_crack.query("C_phi",crack.C_phi);

	// Elasticity properties
	amrex::ParmParse pp_elastic("elastic");
	pp_elastic.query("int",				elastic.interval);
	pp_elastic.query("type",			elastic.type);
	pp_elastic.query("max_iter",		elastic.max_iter);
	pp_elastic.query("max_fmg_iter",	elastic.max_fmg_iter);
	pp_elastic.query("verbose",			elastic.verbose);
	pp_elastic.query("cgverbose",		elastic.cgverbose);
	pp_elastic.query("tol_rel",			elastic.tol_rel);
	pp_elastic.query("tol_abs",			elastic.tol_abs);
	pp_elastic.query("cg_tol_rel",		elastic.cg_tol_rel);
	pp_elastic.query("cg_tol_abs",		elastic.cg_tol_abs);
	pp_elastic.query("use_fsmooth",		elastic.use_fsmooth);
	pp_elastic.query("agglomeration", 	elastic.agglomeration);
	pp_elastic.query("consolidation", 	elastic.consolidation);

	pp_elastic.query("bottom_solver",elastic.bottom_solver);
	pp_elastic.query("linop_maxorder", elastic.linop_maxorder);
	pp_elastic.query("max_coarsening_level",elastic.max_coarsening_level);
	pp_elastic.query("verbose",elastic.verbose);
	pp_elastic.query("cg_verbose", elastic.cgverbose);
	pp_elastic.query("bottom_max_iter", elastic.bottom_max_iter);
	pp_elastic.query("max_fixed_iter", elastic.max_fixed_iter);
	pp_elastic.query("bottom_tol", elastic.bottom_tol);

	if (pp_elastic.countval("body_force")) pp_elastic.getarr("body_force",elastic.body_force);

	amrex::ParmParse pp_elastic_bc("elastic.bc");
	amrex::Vector<std::string> AMREX_D_DECL(bc_x_lo_str,bc_y_lo_str,bc_z_lo_str);
	amrex::Vector<std::string> AMREX_D_DECL(bc_x_hi_str,bc_y_hi_str,bc_z_hi_str);

	std::map<std::string,BC::Operator::Elastic<model_type>::Type >        bc_map;
	bc_map["displacement"] 	= BC::Operator::Elastic<model_type>::Type::Displacement;
	bc_map["disp"] 			= BC::Operator::Elastic<model_type>::Type::Displacement;
	bc_map["traction"] 		= BC::Operator::Elastic<model_type>::Type::Traction;
	bc_map["trac"] 			= BC::Operator::Elastic<model_type>::Type::Traction;
	bc_map["neumann"] 		= BC::Operator::Elastic<model_type>::Type::Neumann;
	bc_map["periodic"] 		= BC::Operator::Elastic<model_type>::Type::Periodic;

		
	AMREX_D_TERM(	bc_lo_1.clear(); bc_hi_1.clear();,
					bc_lo_2.clear(); bc_hi_2.clear();,
					bc_lo_3.clear(); bc_hi_3.clear(););
	//amrex::Vector<Set::Scalar> bc_lo_1, bc_hi_1;
	//amrex::Vector<Set::Scalar> bc_lo_2, bc_hi_2;
	//amrex::Vector<Set::Scalar> bc_lo_3, bc_hi_3;

	/* Need to replace this later with a proper specification of boundary.
	   Right now we are hard-coding the tensile test and just requesting
	   rate of pulling. */
	pp_elastic_bc.query("rate",elastic.test_rate);
	if(elastic.test_rate < 0.) { Util::Warning(INFO,"Rate can't be less than zero. Resetting to 1.0"); elastic.test_rate = 1.0; }

	//Below are the conditions for full tensile test simulation. 
	// If this doesn't work we can try symmetric simulation
	AMREX_D_TERM( 	bc_x_lo_str = {AMREX_D_DECL("disp", "trac", "trac")};
					bc_x_hi_str = {AMREX_D_DECL("disp", "trac", "trac")};
					,
					bc_y_lo_str = {AMREX_D_DECL("trac", "trac", "trac")};
					bc_y_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};
					,
					bc_z_lo_str = {AMREX_D_DECL("trac", "trac", "trac")};
					bc_z_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};);

	/*AMREX_D_TERM( 	bc_x_lo_str = {AMREX_D_DECL("disp", "neumann", "neumann")};
					bc_x_hi_str = {AMREX_D_DECL("disp", "trac", "trac")};
					,
					bc_y_lo_str = {AMREX_D_DECL("neumann", "disp", "neumann")};
					bc_y_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};
					,
					bc_z_lo_str = {AMREX_D_DECL("neumann", "neumann", "disp")};
					bc_z_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};);*/

	AMREX_D_TERM(	elastic.bc_xlo = {AMREX_D_DECL(bc_map[bc_x_lo_str[0]],bc_map[bc_x_lo_str[1]],bc_map[bc_x_lo_str[2]])};
					elastic.bc_xhi = {AMREX_D_DECL(bc_map[bc_x_hi_str[0]],bc_map[bc_x_hi_str[1]],bc_map[bc_x_hi_str[2]])};
					,
					elastic.bc_ylo = {AMREX_D_DECL(bc_map[bc_y_lo_str[0]],bc_map[bc_y_lo_str[1]],bc_map[bc_y_lo_str[2]])};
					elastic.bc_yhi = {AMREX_D_DECL(bc_map[bc_y_hi_str[0]],bc_map[bc_y_hi_str[1]],bc_map[bc_y_hi_str[2]])};
					,
					elastic.bc_zlo = {AMREX_D_DECL(bc_map[bc_z_lo_str[0]],bc_map[bc_z_lo_str[1]],bc_map[bc_z_lo_str[2]])};
					elastic.bc_zhi = {AMREX_D_DECL(bc_map[bc_z_hi_str[0]],bc_map[bc_z_hi_str[1]],bc_map[bc_z_hi_str[2]])};);

	AMREX_D_TERM(	elastic.bc_left = Set::Vector(AMREX_D_DECL(0.,0.,0.));
					elastic.bc_right = Set::Vector(AMREX_D_DECL(0.,0.,0.));
					,
					elastic.bc_bottom = Set::Vector(AMREX_D_DECL(0.,0.,0.));
					elastic.bc_top = Set::Vector(AMREX_D_DECL(0.,0.,0.));
					,
					elastic.bc_back = Set::Vector(AMREX_D_DECL(0.,0.,0.));
					elastic.bc_front = Set::Vector(AMREX_D_DECL(0.,0.,0.)););
	
	const int number_of_stress_components = AMREX_SPACEDIM*AMREX_SPACEDIM;
	
	RegisterNodalFab (m_disp, 		AMREX_SPACEDIM, 				number_of_ghost_cells, "Disp");
	RegisterNodalFab (m_rhs,  		AMREX_SPACEDIM, 				number_of_ghost_cells, "RHS");
	RegisterNodalFab (m_strain,		number_of_stress_components,	number_of_ghost_cells,	"strain");
	RegisterNodalFab (m_stress,		number_of_stress_components,	number_of_ghost_cells,	"stress");
	RegisterNodalFab (m_stressvm,	1,								number_of_ghost_cells,	"stress_vm");
	RegisterNodalFab (m_energy,		1,								number_of_ghost_cells,	"energy");
	RegisterNodalFab (m_energy_pristine,		1,					number_of_ghost_cells,	"energy");;
	RegisterNodalFab (m_residual,	AMREX_SPACEDIM,					number_of_ghost_cells,	"residual");
}

Fracture::~Fracture()
{
}

void
Fracture::Initialize (int lev)
{
	Util::Message(INFO);
	ic->Initialize(lev,m_c);
	ic->Initialize(lev,m_cold);

	Util::Message(INFO);
	m_disp[lev]->setVal(0.0);
	m_strain[lev]->setVal(0.0);
	m_stress[lev]->setVal(0.0);
	m_stressvm[lev]->setVal(0.0);
	m_rhs[lev]->setVal(0.0);
	m_energy[lev]->setVal(0.0);
	m_residual[lev]->setVal(0.0);
	m_energy_pristine[lev] -> setVal(0.);
	Util::Message(INFO);
}

void
Fracture::ScaledModulus(int lev, amrex::FabArray<amrex::BaseFab<model_type> > &model)
{
	/*
	  This function is supposed to degrade material parameters based on certain
	  fracture model.
	  For now we are just using isotropic degradation.
	*/
	Util::Message(INFO);
	static amrex::IntVect AMREX_D_DECL(dx(AMREX_D_DECL(1,0,0)),
					   dy(AMREX_D_DECL(0,1,0)),
					   dz(AMREX_D_DECL(0,0,1)));

	for (amrex::MFIter mfi(model,true); mfi.isValid(); ++mfi)
	{
		const amrex::Box& box = mfi.validbox();
		amrex::Array4<const amrex::Real> const& c_new = (*m_c[lev]).array(mfi);
		amrex::Array4<model_type> const& modelfab = model.array(mfi);

		amrex::ParallelFor (box,[=] AMREX_GPU_DEVICE(int i, int j, int k){
			Set::Scalar mul = 1.0/(AMREX_D_TERM(2.0,+2.0,+4.0));
			// We need to introduce a fracture model in Model/Interface for this. 
			// Right now this is a temporary fix.
			amrex::Vector<Set::Scalar> _temp;
			_temp.push_back( mul*(AMREX_D_TERM(	
								boundary->w_phi(c_new(i,j,k,0)) + boundary->w_phi(c_new(i-1,j,k,0))
								, 
								+ boundary->w_phi(c_new(i,j-1,k,0)) + boundary->w_phi(c_new(i-1,j-1,k,0))
								, 
								+ boundary->w_phi(c_new(i,j,k-1,0)) + boundary->w_phi(c_new(i-1,j,k-1,0))
								+ boundary->w_phi(c_new(i,j-1,k-1,0)) + boundary->w_phi(c_new(i-1,j-1,k-1,0)))
								));
			modelfab(i,j,k,0).DegradeModulus(_temp[0]);
		});
	}
	//Util::Message(INFO, "Exit");
}

void 
Fracture::TimeStepBegin(amrex::Real /*time*/, int iter)
{
	Util::Message(INFO);
	LPInfo info;
	info.setAgglomeration(elastic.agglomeration);
	info.setConsolidation(elastic.consolidation);
	info.setMaxCoarseningLevel(elastic.max_coarsening_level);

	amrex::Vector<amrex::FabArray<amrex::BaseFab<model_type> > > model;
	model.resize(nlevels);
	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
		model[ilev].define(m_disp[ilev]->boxArray(), m_disp[ilev]->DistributionMap(), 1, number_of_ghost_cells);
		model[ilev].setVal(*modeltype);
		ScaledModulus(ilev,model[ilev]);
	}

	Operator::Elastic<model_type> elastic_operator;
	elastic_operator.define(geom, grids, dmap, info);

	for (int ilev = 0; ilev < nlevels; ++ilev)
		elastic_operator.SetModel(ilev,model[ilev]);
	
	elastic_operator.setMaxOrder(elastic.linop_maxorder);
	BC::Operator::Elastic<model_type> bc;
	elastic_operator.SetBC(&bc);

	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
		const Real* DX = geom[ilev].CellSize();
		Set::Scalar volume = AMREX_D_TERM(DX[0],*DX[1],*DX[2]);

		AMREX_D_TERM(m_rhs[ilev]->setVal(elastic.body_force[0]*volume,0,1);,
			     m_rhs[ilev]->setVal(elastic.body_force[1]*volume,1,1);,
			     m_rhs[ilev]->setVal(elastic.body_force[2]*volume,2,1););
	}
	elastic.bc_right[0] += elastic.test_rate*elastic.test_dt;
	AMREX_D_TERM(
		bc.Set(bc.Face::XLO, bc.Direction::X, elastic.bc_xlo[0], elastic.bc_left[0], 	m_rhs, geom);
		bc.Set(bc.Face::XHI, bc.Direction::X, elastic.bc_xhi[0], elastic.bc_right[0], 	m_rhs, geom);
		,
		bc.Set(bc.Face::XLO, bc.Direction::Y, elastic.bc_xlo[1], elastic.bc_left[1], 	m_rhs, geom);
		bc.Set(bc.Face::XHI, bc.Direction::Y, elastic.bc_xhi[1], elastic.bc_right[1], 	m_rhs, geom);
		bc.Set(bc.Face::YLO, bc.Direction::X, elastic.bc_ylo[0], elastic.bc_bottom[0], 	m_rhs, geom);
		bc.Set(bc.Face::YLO, bc.Direction::Y, elastic.bc_ylo[1], elastic.bc_bottom[1], 	m_rhs, geom);
		bc.Set(bc.Face::YHI, bc.Direction::X, elastic.bc_yhi[0], elastic.bc_top[0], 	m_rhs, geom);
		bc.Set(bc.Face::YHI, bc.Direction::Y, elastic.bc_yhi[1], elastic.bc_top[1], 	m_rhs, geom);
		,
		bc.Set(bc.Face::XLO, bc.Direction::Z, elastic.bc_xlo[2], elastic.bc_left[2], 	m_rhs, geom);
		bc.Set(bc.Face::XHI, bc.Direction::Z, elastic.bc_xhi[2], elastic.bc_right[2], 	m_rhs, geom);
		bc.Set(bc.Face::YLO, bc.Direction::Z, elastic.bc_ylo[2], elastic.bc_bottom[2], 	m_rhs, geom);
		bc.Set(bc.Face::YHI, bc.Direction::Z, elastic.bc_yhi[2], elastic.bc_top[2], 	m_rhs, geom);
		bc.Set(bc.Face::ZLO, bc.Direction::X, elastic.bc_zlo[0], elastic.bc_back[0], 	m_rhs, geom);
		bc.Set(bc.Face::ZLO, bc.Direction::Y, elastic.bc_zlo[1], elastic.bc_back[1], 	m_rhs, geom);
		bc.Set(bc.Face::ZLO, bc.Direction::Z, elastic.bc_zlo[2], elastic.bc_back[2], 	m_rhs, geom);
		bc.Set(bc.Face::ZHI, bc.Direction::X, elastic.bc_zhi[0], elastic.bc_front[0], 	m_rhs, geom);
		bc.Set(bc.Face::ZHI, bc.Direction::Y, elastic.bc_zhi[1], elastic.bc_front[1], 	m_rhs, geom);
		bc.Set(bc.Face::ZHI, bc.Direction::Z, elastic.bc_zhi[2], elastic.bc_front[2], 	m_rhs, geom);
	);
	Solver::Nonlocal::Linear solver(elastic_operator);
	solver.setMaxIter(elastic.max_iter);
	solver.setMaxFmgIter(elastic.max_fmg_iter);
	solver.setFixedIter(elastic.max_fixed_iter);
	solver.setVerbose(elastic.verbose);
	solver.setCGVerbose(elastic.cgverbose);
	solver.setBottomMaxIter(elastic.bottom_max_iter);
	solver.setBottomTolerance(elastic.cg_tol_rel) ;
	solver.setBottomToleranceAbs(elastic.cg_tol_abs) ;
	for (int ilev = 0; ilev < nlevels; ilev++) if (m_disp[ilev]->contains_nan()) Util::Warning(INFO);

	if (elastic.bottom_solver == "cg") solver.setBottomSolver(MLMG::BottomSolver::cg);
	else if (elastic.bottom_solver == "bicgstab") solver.setBottomSolver(MLMG::BottomSolver::bicgstab);
	solver.solve(GetVecOfPtrs(m_disp), GetVecOfConstPtrs(m_rhs), elastic.tol_rel, elastic.tol_abs);
	solver.compResidual(GetVecOfPtrs(m_residual),GetVecOfPtrs(m_disp),GetVecOfConstPtrs(m_rhs));
	for (int lev = 0; lev < nlevels; lev++)
	{
		elastic_operator.Strain(lev,*m_strain[lev],*m_disp[lev]);
		elastic_operator.Stress(lev,*m_stress[lev],*m_disp[lev]);
		elastic_operator.Energy(lev,*m_energy[lev],*m_disp[lev]);
	}
	for (int lev = 0; lev < nlevels; lev++)
	{
		for (amrex::MFIter mfi(*m_strain[lev],true); mfi.isValid(); ++mfi)
		{
			const amrex::Box& box = mfi.validbox();
			amrex::Array4<const Set::Scalar> const& strain_box = (*m_strain[lev]).array(mfi);
			amrex::Array4<Set::Scalar> const& energy_box = (*m_energy_pristine[lev]).array(mfi);
			amrex::ParallelFor (box,[=] AMREX_GPU_DEVICE(int i, int j, int k){
				Set::Matrix eps;
				AMREX_D_PICK(
					eps(0,0) = strain_box(i,j,k,0);
					,
					eps(0,0) = strain_box(i,j,k,0); eps(0,1) = strain_box(i,j,k,1);
					eps(1,0) = strain_box(i,j,k,2); eps(1,1) = strain_box(i,j,k,3);
					,
					eps(0,0) = strain_box(i,j,k,0); eps(0,1) = strain_box(i,j,k,1); eps(0,2) = strain_box(i,j,k,2);
					eps(1,0) = strain_box(i,j,k,3); eps(1,1) = strain_box(i,j,k,4); eps(1,2) = strain_box(i,j,k,5);
					eps(2,0) = strain_box(i,j,k,6); eps(2,1) = strain_box(i,j,k,7); eps(2,2) = strain_box(i,j,k,8);
				);
				Set::Matrix sig;
				sig = (*modeltype)(eps);
				for (int m = 0; m < AMREX_SPACEDIM; m++)
				{
					for (int n = 0; n < AMREX_SPACEDIM; n++)
						energy_box(i,j,k,0) += 0.5*sig(m,n)*eps(m,n);
				}
			});
		}
	}
}

void
Fracture::Advance (int lev, amrex::Real /*time*/, amrex::Real dt)
{
	std::swap(*m_cold[lev], 	*m_c[lev]);

	static amrex::IntVect AMREX_D_DECL(	dx(AMREX_D_DECL(1,0,0)), dy(AMREX_D_DECL(0,1,0)), dz(AMREX_D_DECL(0,0,1)));
	const amrex::Real* DX = geom[lev].CellSize();

	for ( amrex::MFIter mfi(*m_c[lev],true); mfi.isValid(); ++mfi )
	{
		const amrex::Box& bx = mfi.validbox();
		amrex::Array4<amrex::Real> const& c_old = (*m_cold[lev]).array(mfi);
		amrex::Array4<amrex::Real> const& c_new = (*m_c[lev]).array(mfi);
		amrex::Array4<const Set::Scalar> const& energy_box = (*m_energy_pristine[lev]).array(mfi);

		amrex::ParallelFor (bx,[=] AMREX_GPU_DEVICE(int i, int j, int k){
			amrex::Real AMREX_D_DECL(grad11 = (c_old(i+1,j,k,0) - 2.*c_old(i,j,k,0) + c_old(i-1,j,k,0))/DX[0]/DX[0],
						 			 grad22 = (c_old(i,j+1,k,0) - 2.*c_old(i,j,k,0) + c_old(i,j-1,k,0))/DX[1]/DX[1],
						 			 grad33 = (c_old(i,j,k+1,0) - 2.*c_old(i,j,k,0) + c_old(i,j,k-1,0))/DX[2]/DX[2]);
			Set::Scalar rhs = 0.;	
			amrex::Real laplacian = AMREX_D_TERM(grad11, + grad22, + grad33);
			Set::Scalar mul = 1.0/(AMREX_D_TERM(2.0,+2.0,+4.0));
			Set::Scalar en_cell = mul*(AMREX_D_TERM(	
								energy_box(i,j,k,0) + energy_box(i+1,j,k,0)
								, 
								+ energy_box(i,j+1,k,0) + energy_box(i+1,j+1,k,0)
								, 
								+ energy_box(i,j,k+1,0) + energy_box(i+1,j,k+1,0)
								+ energy_box(i,j+1,k+1,0) + energy_box(i+1,j+1,k+1,0)
								));

			rhs += boundary->Epc(c_old(i,j,k,0)) > en_cell ? boundary->Dw_phi(c_old(i,j,k,0))*(boundary->Epc(c_old(i,j,k,0))-en_cell) : 0.;
			rhs += boundary->kappa(c_old(i,j,k,0))*laplacian;
			c_new(i,j,k,0) = c_old(i,j,k,0) + dt*rhs;
		});
	}
}

#define C_NEW(i,j,k,m) c_new(amrex::IntVect(AMREX_D_DECL(i,j,k)),m)
void
Fracture::TagCellsForRefinement (int lev, amrex::TagBoxArray& tags, amrex::Real /*time*/, int /*ngrow*/)
{
	const amrex::Real* dx      = geom[lev].CellSize();
	amrex::Vector<int>  itags;
	for (amrex::MFIter mfi(*m_c[lev],true); mfi.isValid(); ++mfi)
	{
		const amrex::Box&  bx  = mfi.tilebox();
		amrex::TagBox&     tag  = tags[mfi];
		amrex::BaseFab<amrex::Real> &c_new = (*m_c[lev])[mfi];

		AMREX_D_TERM(		for (int i = bx.loVect()[0]; i<=bx.hiVect()[0]; i++),
							for (int j = bx.loVect()[1]; j<=bx.hiVect()[1]; j++),
							for (int k = bx.loVect()[2]; k<=bx.hiVect()[2]; k++)
					)
			{
				AMREX_D_TERM(	Set::Scalar gradx = (C_NEW(i+1,j,k,0) - C_NEW(i-1,j,k,0))/(2.*dx[0]);,
								Set::Scalar grady = (C_NEW(i,j+1,k,0) - C_NEW(i,j-1,k,0))/(2.*dx[1]);,
								Set::Scalar gradz = (C_NEW(i,j,k+1,0) - C_NEW(i,j,k-1,0))/(2.*dx[2]);
					)
				Set::Scalar grad = sqrt(AMREX_D_TERM(gradx*gradx, + grady*grady, + gradz*gradz));
				Set::Scalar dr = sqrt(AMREX_D_TERM(dx[0]*dx[0], + dx[1]*dx[1], + dx[2]*dx[2]));

				if(grad*dr > refinement_threshold)
					tag(amrex::IntVect(AMREX_D_DECL(i,j,k))) = amrex::TagBox::SET;
			}

	}
}

}
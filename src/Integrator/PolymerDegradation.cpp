#include "PolymerDegradation.H"
#include "Solver/Linear.H"

Set::Scalar Model::Solid::LinearElastic::Degradable::Isotropic2::E10_iso;
Set::Scalar Model::Solid::LinearElastic::Degradable::Isotropic2::E20_iso; 
Set::Scalar Model::Solid::LinearElastic::Degradable::Isotropic2::Tg0_iso;
Set::Scalar Model::Solid::LinearElastic::Degradable::Isotropic2::Ts0_iso;
Set::Scalar Model::Solid::LinearElastic::Degradable::Isotropic2::temp_iso;
Set::Scalar Model::Solid::LinearElastic::Degradable::Isotropic2::nu_iso;
//#if AMREX_SPACEDIM == 1
namespace Integrator
{
PolymerDegradation::PolymerDegradation():
	Integrator()
{
	//Util::Message(INFO);
	//
	// READ INPUT PARAMETERS
	//

	// ---------------------------------------------------------------------
	// --------------------- Water diffusion -------------------------------
	// ---------------------------------------------------------------------
	amrex::ParmParse pp_water("water");
	pp_water.query("on",water.on);
	if(water.on)
	{
		pp_water.query("diffusivity", water.diffusivity);
		pp_water.query("refinement_threshold", water.refinement_threshold);
		pp_water.query("ic_type", water.ic_type);

		// // Determine initial condition
		if (water.ic_type == "constant")
		{
			amrex::ParmParse pp_water_ic("water.ic");
			std::vector<amrex::Real> value;
			pp_water_ic.queryarr("value",value);
			water.ic = new IC::Constant(geom,value);
		}
		else
			Util::Abort(INFO, "This kind of IC has not been implemented yet");

		amrex::ParmParse pp_water_bc("water.bc");

		amrex::Vector<std::string> bc_hi_str(AMREX_SPACEDIM);
		amrex::Vector<std::string> bc_lo_str(AMREX_SPACEDIM);

		pp_water_bc.queryarr("lo",bc_lo_str,0,AMREX_SPACEDIM);
		pp_water_bc.queryarr("hi",bc_hi_str,0,AMREX_SPACEDIM);
		amrex::Vector<amrex::Real> bc_lo_1, bc_hi_1;
		amrex::Vector<amrex::Real> bc_lo_2, bc_hi_2;
		amrex::Vector<amrex::Real> bc_lo_3, bc_hi_3;

		if (pp_water_bc.countval("lo_1")) pp_water_bc.getarr("lo_1",bc_lo_1);
		if (pp_water_bc.countval("hi_1")) pp_water_bc.getarr("hi_1",bc_hi_1);
		if (pp_water_bc.countval("lo_2")) pp_water_bc.getarr("lo_2",bc_lo_2);
		if (pp_water_bc.countval("hi_2")) pp_water_bc.getarr("hi_2",bc_hi_2);
		if (pp_water_bc.countval("lo_3")) pp_water_bc.getarr("lo_3",bc_lo_3);
		if (pp_water_bc.countval("hi_3")) pp_water_bc.getarr("hi_3",bc_hi_3);

		water.bc = new BC::Constant(bc_hi_str, bc_lo_str
							  ,AMREX_D_DECL(bc_lo_1, bc_lo_2, bc_lo_3)
							  ,AMREX_D_DECL(bc_hi_1, bc_hi_2, bc_hi_3)
							  );

		RegisterNewFab(water_conc,     water.bc, 1, number_of_ghost_cells, "Water Concentration");
		RegisterNewFab(water_conc_old, water.bc, 1, number_of_ghost_cells, "Water Concentration Old");
	}

	// ---------------------------------------------------------------------
	// --------------------- Heat diffusion -------------------------------
	// ---------------------------------------------------------------------
	amrex::ParmParse pp_heat("thermal");
	pp_heat.query("on",thermal.on);
	if(thermal.on)
	{
		pp_heat.query("diffusivity", thermal.diffusivity);
		pp_heat.query("refinement_threshold",thermal.refinement_threshold);
		pp_heat.query("ic_type",thermal.ic_type);

		if (thermal.ic_type == "constant")
		{
			amrex::ParmParse pp_heat_ic("thermal.ic");
			amrex::Vector<amrex::Real> T;
			pp_heat_ic.queryarr("value",T);
			thermal.ic = new IC::Constant(geom,T);
		}
		else
			Util::Abort(INFO, "This kind of IC has not been implemented yet");

		amrex::ParmParse pp_heat_bc("thermal.bc");

		amrex::Vector<std::string> bc_hi_str(AMREX_SPACEDIM);
		amrex::Vector<std::string> bc_lo_str(AMREX_SPACEDIM);

		pp_heat_bc.queryarr("lo",bc_lo_str,0,AMREX_SPACEDIM);
		pp_heat_bc.queryarr("hi",bc_hi_str,0,AMREX_SPACEDIM);
		amrex::Vector<amrex::Real> bc_lo_1, bc_hi_1;
		amrex::Vector<amrex::Real> bc_lo_2, bc_hi_2;
		amrex::Vector<amrex::Real> bc_lo_3, bc_hi_3;

		if (pp_heat_bc.countval("lo_1")) pp_heat_bc.getarr("lo_1",bc_lo_1);
		if (pp_heat_bc.countval("hi_1")) pp_heat_bc.getarr("hi_1",bc_hi_1);
		if (pp_heat_bc.countval("lo_2")) pp_heat_bc.getarr("lo_2",bc_lo_2);
		if (pp_heat_bc.countval("hi_2")) pp_heat_bc.getarr("hi_2",bc_hi_2);
		if (pp_heat_bc.countval("lo_3")) pp_heat_bc.getarr("lo_3",bc_lo_3);
		if (pp_heat_bc.countval("hi_3")) pp_heat_bc.getarr("hi_3",bc_hi_3);

		thermal.bc = new BC::Constant(bc_hi_str, bc_lo_str
								,AMREX_D_DECL(bc_lo_1, bc_lo_2, bc_lo_3)
							  	,AMREX_D_DECL(bc_hi_1, bc_hi_2, bc_hi_3)
						);
	}
	else
		thermal.bc = new BC::Nothing();

	RegisterNewFab(Temp,     thermal.bc, 1, number_of_ghost_cells, "Temperature");
	RegisterNewFab(Temp_old, thermal.bc, 1, number_of_ghost_cells, "Temperature Old");

	// ---------------------------------------------------------------------
	// --------------------- Material model --------------------------------
	// ---------------------------------------------------------------------
	amrex::ParmParse pp_material("material");
	pp_material.query("model",input_material);
	
	if(input_material == "isotropic2")
	{
		Set::Scalar E1 = 1.0, E2 = 0.5, Tg = 273.0+42.0, Ts = 17.0, temp = 298.0;
		Set::Scalar nu = 0.3;
		amrex::ParmParse pp_material_isotropic2("material.isotropic2");
		pp_material_isotropic2.query("E10", E1);
		pp_material_isotropic2.query("E20", E2);
		pp_material_isotropic2.query("Tg", Tg);
		pp_material_isotropic2.query("Ts", Ts);
		pp_material_isotropic2.query("nu",nu);
		pp_material_isotropic2.query("temp",temp);
		if(E1 <= 0) {Util::Warning(INFO, "E1 must be positive. Restting to default"); E1 = 1.0;} 
		if(E2 <= 0) {Util::Warning(INFO, "E2 must be positive. Restting to default"); E2 = 0.5;} 
		if(Tg <= 0) {Util::Warning(INFO, "Tg must be positive. Restting to default"); Tg = 319.0;} 
		if(Ts <= 0) {Util::Warning(INFO, "Ts must be positive. Restting to default"); Ts = 17.0;}
		if(temp <= 0) {Util::Warning(INFO, "temp must be positive. Restting to default"); temp = 298.0;}  
		//Util::Abort(INFO,"Isotropic2 model has been disabled for now");
		modeltype = new model_type(E1,E2,Tg,Ts,nu,temp);
	}
	else if(input_material == "isotropic")
	{
		//Util::Abort(INFO, "Check for model_type in PD.H");
		Set::Scalar lambda = 410.0;
		Set::Scalar mu = 305.0;
		Set::Scalar yield = 18.0;
		amrex::ParmParse pp_material_isotropic("material.isotropic");
		pp_material_isotropic.query("lambda",lambda);
		pp_material_isotropic.query("mu",mu);
		pp_material_isotropic.query("yield",yield);
		if(lambda <=0) { Util::Warning(INFO,"Lambda must be positive. Resetting back to default value"); lambda = 410.0; }
		if(mu <= 0) { Util::Warning(INFO,"Mu must be positive. Resetting back to default value"); mu = 305.0; }
		if(yield <= 0) { Util::Warning(INFO, "Yield must be positive. Resetting back to default value"); yield = 18.0; }
		modeltype = new model_type(lambda,mu);
	}
	else
		Util::Abort(INFO, "Not implemented yet");

	// ---------------------------------------------------------------------
	// --------------------- Damage model ----------------------------------
	// ---------------------------------------------------------------------
	amrex::ParmParse pp_damage("damage"); // Phase-field model parameters
	pp_damage.query("anisotropy",damage.anisotropy);

	if(damage.anisotropy == 0)
		damage.number_of_eta = 1;
	else
		damage.number_of_eta = AMREX_SPACEDIM;

	pp_damage.query("type",damage.type);

	if(damage.type == "water" || damage.type == "water2") 
	{
		if(damage.type == "water") damage.number_of_eta = 2;
		else damage.number_of_eta = 4;

		damage.anisotropy = 0;

		damage.d_final.resize(damage.number_of_eta);
		damage.d_i.resize(damage.number_of_eta);
		damage.tau_i.resize(damage.number_of_eta);
		damage.t_start_i.resize(damage.number_of_eta);
		damage.number_of_terms.resize(damage.number_of_eta);

		amrex::Vector<Set::Scalar> dfinal;
		pp_damage.queryarr("d_final",dfinal);

		if(dfinal.size()!=damage.number_of_eta) Util::Abort(INFO, "Incorrect size of d_final");
		for (int i = 0; i < damage.number_of_eta; i++)
		{
			if(dfinal[i] <0 || dfinal[i] > 1.0) Util::Abort(INFO,"Incorrect value of d_final");
			damage.d_final[i] = dfinal[i];
		}

		int totalnumberofterms = 0, tempIndex = 0, tempIndex2 = 0;
		amrex::Vector<int> numberofterms;
		pp_damage.queryarr("number_of_terms",numberofterms);
		if(numberofterms.size()!=damage.number_of_eta) Util::Abort(INFO, "Incorrect size of number_of_terms");
		for (int i = 0; i<damage.number_of_eta; i++)
		{
			if(numberofterms[i] < 1) {Util::Abort(INFO, "number_of_terms can not be less than 1. Resetting"); numberofterms[i]=1;}
			damage.number_of_terms[i] = numberofterms[i];
			totalnumberofterms += numberofterms[i];
		}

		amrex::Vector<Set::Scalar> di, taui, tstarti;
		Set::Scalar sum = 0.;
		pp_damage.queryarr("d_i",di);
		pp_damage.queryarr("tau_i",taui);
		pp_damage.queryarr("t_start_i",tstarti);
		if(di.size()!=totalnumberofterms || taui.size()!=totalnumberofterms || tstarti.size()!=totalnumberofterms) Util::Abort(INFO,"Incorrect number of terms in di, taui or tstarti");
		for (int i=0; i<totalnumberofterms; i++)
		{
			if(tempIndex2 == damage.number_of_terms[tempIndex]) 
			{
				if(sum != 1.0) Util::Abort(INFO, "d_i's don't add to 1");
				tempIndex2 = 0; tempIndex+=1; sum = 0.;
			}
			
			if(di[i] < 0. || di[i] > 1.) Util::Abort(INFO, "Invalid values of d_i. Must be between 0 and 1");
			if(taui[i] < 0.) Util::Abort(INFO,"Invalid values of tau");

			sum += di[i];
			damage.d_i[tempIndex].push_back(di[i]);
			damage.tau_i[tempIndex].push_back(taui[i]);
			damage.t_start_i[tempIndex].push_back(tstarti[i]);

			tempIndex2 += 1;
		}
		/*for (int i = 0; i < damage.number_of_eta; i++)
		{
			Util::Message(INFO, "number of terms = ", damage.number_of_terms[i]);
			Util::Message(INFO, "d_final = ", damage.d_final[i]);
			for (int j = 0; j < damage.number_of_terms[i]; j++)
			{
				Util::Message(INFO, "d_i[", i, "][", j, "]=",damage.d_i[i][j]);
				Util::Message(INFO, "tau_i[", i, "][", j, "]=",damage.tau_i[i][j]);
				Util::Message(INFO, "t_start_i[", i, "][", j, "]=",damage.t_start_i[i][j]);
			}
		}*/
	}
	else
		Util::Abort(INFO, "This kind of damage model has not been implemented yet");

	pp_damage.query("ic_type",damage.ic_type);
	pp_damage.query("refinement_threshold",damage.refinement_threshold);
	if(damage.ic_type == "constant")
	{
		amrex::ParmParse pp_damage_ic("damage.ic");
		amrex::Vector<amrex::Real> eta_init;
		pp_damage_ic.queryarr("value",eta_init);
		damage.ic = new IC::Constant(geom,eta_init);
	}
	else
		Util::Abort(INFO, "This kind of IC has not been implemented yet");

	amrex::ParmParse pp_damage_bc("damage.bc");

	amrex::Vector<std::string> bc_hi_str(AMREX_SPACEDIM);
	amrex::Vector<std::string> bc_lo_str(AMREX_SPACEDIM);

	pp_damage_bc.queryarr("lo",bc_lo_str,0,BL_SPACEDIM);
	pp_damage_bc.queryarr("hi",bc_hi_str,0,BL_SPACEDIM);
	amrex::Vector<amrex::Real> bc_lo_1, bc_hi_1;
	amrex::Vector<amrex::Real> bc_lo_2, bc_hi_2;
	amrex::Vector<amrex::Real> bc_lo_3, bc_hi_3;

	if (pp_damage_bc.countval("lo_1")) pp_damage_bc.getarr("lo_1",bc_lo_1);
	if (pp_damage_bc.countval("hi_1")) pp_damage_bc.getarr("hi_1",bc_hi_1);

	if (pp_damage_bc.countval("lo_2")) pp_damage_bc.getarr("lo_2",bc_lo_2);
	if (pp_damage_bc.countval("hi_2")) pp_damage_bc.getarr("hi_2",bc_hi_2);

	if (pp_damage_bc.countval("lo_3")) pp_damage_bc.getarr("lo_3",bc_lo_3);
	if (pp_damage_bc.countval("hi_3")) pp_damage_bc.getarr("hi_3",bc_hi_3);

	damage.bc = new BC::Constant(bc_hi_str, bc_lo_str
				  ,AMREX_D_DECL(bc_lo_1, bc_lo_2, bc_lo_3)
				  ,AMREX_D_DECL(bc_hi_1, bc_hi_2, bc_hi_3));

	RegisterNewFab(eta_new, damage.bc, damage.number_of_eta, number_of_ghost_cells, "Eta");
	RegisterNewFab(eta_old, damage.bc, damage.number_of_eta, number_of_ghost_cells, "Eta old");
	RegisterNewFab(damage_start_time,damage.bc,1,2,"Start time");

	//std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	// ---------------------------------------------------------------------
	// --------------------- Elasticity parameters -------------------------
	// ---------------------------------------------------------------------
	amrex::ParmParse pp_elastic("elastic");
	pp_elastic.query("on",elastic.on);
	if(elastic.on)
	{
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

		
		amrex::Vector<Set::Scalar> bc_lo_1, bc_hi_1;
		amrex::Vector<Set::Scalar> bc_lo_2, bc_hi_2;
		amrex::Vector<Set::Scalar> bc_lo_3, bc_hi_3;
		amrex::Vector<Set::Scalar> bc_lo_1_t, bc_hi_1_t;
		amrex::Vector<Set::Scalar> bc_lo_2_t, bc_hi_2_t;
		amrex::Vector<Set::Scalar> bc_lo_3_t, bc_hi_3_t;

		amrex::ParmParse pp_temp;
		Set::Scalar stop_time, timestep;
		pp_temp.query("timestep",timestep);
		pp_temp.query("stop_time",stop_time);

		if(elastic.type == "tensile_test" || elastic.type == "tensile")
		{
			/*AMREX_D_TERM( 	bc_x_lo_str = {AMREX_D_DECL("disp", "neumann", "neumann")};
							bc_x_hi_str = {AMREX_D_DECL("disp", "trac", "trac")};
							,
							bc_y_lo_str = {AMREX_D_DECL("neumann", "disp", "neumann")};
							bc_y_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};
							,
							bc_z_lo_str = {AMREX_D_DECL("neumann", "neumann", "disp")};
							bc_z_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};);*/

			AMREX_D_TERM( 	bc_x_lo_str = {AMREX_D_DECL("disp", "trac", "trac")};
							bc_x_hi_str = {AMREX_D_DECL("disp", "trac", "trac")};
							,
							bc_y_lo_str = {AMREX_D_DECL("trac", "trac", "trac")};
							bc_y_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};
							,
							bc_z_lo_str = {AMREX_D_DECL("trac", "trac", "trac")};
							bc_z_hi_str = {AMREX_D_DECL("trac", "trac", "trac")};);

			AMREX_D_TERM(	elastic.bc_xlo = {AMREX_D_DECL(bc_map[bc_x_lo_str[0]],bc_map[bc_x_lo_str[1]],bc_map[bc_x_lo_str[2]])};
							elastic.bc_xhi = {AMREX_D_DECL(bc_map[bc_x_hi_str[0]],bc_map[bc_x_hi_str[1]],bc_map[bc_x_hi_str[2]])};
							,
							elastic.bc_ylo = {AMREX_D_DECL(bc_map[bc_y_lo_str[0]],bc_map[bc_y_lo_str[1]],bc_map[bc_y_lo_str[2]])};
							elastic.bc_yhi = {AMREX_D_DECL(bc_map[bc_y_hi_str[0]],bc_map[bc_y_hi_str[1]],bc_map[bc_y_hi_str[2]])};
							,
							elastic.bc_zlo = {AMREX_D_DECL(bc_map[bc_z_lo_str[0]],bc_map[bc_z_lo_str[1]],bc_map[bc_z_lo_str[2]])};
							elastic.bc_zhi = {AMREX_D_DECL(bc_map[bc_z_hi_str[0]],bc_map[bc_z_hi_str[1]],bc_map[bc_z_hi_str[2]])};);
			pp_elastic_bc.query("rate",elastic.test_rate);
			if(elastic.test_rate < 0.) { Util::Warning(INFO,"Rate can't be less than zero. Resetting to 1.0"); elastic.test_rate = 1.0; }
			AMREX_D_TERM(	elastic.bc_left = Set::Vector(AMREX_D_DECL(0.,0.,0.));
							elastic.bc_right = Set::Vector(AMREX_D_DECL(0.,0.,0.));
							,
							elastic.bc_bottom = Set::Vector(AMREX_D_DECL(0.,0.,0.));
							elastic.bc_top = Set::Vector(AMREX_D_DECL(0.,0.,0.));
							,
							elastic.bc_back = Set::Vector(AMREX_D_DECL(0.,0.,0.));
							elastic.bc_front = Set::Vector(AMREX_D_DECL(0.,0.,0.));
			);
			pp_elastic_bc.queryarr("test_time",elastic.test_time);
			if(elastic.test_time.size() < 1){Util::Warning(INFO,"No test time provided. Providing default value"); elastic.test_time={0.,stop_time};}
			std::sort(elastic.test_time.begin(),elastic.test_time.end());
			pp_elastic_bc.query("test_duration",elastic.test_duration);
			if(elastic.test_duration < 0.) { Util::Warning(INFO,"Test duration must be positive. Resetting it to 2.0"); elastic.test_duration = 1.0; }
			pp_elastic_bc.query("test_dt",elastic.test_dt);
			if(elastic.test_dt < 0.) { Util::Warning(INFO,"Test dt must be positive. Resetting it to 0.01"); elastic.test_duration = 0.01; }
		}

		else if(elastic.type == "tensile_single" || elastic.type == "single")
		{
			pp_elastic.query("tstart",elastic.tstart);
			if(elastic.tstart < 0.0)
			{
				Util::Warning(INFO,"Invalid value for elasitc t_start (",elastic.tstart,"). Setting it to zero");
				elastic.tstart = 0.0;
			}
			else if(elastic.tstart > stop_time)
			{
				Util::Warning(INFO,"Invalid value for elastic t_start (",elastic.tstart,"). Setting it to stop_time");
				elastic.tstart = stop_time;
			}

			pp_elastic.query("tend",elastic.tend);
			if(elastic.tend < elastic.tstart || elastic.tend > stop_time)
			{			
				Util::Warning(INFO,"Invalid value for elastic t_end (",elastic.tend,"). Setting it to stop_time");
				elastic.tend = stop_time;
				if(elastic.tstart == stop_time) elastic.tstart = stop_time - timestep;
			}
			AMREX_D_TERM(	pp_elastic_bc.queryarr("bc_x_min",bc_x_lo_str);,
							pp_elastic_bc.queryarr("bc_y_min",bc_y_lo_str);,
							pp_elastic_bc.queryarr("bc_z_min",bc_z_lo_str);
			);

			AMREX_D_TERM(	pp_elastic_bc.queryarr("bc_x_max",bc_x_hi_str);,
							pp_elastic_bc.queryarr("bc_y_max",bc_y_hi_str);,
							pp_elastic_bc.queryarr("bc_z_max",bc_z_hi_str);
			);

			AMREX_D_TERM(	elastic.bc_xlo = {AMREX_D_DECL(bc_map[bc_x_lo_str[0]],bc_map[bc_x_lo_str[1]],bc_map[bc_x_lo_str[2]])};
							elastic.bc_xhi = {AMREX_D_DECL(bc_map[bc_x_hi_str[0]],bc_map[bc_x_hi_str[1]],bc_map[bc_x_hi_str[2]])};
							,
							elastic.bc_ylo = {AMREX_D_DECL(bc_map[bc_y_lo_str[0]],bc_map[bc_y_lo_str[1]],bc_map[bc_y_lo_str[2]])};
							elastic.bc_yhi = {AMREX_D_DECL(bc_map[bc_y_hi_str[0]],bc_map[bc_y_hi_str[1]],bc_map[bc_y_hi_str[2]])};
							,
							elastic.bc_zlo = {AMREX_D_DECL(bc_map[bc_z_lo_str[0]],bc_map[bc_z_lo_str[1]],bc_map[bc_z_lo_str[2]])};
							elastic.bc_zhi = {AMREX_D_DECL(bc_map[bc_z_hi_str[0]],bc_map[bc_z_hi_str[1]],bc_map[bc_z_hi_str[2]])};
			);

			AMREX_D_TERM(	if (pp_elastic_bc.countval("x_min")) pp_elastic_bc.getarr("x_min",bc_lo_1);
							if(bc_lo_1.size() % AMREX_SPACEDIM !=0) Util::Abort(INFO, "Invalid number of values for left_face displacement");
							elastic.bc_left = Set::Vector(AMREX_D_DECL(bc_lo_1[0],bc_lo_1[1],bc_lo_1[2]));

							if (pp_elastic_bc.countval("x_max")) pp_elastic_bc.getarr("x_max",bc_hi_1);
							if(bc_hi_1.size() % AMREX_SPACEDIM !=0) Util::Abort(INFO, "Invalid number of values for right_face displacement");
							elastic.bc_right = Set::Vector(AMREX_D_DECL(bc_hi_1[0],bc_hi_1[1],bc_hi_1[2]));
							,
							if (pp_elastic_bc.countval("y_min")) pp_elastic_bc.getarr("y_min",bc_lo_2);
							if(bc_lo_2.size() % AMREX_SPACEDIM !=0) Util::Abort(INFO, "Invalid number of values for bottom_face displacement");
							elastic.bc_bottom = Set::Vector(AMREX_D_DECL(bc_lo_2[0],bc_lo_2[1],bc_lo_2[2]));

							if (pp_elastic_bc.countval("y_max")) pp_elastic_bc.getarr("y_max",bc_hi_2);
							if(bc_hi_2.size() % AMREX_SPACEDIM !=0) Util::Abort(INFO, "Invalid number of values for top_face displacement");
							elastic.bc_top = Set::Vector(AMREX_D_DECL(bc_hi_2[0],bc_hi_2[1],bc_hi_2[2]));
							,
							if (pp_elastic_bc.countval("z_min")) pp_elastic_bc.getarr("z_min",bc_lo_3);
							if(bc_lo_3.size() % AMREX_SPACEDIM !=0)	Util::Abort(INFO, "Invalid number of values for back_face displacement");
							elastic.bc_back = Set::Vector(AMREX_D_DECL(bc_lo_3[0],bc_lo_3[1],bc_lo_3[2]));

							if (pp_elastic_bc.countval("z_max")) pp_elastic_bc.getarr("z_max",bc_hi_3);
							if(bc_hi_3.size() % AMREX_SPACEDIM !=0) Util::Abort(INFO, "Invalid number of values for front_face displacement");
							elastic.bc_front = Set::Vector(AMREX_D_DECL(bc_hi_3[0],bc_hi_3[1],bc_hi_3[2]));
			);
		}
		else
			Util::Abort(INFO, "Not implemented this type of test yet");
		
		//Util::Message(INFO);
		/*int tempSize = bc_lo_1.size()/AMREX_SPACEDIM;
		for (int i = 0; i<tempSize; i++)
			elastic.bc_left.push_back(Set::Vector(AMREX_D_DECL(bc_lo_1[AMREX_SPACEDIM*i],bc_lo_1[AMREX_SPACEDIM*i+1],bc_lo_1[AMREX_SPACEDIM*i+2])));
		if(pp_elastic_bc.countval("left_face_t")) pp_elastic_bc.getarr("left_face_t",bc_lo_1_t);
		if(bc_lo_1_t.size() == tempSize)
			elastic.bc_left_t = bc_lo_1_t;
		else
			for (int j = 0; j < tempSize; j++)
				elastic.bc_left_t.push_back(elastic.tstart + j*(elastic.tend - elastic.tstart)/(tempSize-1.0 != 0.0 ? tempSize-1.0 : 1.0));

		tempSize = bc_hi_1.size()/AMREX_SPACEDIM;
		for (int i = 0; i<tempSize; i++)
			elastic.bc_right.push_back(Set::Vector(AMREX_D_DECL(bc_hi_1[AMREX_SPACEDIM*i],bc_hi_1[AMREX_SPACEDIM*i+1],bc_hi_1[AMREX_SPACEDIM*i+2])));
		if(pp_elastic_bc.countval("right_face_t")) pp_elastic_bc.getarr("right_face_t",bc_hi_1_t);
		if(bc_hi_1_t.size() == tempSize)
			elastic.bc_right_t = bc_hi_1_t;
		else
			for (int j = 0; j < tempSize; j++)
				elastic.bc_right_t.push_back(elastic.tstart + j*(elastic.tend - elastic.tstart)/(tempSize-1.0 != 0.0 ? tempSize-1.0 : 1.0));

#if AMREX_SPACEDIM > 1
		tempSize = bc_lo_2.size()/AMREX_SPACEDIM;
		for (int i = 0; i<tempSize; i++)
			elastic.bc_bottom.push_back(Set::Vector(AMREX_D_DECL(bc_lo_2[AMREX_SPACEDIM*i],bc_lo_2[AMREX_SPACEDIM*i+1],bc_lo_2[AMREX_SPACEDIM*i+2])));
		if(pp_elastic_bc.countval("bottom_face_t")) pp_elastic_bc.getarr("bottom_face_t",bc_lo_2_t);
		if(bc_lo_2_t.size() == tempSize)
			elastic.bc_bottom_t = bc_lo_2_t;
		else
			for (int j = 0; j < tempSize; j++)
				elastic.bc_bottom_t.push_back(elastic.tstart + j*(elastic.tend - elastic.tstart)/(tempSize-1.0 != 0.0 ? tempSize-1.0 : 1.0));

		tempSize = bc_hi_2.size()/AMREX_SPACEDIM;
		for (int i = 0; i<tempSize; i++)
			elastic.bc_top.push_back(Set::Vector(AMREX_D_DECL(bc_hi_2[AMREX_SPACEDIM*i],bc_hi_2[AMREX_SPACEDIM*i+1],bc_hi_2[AMREX_SPACEDIM*i+2])));
		if(pp_elastic_bc.countval("top_face_t")) pp_elastic_bc.getarr("top_face_t",bc_hi_2_t);
		if(bc_hi_2_t.size() == tempSize)
			elastic.bc_top_t = bc_hi_2_t;
		else
			for (int j = 0; j < tempSize; j++)
				elastic.bc_top_t.push_back(elastic.tstart + j*(elastic.tend - elastic.tstart)/(tempSize-1.0 != 0.0 ? tempSize-1.0 : 1.0));

#if AMREX_SPACEDIM > 2
		tempSize = bc_lo_3.size()/AMREX_SPACEDIM;
		for (int i = 0; i<tempSize; i++)
			elastic.bc_back.push_back(Set::Vector(AMREX_D_DECL(bc_lo_3[AMREX_SPACEDIM*i],bc_lo_3[AMREX_SPACEDIM*i+1],bc_lo_3[AMREX_SPACEDIM*i+2])));
		if(pp_elastic_bc.countval("back_face_t")) pp_elastic_bc.getarr("back_face_t",bc_lo_3_t);
		if(bc_lo_3_t.size() == tempSize)
			elastic.bc_back_t = bc_lo_3_t;
		else
			for (int j = 0; j < tempSize; j++)
				elastic.bc_back_t.push_back(elastic.tstart + j*(elastic.tend - elastic.tstart)/(tempSize-1.0 != 0.0 ? tempSize-1.0 : 1.0));

		tempSize = bc_hi_3.size()/AMREX_SPACEDIM;
		for (int i = 0; i<tempSize; i++)
			elastic.bc_front.push_back(Set::Vector(AMREX_D_DECL(bc_hi_3[AMREX_SPACEDIM*i],bc_hi_3[AMREX_SPACEDIM*i+1],bc_hi_3[AMREX_SPACEDIM*i+2])));
		if(pp_elastic_bc.countval("front_face_t")) pp_elastic_bc.getarr("front_face_t",bc_hi_3_t);
		if(bc_hi_3_t.size() == tempSize)
			elastic.bc_front_t = bc_hi_3_t;
		else
			for (int j = 0; j < tempSize; j++)
				elastic.bc_front_t.push_back(elastic.tstart + j*(elastic.tend - elastic.tstart)/(tempSize-1.0 != 0.0 ? tempSize-1.0 : 1.0));
#endif
#endif*/
		//----------------------------------------------------------------------
		// The following routine should be replaced by RegisterNewFab in the
		// future. For now, we are manually defining and resizing
		//-----------------------------------------------------------------------

		const int number_of_stress_components = AMREX_SPACEDIM*AMREX_SPACEDIM;
		RegisterNodalFab (displacement,	AMREX_SPACEDIM,					2,	"displacement");;
		RegisterNodalFab (rhs,			AMREX_SPACEDIM,					2,	"rhs");;
		RegisterNodalFab (strain,		number_of_stress_components,	2,	"strain");;
		RegisterNodalFab (stress,		number_of_stress_components,	2,	"stress");;
		RegisterNodalFab (stress_vm,	1,								2,	"stress_vm");;
		RegisterNodalFab (energy,		1,								2,	"energy");;
		RegisterNodalFab (residual,		AMREX_SPACEDIM,					2,	"residual");;

	}

	//
	// Boundary condition interpolators
	//
	/*AMREX_D_TERM(	interpolate_left  .define(elastic.bc_left,elastic.bc_left_t);
					interpolate_right .define(elastic.bc_right,elastic.bc_right_t);
					,
					interpolate_bottom.define(elastic.bc_bottom,elastic.bc_bottom_t);
					interpolate_top   .define(elastic.bc_top,elastic.bc_top_t);
					,
					interpolate_back  .define(elastic.bc_back,elastic.bc_back_t);
					interpolate_front .define(elastic.bc_front,elastic.bc_front_t););*/

	nlevels = maxLevel() + 1;
}


void
PolymerDegradation::Advance (int lev, amrex::Real time, amrex::Real dt)
{
	Util::Message(INFO, "Enter");
	std::swap(*eta_old[lev], 	*eta_new[lev]);
	//	if (elastic.on) if (rhs[lev]->contains_nan()) Util::Abort(INFO);

	if(water.on) std::swap(*water_conc_old[lev],*water_conc[lev]);
	if(thermal.on) std::swap(*Temp_old[lev], *Temp[lev]);

	static amrex::IntVect AMREX_D_DECL(	dx(AMREX_D_DECL(1,0,0)),
						dy(AMREX_D_DECL(0,1,0)),
						dz(AMREX_D_DECL(0,0,1)));
	const amrex::Real* DX = geom[lev].CellSize();

	if(water.on)
	{
		Util::Message(INFO);
		for ( amrex::MFIter mfi(*water_conc[lev],true); mfi.isValid(); ++mfi )
		{
			const amrex::Box& bx = mfi.validbox();
			amrex::Array4<amrex::Real> const& water_old_box = (*water_conc_old[lev]).array(mfi);
			amrex::Array4<amrex::Real> const& water_box = (*water_conc[lev]).array(mfi);
			amrex::Array4<amrex::Real> const& time_box = (*damage_start_time[lev]).array(mfi);

			amrex::ParallelFor (bx,[=] AMREX_GPU_DEVICE(int i, int j, int k){
				if(std::isnan(water_old_box(i,j,k,0))) Util::Abort(INFO, "Nan found in WATER_OLD(i,j,k)");
				if(std::isinf(water_old_box(i,j,k,0))) Util::Abort(INFO, "Nan found in WATER_OLD(i,j,k)");
				if(water_old_box(i,j,k,0) > 1.0)
				{
					Util::Warning(INFO,"Water concentration exceeded 1 at (", i, ",", j, ",", "k) and lev = ", lev, " Resetting");
					water_old_box(i,j,k,0) = 1.0;
				}
				water_box(i,j,k,0) =
					water_old_box(i,j,k,0)
					+ dt * water.diffusivity * (
						AMREX_D_TERM((water_old_box(i+1,j,k,0) + water_old_box(i-1,j,k,0) - 2.0*water_old_box(i,j,k,0)) / DX[0] / DX[0],
									+ (water_old_box(i,j+1,k,0) + water_old_box(i,j-1,k,0) - 2.0*water_old_box(i,j,k,0)) / DX[1] / DX[1],
									+ (water_old_box(i,j,k+1,0) + water_old_box(i,j,k-1,0) - 2.0*water_old_box(i,j,k,0)) / DX[2] / DX[2])
							);
				if(water_box(i,j,k,0) > 1.0)
				{
					Util::Warning(INFO, "Water concentration has exceeded one after computation. Resetting it to one");
					water_box(i,j,k,0) = 1.0;
				}
				if(water_old_box(i,j,k,0) < 1.E-2 && water_box(i,j,k,0) > 1.E-2)
					time_box(i,j,k,0) = time;
			});
		}
		Util::Message(INFO);
	}

	if(thermal.on)
	{
		for ( amrex::MFIter mfi(*Temp[lev],true); mfi.isValid(); ++mfi )
		{
			const amrex::Box& bx = mfi.validbox();
			amrex::Array4<const amrex::Real> const& Temp_old_box = (*Temp_old[lev]).array(mfi);
			amrex::Array4<amrex::Real> const& Temp_box = (*Temp[lev]).array(mfi);
			
			amrex::ParallelFor (bx,[=] AMREX_GPU_DEVICE(int i, int j, int k){
				Temp_box(i,j,k,0) =
						Temp_old_box(i,j,k,0)
						+ dt * thermal.diffusivity * (AMREX_D_TERM((Temp_old_box(i+1,j,k,0) + Temp_old_box(i-1,j,k,0) - 2.0*Temp_old_box(i,j,k,0)) / DX[0] / DX[0],
						+ (Temp_old_box(i,j+1,k,0) + Temp_old_box(i,j-1,k,0) - 2.0*Temp_old_box(i,j,k,0)) / DX[1] / DX[1],
						+ (Temp_old_box(i,j,k+1,0) + Temp_old_box(i,j,k-1,0) - 2.0*Temp_old_box(i,j,k,0)) / DX[2] / DX[2]));
			});
		}
	}
	Util::Message(INFO);
	for ( amrex::MFIter mfi(*eta_new[lev],true); mfi.isValid(); ++mfi )
	{
		const amrex::Box& bx = mfi.validbox();
		amrex::Array4<amrex::Real> const& eta_new_box     		= (*eta_new[lev]).array(mfi);
		amrex::Array4<const amrex::Real> const& eta_old_box     = (*eta_old[lev]).array(mfi);
		amrex::Array4<const amrex::Real> const& water_box 		= (*water_conc[lev]).array(mfi);
		amrex::Array4<const amrex::Real> const& time_box 		= (*damage_start_time[lev]).array(mfi);
		//amrex::Array4<const amrex::Real> const& Temp_box 		= (*Temp[lev]).array(mfi);
		//Util::Message(INFO);

		amrex::ParallelFor (bx,[=] AMREX_GPU_DEVICE(int i, int j, int k){
			for (int n = 0; n < damage.number_of_eta; n++)
			{
				if(damage.type == "water" || damage.type == "water2")
				{
					Set::Scalar temp1 = 0.0;
					if(water_box(i,j,k,0) > 0.0 && eta_old_box(i,j,k,n) < damage.d_final[n])
					{
						for (int l = 0; l < damage.number_of_terms[n]; l++)
								temp1 += damage.d_final[n]*damage.d_i[n][l]*water_box(i,j,k,0)*std::exp(-std::max(0.0,time-time_box(i,j,k,0)-damage.t_start_i[n][l])/damage.tau_i[n][l])/(damage.tau_i[n][l]);
					}
					eta_new_box(i,j,k,n) = eta_old_box(i,j,k,n) + temp1*dt;
					if(eta_new_box(i,j,k,n) > damage.d_final[n])
						Util::Abort(INFO, "eta exceeded ",damage.d_final[n], ". Rhs = ", temp1, ", Water = ", water_box(i,j,k,n));
				}
				else
					Util::Abort(INFO, "Damage model not implemented yet");
			}
		});
	}
	//if(elastic.on)	if (rhs[lev]->contains_nan()) Util::Abort(INFO);
	Util::Message(INFO,"Exit");
}

void
PolymerDegradation::Initialize (int lev)
{
	Util::Message(INFO);
	if(water.on)
	{
		water.ic->Initialize(lev,water_conc);
		water.ic->Initialize(lev,water_conc_old);
		IC::IC *time_ic;
		time_ic = new IC::Constant(geom,{0.0});
		time_ic->Initialize(lev,damage_start_time);
	}

	if(thermal.on)
	{
		thermal.ic->Initialize(lev,Temp);
		thermal.ic->Initialize(lev,Temp_old);
	}

	Util::Message(INFO);
	damage.ic->Initialize(lev,eta_new);
	damage.ic->Initialize(lev,eta_old);
	
	if (elastic.on)
	{
		displacement[lev]->setVal(0.0);
		strain[lev]->setVal(0.0);
		stress[lev]->setVal(0.0);
		stress_vm[lev]->setVal(0.0);
		rhs[lev]->setVal(0.0);
		energy[lev]->setVal(0.0);
		residual[lev]->setVal(0.0);
	}
	Util::Message(INFO);
}

PolymerDegradation::~PolymerDegradation()
{
	delete water.ic;
	delete damage.ic;
	delete water.bc;
	delete damage.bc;
}

#define TEMP_OLD(i,j,k) Temp_old_box(amrex::IntVect(AMREX_D_DECL(i,j,k)))
#define TEMP(i,j,k) Temp_box(amrex::IntVect(AMREX_D_DECL(i,j,k)))
#define WATER_OLD(i,j,k) water_conc_old_box(amrex::IntVect(AMREX_D_DECL(i,j,k)))
#define WATER(i,j,k) water_conc_box(amrex::IntVect(AMREX_D_DECL(i,j,k)))
#define ETA_OLD(i,j,k,m) eta_old_box(amrex::IntVect(AMREX_D_DECL(i,j,k)),m)
#define ETA_NEW(i,j,k,m) eta_new_box(amrex::IntVect(AMREX_D_DECL(i,j,k)),m)
void
PolymerDegradation::TagCellsForRefinement (int lev, amrex::TagBoxArray& tags, amrex::Real /*time*/, int /*ngrow*/)
{
	const amrex::Real* dx      = geom[lev].CellSize();
	Util::Message(INFO);
	amrex::Vector<int>  itags;
	if(water.on)
	{
		for (amrex::MFIter mfi(*water_conc[lev],true); mfi.isValid(); ++mfi)
		{
			const amrex::Box&  bx  = mfi.tilebox();
			amrex::TagBox&     tag  = tags[mfi];

			amrex::FArrayBox &water_conc_box = (*water_conc[lev])[mfi];

			AMREX_D_TERM(	for (int i = bx.loVect()[0]; i<=bx.hiVect()[0]; i++),
							for (int j = bx.loVect()[1]; j<=bx.hiVect()[1]; j++),
							for (int k = bx.loVect()[2]; k<=bx.hiVect()[2]; k++)
						)
				{
					AMREX_D_TERM(	amrex::Real grad1 = (WATER(i+1,j,k) - WATER(i-1,j,k))/(2.*dx[0]);,
									amrex::Real grad2 = (WATER(i,j+1,k) - WATER(i,j-1,k))/(2.*dx[1]);,
									amrex::Real grad3 = (WATER(i,j,k+1) - WATER(i,j,k-1))/(2.*dx[2]););
					amrex::Real grad = sqrt(AMREX_D_TERM(grad1*grad1,
						+ grad2*grad2,
						+ grad3*grad3));

					amrex::Real dr = sqrt(AMREX_D_TERM(dx[0]*dx[0],
						+ dx[1]*dx[1],
						+ dx[2]*dx[2]));

					if (grad*dr > water.refinement_threshold)
						tag(amrex::IntVect(AMREX_D_DECL(i,j,k))) = amrex::TagBox::SET;
				}
		}
	}

	if(thermal.on)
	{
		for (amrex::MFIter mfi(*Temp[lev],true); mfi.isValid(); ++mfi)
		{
			const amrex::Box&  bx  = mfi.tilebox();
			amrex::TagBox&     tag  = tags[mfi];

			amrex::FArrayBox &Temp_box = (*Temp[lev])[mfi];

			AMREX_D_TERM(	for (int i = bx.loVect()[0]; i<=bx.hiVect()[0]; i++),
							for (int j = bx.loVect()[1]; j<=bx.hiVect()[1]; j++),
							for (int k = bx.loVect()[2]; k<=bx.hiVect()[2]; k++)
						)
				{
					AMREX_D_TERM(	amrex::Real grad1 = (TEMP(i+1,j,k) - TEMP(i-1,j,k))/(2.*dx[0]);,
									amrex::Real grad2 = (TEMP(i,j+1,k) - TEMP(i,j-1,k))/(2.*dx[1]);,
									amrex::Real grad3 = (TEMP(i,j,k+1) - TEMP(i,j,k-1))/(2.*dx[2]);
								)
					amrex::Real grad = sqrt(AMREX_D_TERM(grad1*grad1,
						+ grad2*grad2,
						+ grad3*grad3));

					amrex::Real dr = sqrt(AMREX_D_TERM(dx[0]*dx[0],
						+ dx[1]*dx[1],
						+ dx[2]*dx[2]));

					if (grad*dr > thermal.refinement_threshold)
						tag(amrex::IntVect(AMREX_D_DECL(i,j,k))) = amrex::TagBox::SET;
				}
		}
	}


	for (amrex::MFIter mfi(*eta_new[lev],true); mfi.isValid(); ++mfi)
	{
		const amrex::Box&  bx  = mfi.tilebox();
		amrex::TagBox&     tag  = tags[mfi];
		amrex::BaseFab<amrex::Real> &eta_new_box = (*eta_new[lev])[mfi];

		AMREX_D_TERM(		for (int i = bx.loVect()[0]; i<=bx.hiVect()[0]; i++),
							for (int j = bx.loVect()[1]; j<=bx.hiVect()[1]; j++),
							for (int k = bx.loVect()[2]; k<=bx.hiVect()[2]; k++)
					)
			{
				for (int m = 0; m < damage.number_of_eta; m++)
				{
					AMREX_D_TERM(	Set::Scalar gradx = (ETA_NEW(i+1,j,k,m) - ETA_NEW(i-1,j,k,m))/(2.*dx[0]);,
									Set::Scalar grady = (ETA_NEW(i,j+1,k,m) - ETA_NEW(i,j-1,k,m))/(2.*dx[1]);,
									Set::Scalar gradz = (ETA_NEW(i,j,k+1,m) - ETA_NEW(i,j,k-1,m))/(2.*dx[2]);
					)
					Set::Scalar grad = sqrt(AMREX_D_TERM(gradx*gradx, + grady*grady, + gradz*gradz));
					Set::Scalar dr = sqrt(AMREX_D_TERM(dx[0]*dx[0], + dx[1]*dx[1], + dx[2]*dx[2]));

					if(grad*dr > damage.refinement_threshold)
						tag(amrex::IntVect(AMREX_D_DECL(i,j,k))) = amrex::TagBox::SET;
				}
			}

	}
	Util::Message(INFO);
}

void
PolymerDegradation::DegradeMaterial(int lev, amrex::FabArray<amrex::BaseFab<model_type> > &model)
{
	Util::Message(INFO);
	/*
	  This function is supposed to degrade material parameters based on certain
	  damage model.
	  For now we are just using isotropic degradation.
	*/
	if(damage.anisotropy) Util::Abort(__FILE__,"DegradeModulus",__LINE__,"Not implemented yet");

	static amrex::IntVect AMREX_D_DECL(dx(AMREX_D_DECL(1,0,0)),
					   dy(AMREX_D_DECL(0,1,0)),
					   dz(AMREX_D_DECL(0,0,1)));

	for (amrex::MFIter mfi(model,true); mfi.isValid(); ++mfi)
	{
		const amrex::Box& box = mfi.validbox();
		amrex::Array4<const amrex::Real> const& eta_box = (*eta_new[lev]).array(mfi);
		amrex::Array4<model_type> const& modelfab = model.array(mfi);

		amrex::ParallelFor (box,[=] AMREX_GPU_DEVICE(int i, int j, int k){
			Set::Scalar mul = 1.0/(AMREX_D_TERM(2.0,+2.0,+4.0));
			amrex::Vector<Set::Scalar> _temp;
			for(int n=0; n<damage.number_of_eta; n++)
			{
				_temp.push_back( mul*(AMREX_D_TERM(	
								eta_box(i,j,k,n) + eta_box(i-1,j,k,n)
								,
								+ eta_box(i,j-1,k,n) + eta_box(i-1,j-1,k,n)
								,
								+ eta_box(i,j,k-1,n)	+ eta_box(i-1,j,k-1,n)
								+ eta_box(i,j-1,k-1,n) + eta_box(i-1,j-1,k-1,n)
									)));
			}
			if(damage.type == "water") modelfab(i,j,k,0).DegradeModulus(_temp[0]);
			else if (damage.type == "water2") modelfab(i,j,k,0).DegradeModulus(_temp[0],_temp[1],_temp[2]);
			else Util::Abort(INFO, "Damage model not implemented yet");
		});
	}
	//Util::Message(INFO, "Exit");
}

std::vector<std::string>
PolymerDegradation::PlotFileNameNode (std::string plot_file_name, int lev) const
{
	std::vector<std::string> name;
	name.push_back(plot_file_name+"/");
	name.push_back(amrex::Concatenate("", lev, 5));
	return name;
}

void 
PolymerDegradation::TimeStepComplete(amrex::Real time, int iter)
{
	if (! elastic.on) return;
	if (iter % elastic.interval) return;
	if (time < elastic.tstart) return;
	if (time > elastic.tend) return;

}

void 
PolymerDegradation::TimeStepBegin(amrex::Real time, int iter)
{
	if (!elastic.on) return;
	//if (time < elastic.tstart) return;
	//if (time > elastic.tend) return;

	if ((elastic.type == "tensile.single" || elastic.type == "single") && iter%elastic.interval) return;
	if ((elastic.type == "tensile.single" || elastic.type == "single") && time < elastic.tstart) return;
	if ((elastic.type == "tensile.single" || elastic.type == "single") && time > elastic.tend) return;
	if ((elastic.type == "tensile_test" || elastic.type == "tensile") && std::abs(time-elastic.test_time[elastic.current_test]) > 1.e-4) return;

	Util::Message(INFO);

	LPInfo info;
	info.setAgglomeration(elastic.agglomeration);
	info.setConsolidation(elastic.consolidation);
	info.setMaxCoarseningLevel(elastic.max_coarsening_level);


	amrex::Vector<amrex::FabArray<amrex::BaseFab<model_type> > > model;
	model.resize(nlevels);
	for (int ilev = 0; ilev < nlevels; ++ilev)
	{
		model[ilev].define(displacement[ilev]->boxArray(), displacement[ilev]->DistributionMap(), 1, number_of_ghost_cells);
		model[ilev].setVal(*modeltype);
		DegradeMaterial(ilev,model[ilev]);
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

		AMREX_D_TERM(rhs[ilev]->setVal(elastic.body_force[0]*volume,0,1);,
			     rhs[ilev]->setVal(elastic.body_force[1]*volume,1,1);,
			     rhs[ilev]->setVal(elastic.body_force[2]*volume,2,1););
	}

	if (elastic.type == "single" || elastic.type == "tensile_single")
	{
		AMREX_D_TERM(
			bc.Set(bc.Face::XLO, bc.Direction::X, elastic.bc_xlo[0], elastic.bc_left[0], 	rhs, geom);
	 	    bc.Set(bc.Face::XHI, bc.Direction::X, elastic.bc_xhi[0], elastic.bc_right[0], 	rhs, geom);
	 	    ,
	 	    bc.Set(bc.Face::XLO, bc.Direction::Y, elastic.bc_xlo[1], elastic.bc_left[1], 	rhs, geom);
	 	    bc.Set(bc.Face::XHI, bc.Direction::Y, elastic.bc_xhi[1], elastic.bc_right[1], 	rhs, geom);
	 	    bc.Set(bc.Face::YLO, bc.Direction::X, elastic.bc_ylo[0], elastic.bc_bottom[0], 	rhs, geom);
	 	    bc.Set(bc.Face::YLO, bc.Direction::Y, elastic.bc_ylo[1], elastic.bc_bottom[1], 	rhs, geom);
	 	    bc.Set(bc.Face::YHI, bc.Direction::X, elastic.bc_yhi[0], elastic.bc_top[0], 	rhs, geom);
	 	    bc.Set(bc.Face::YHI, bc.Direction::Y, elastic.bc_yhi[1], elastic.bc_top[1], 	rhs, geom);
	 	    ,
	 	    bc.Set(bc.Face::XLO, bc.Direction::Z, elastic.bc_xlo[2], elastic.bc_left[2], 	rhs, geom);
	 	    bc.Set(bc.Face::XHI, bc.Direction::Z, elastic.bc_xhi[2], elastic.bc_right[2], 	rhs, geom);
	 	    bc.Set(bc.Face::YLO, bc.Direction::Z, elastic.bc_ylo[2], elastic.bc_bottom[2], 	rhs, geom);
	 	    bc.Set(bc.Face::YHI, bc.Direction::Z, elastic.bc_yhi[2], elastic.bc_top[2], 	rhs, geom);
	 	    bc.Set(bc.Face::ZLO, bc.Direction::X, elastic.bc_zlo[0], elastic.bc_back[0], 	rhs, geom);
	 	    bc.Set(bc.Face::ZLO, bc.Direction::Y, elastic.bc_zlo[1], elastic.bc_back[1], 	rhs, geom);
	 	    bc.Set(bc.Face::ZLO, bc.Direction::Z, elastic.bc_zlo[2], elastic.bc_back[2], 	rhs, geom);
	 	    bc.Set(bc.Face::ZHI, bc.Direction::X, elastic.bc_zhi[0], elastic.bc_front[0], 	rhs, geom);
	 	    bc.Set(bc.Face::ZHI, bc.Direction::Y, elastic.bc_zhi[1], elastic.bc_front[1], 	rhs, geom);
	 	    bc.Set(bc.Face::ZHI, bc.Direction::Z, elastic.bc_zhi[2], elastic.bc_front[2], 	rhs, geom);
	 	);
		//Util::Message(INFO);
		Solver::Linear solver(elastic_operator);
		solver.setMaxIter(elastic.max_iter);
		solver.setMaxFmgIter(elastic.max_fmg_iter);
		solver.setFixedIter(elastic.max_fixed_iter);
		solver.setVerbose(elastic.verbose);
		solver.setCGVerbose(elastic.cgverbose);
		solver.setBottomMaxIter(elastic.bottom_max_iter);
		solver.setBottomTolerance(elastic.cg_tol_rel) ;
		solver.setBottomToleranceAbs(elastic.cg_tol_abs) ;
		for (int ilev = 0; ilev < nlevels; ilev++) if (displacement[ilev]->contains_nan()) Util::Warning(INFO);

		if (elastic.bottom_solver == "cg") solver.setBottomSolver(MLMG::BottomSolver::cg);
		else if (elastic.bottom_solver == "bicgstab") solver.setBottomSolver(MLMG::BottomSolver::bicgstab);
		solver.solve(GetVecOfPtrs(displacement), GetVecOfConstPtrs(rhs), elastic.tol_rel, elastic.tol_abs);
		solver.compResidual(GetVecOfPtrs(residual),GetVecOfPtrs(displacement),GetVecOfConstPtrs(rhs));
		for (int lev = 0; lev < nlevels; lev++)
		{
			elastic_operator.Strain(lev,*strain[lev],*displacement[lev]);
			elastic_operator.Stress(lev,*stress[lev],*displacement[lev]);
			elastic_operator.Energy(lev,*energy[lev],*displacement[lev]);
		}
		for (int lev = 0; lev < nlevels; lev++)
		{
			for (amrex::MFIter mfi(*stress[lev],true); mfi.isValid(); ++mfi)
			{
				const amrex::Box& box = mfi.validbox();
				amrex::Array4<const Set::Scalar> const& stress_box = (*stress[lev]).array(mfi);
				amrex::Array4<Set::Scalar> const& stress_vm_box = (*stress_vm[lev]).array(mfi);
				amrex::Array4<const Set::Scalar> const& eta_box = (*eta_new[lev]).array(mfi);
				amrex::ParallelFor (box,[=] AMREX_GPU_DEVICE(int i, int j, int k){
					Set::Matrix sigma;
					Set::Scalar mul = 1.0/(AMREX_D_TERM(2.0,+2.0,+4.0));
					Set::Scalar temp = mul*(AMREX_D_TERM(	
											eta_box(i,j,k,damage.number_of_eta-1) + eta_box(i-1,j,k,damage.number_of_eta-1)
											,
											+ eta_box(i,j-1,k,damage.number_of_eta-1) + eta_box(i-1,j-1,k,damage.number_of_eta-1)
											,
											+ eta_box(i,j,k-1,damage.number_of_eta-1)	+ eta_box(i-1,j,k-1,damage.number_of_eta-1)
											+ eta_box(i,j-1,k-1,damage.number_of_eta-1) + eta_box(i-1,j-1,k-1,damage.number_of_eta-1)
										));
					AMREX_D_PICK(	
						sigma(0,0) = stress_box(i,j,k,0);
						,
						sigma(0,0) = stress_box(i,j,k,0); sigma(0,1) = stress_box(i,j,k,1);
						sigma(1,0) = stress_box(i,j,k,2); sigma(1,1) = stress_box(i,j,k,3);
						,
						sigma(0,0) = stress_box(i,j,k,0); sigma(0,1) = stress_box(i,j,k,1); sigma(0,2) = stress_box(i,j,k,2);
						sigma(1,0) = stress_box(i,j,k,3); sigma(1,1) = stress_box(i,j,k,4); sigma(1,2) = stress_box(i,j,k,5);
						sigma(2,0) = stress_box(i,j,k,6); sigma(2,1) = stress_box(i,j,k,7); sigma(2,2) = stress_box(i,j,k,8);
					);
					Set::Matrix sigmadev = sigma - sigma.trace()/((double) AMREX_SPACEDIM)*Set::Matrix::Identity();
					Set::Scalar temp2 = std::sqrt(1.5*sigmadev.squaredNorm());
					stress_vm_box(i,j,k,0) =  temp2 < (1.-temp)*yieldstrength ? temp2 : (1.-temp)*yieldstrength;
				});
			}
		}
	}
	else if (elastic.type == "tensile" || elastic.type == "tensile_test")
	{
		Set::Scalar test_t = 0.;
		int countstep = 0;
		amrex::Vector<Set::Scalar> plottime;
		amrex::Vector<int> plotstep;
		std::string plotfolder = "elastic_"+ std::to_string(elastic.current_test);

		plottime.resize(nlevels);
		plotstep.resize(nlevels);
		
		while (test_t < elastic.test_duration)
		{
			test_t += elastic.test_dt; countstep++;
			for (int lev = 0; lev < nlevels; lev++) {plottime[lev] = test_t; plotstep[lev]=countstep-1;}
			
			elastic.bc_right[0] += elastic.test_rate*elastic.test_dt;

			AMREX_D_TERM(
			bc.Set(bc.Face::XLO, bc.Direction::X, elastic.bc_xlo[0], elastic.bc_left[0], 	rhs, geom);
	 	    bc.Set(bc.Face::XHI, bc.Direction::X, elastic.bc_xhi[0], elastic.bc_right[0], 	rhs, geom);
	 	    ,
	 	    bc.Set(bc.Face::XLO, bc.Direction::Y, elastic.bc_xlo[1], elastic.bc_left[1], 	rhs, geom);
	 	    bc.Set(bc.Face::XHI, bc.Direction::Y, elastic.bc_xhi[1], elastic.bc_right[1], 	rhs, geom);
	 	    bc.Set(bc.Face::YLO, bc.Direction::X, elastic.bc_ylo[0], elastic.bc_bottom[0], 	rhs, geom);
	 	    bc.Set(bc.Face::YLO, bc.Direction::Y, elastic.bc_ylo[1], elastic.bc_bottom[1], 	rhs, geom);
	 	    bc.Set(bc.Face::YHI, bc.Direction::X, elastic.bc_yhi[0], elastic.bc_top[0], 	rhs, geom);
	 	    bc.Set(bc.Face::YHI, bc.Direction::Y, elastic.bc_yhi[1], elastic.bc_top[1], 	rhs, geom);
	 	    ,
	 	    bc.Set(bc.Face::XLO, bc.Direction::Z, elastic.bc_xlo[2], elastic.bc_left[2], 	rhs, geom);
	 	    bc.Set(bc.Face::XHI, bc.Direction::Z, elastic.bc_xhi[2], elastic.bc_right[2], 	rhs, geom);
	 	    bc.Set(bc.Face::YLO, bc.Direction::Z, elastic.bc_ylo[2], elastic.bc_bottom[2], 	rhs, geom);
	 	    bc.Set(bc.Face::YHI, bc.Direction::Z, elastic.bc_yhi[2], elastic.bc_top[2], 	rhs, geom);
	 	    bc.Set(bc.Face::ZLO, bc.Direction::X, elastic.bc_zlo[0], elastic.bc_back[0], 	rhs, geom);
	 	    bc.Set(bc.Face::ZLO, bc.Direction::Y, elastic.bc_zlo[1], elastic.bc_back[1], 	rhs, geom);
	 	    bc.Set(bc.Face::ZLO, bc.Direction::Z, elastic.bc_zlo[2], elastic.bc_back[2], 	rhs, geom);
	 	    bc.Set(bc.Face::ZHI, bc.Direction::X, elastic.bc_zhi[0], elastic.bc_front[0], 	rhs, geom);
	 	    bc.Set(bc.Face::ZHI, bc.Direction::Y, elastic.bc_zhi[1], elastic.bc_front[1], 	rhs, geom);
	 	    bc.Set(bc.Face::ZHI, bc.Direction::Z, elastic.bc_zhi[2], elastic.bc_front[2], 	rhs, geom);
	 	    );
			//Util::Message(INFO);
			Solver::Linear solver(elastic_operator);
			solver.setMaxIter(elastic.max_iter);
			solver.setMaxFmgIter(elastic.max_fmg_iter);
			solver.setFixedIter(elastic.max_fixed_iter);
			solver.setVerbose(elastic.verbose);
			solver.setCGVerbose(elastic.cgverbose);
			solver.setBottomMaxIter(elastic.bottom_max_iter);
			solver.setBottomTolerance(elastic.cg_tol_rel) ;
			solver.setBottomToleranceAbs(elastic.cg_tol_abs) ;
			for (int ilev = 0; ilev < nlevels; ilev++) if (displacement[ilev]->contains_nan()) Util::Warning(INFO);

			if (elastic.bottom_solver == "cg") solver.setBottomSolver(MLMG::BottomSolver::cg);
			else if (elastic.bottom_solver == "bicgstab") solver.setBottomSolver(MLMG::BottomSolver::bicgstab);
			solver.solve(GetVecOfPtrs(displacement), GetVecOfConstPtrs(rhs), elastic.tol_rel, elastic.tol_abs);
			solver.compResidual(GetVecOfPtrs(residual),GetVecOfPtrs(displacement),GetVecOfConstPtrs(rhs));
			for (int lev = 0; lev < nlevels; lev++)
			{
				elastic_operator.Strain(lev,*strain[lev],*displacement[lev]);
				elastic_operator.Stress(lev,*stress[lev],*displacement[lev]);
				elastic_operator.Energy(lev,*energy[lev],*displacement[lev]);
			}
			for (int lev = 0; lev < nlevels; lev++)
			{
				for (amrex::MFIter mfi(*stress[lev],true); mfi.isValid(); ++mfi)
				{
					const amrex::Box& box = mfi.validbox();
					amrex::Array4<const Set::Scalar> const& stress_box = (*stress[lev]).array(mfi);
					amrex::Array4<Set::Scalar> const& stress_vm_box = (*stress_vm[lev]).array(mfi);
					amrex::Array4<const Set::Scalar> const& eta_box = (*eta_new[lev]).array(mfi);
					amrex::ParallelFor (box,[=] AMREX_GPU_DEVICE(int i, int j, int k){
						Set::Matrix sigma;
						Set::Scalar mul = 1.0/(AMREX_D_TERM(2.0,+2.0,+4.0));
						Set::Scalar temp = mul*(AMREX_D_TERM(	
												eta_box(i,j,k,damage.number_of_eta-1) + eta_box(i-1,j,k,damage.number_of_eta-1)
												,
												+ eta_box(i,j-1,k,damage.number_of_eta-1) + eta_box(i-1,j-1,k,damage.number_of_eta-1)
												,
												+ eta_box(i,j,k-1,damage.number_of_eta-1)	+ eta_box(i-1,j,k-1,damage.number_of_eta-1)
												+ eta_box(i,j-1,k-1,damage.number_of_eta-1) + eta_box(i-1,j-1,k-1,damage.number_of_eta-1)
											));
						AMREX_D_PICK(	
							sigma(0,0) = stress_box(i,j,k,0);
							,
							sigma(0,0) = stress_box(i,j,k,0); sigma(0,1) = stress_box(i,j,k,1);
							sigma(1,0) = stress_box(i,j,k,2); sigma(1,1) = stress_box(i,j,k,3);
							,
							sigma(0,0) = stress_box(i,j,k,0); sigma(0,1) = stress_box(i,j,k,1); sigma(0,2) = stress_box(i,j,k,2);
							sigma(1,0) = stress_box(i,j,k,3); sigma(1,1) = stress_box(i,j,k,4); sigma(1,2) = stress_box(i,j,k,5);
							sigma(2,0) = stress_box(i,j,k,6); sigma(2,1) = stress_box(i,j,k,7); sigma(2,2) = stress_box(i,j,k,8);
						);
						Set::Matrix sigmadev = sigma - sigma.trace()/((double) AMREX_SPACEDIM)*Set::Matrix::Identity();
						Set::Scalar temp2 = std::sqrt(1.5*sigmadev.squaredNorm());
						stress_vm_box(i,j,k,0) =  temp2 < (1.-temp)*yieldstrength ? temp2 : (1.-temp)*yieldstrength;
					});
				}
			}
			WritePlotFile(plotfolder,plottime,plotstep);
		}
		elastic.current_test++;
	}
	else
		return;
}

}
//#endif

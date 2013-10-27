// Petter Strandmark 2012--2013
//
// Test functions from
// Jorge J. More, Burton S. Garbow and Kenneth E. Hillstrom,
// "Testing unconstrained optimization software",
// Transactions on Mathematical Software 7(1):17-41, 1981.
// http://www.caam.rice.edu/~zhang/caam454/nls/MGH.pdf
//
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>

#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <spii/google_test_compatibility.h>

#include <spii/auto_diff_term.h>
#include <spii/solver.h>

using namespace spii;

std::stringstream global_string_stream;
void info_log_function(const std::string& str)
{
	global_string_stream << str << "\n";
}

std::unique_ptr<Solver> create_solver()
{
	std::unique_ptr<NewtonSolver> solver(new NewtonSolver);
	solver->log_function = info_log_function;

	solver->function_improvement_tolerance = 0;
	solver->argument_improvement_tolerance = 0;
	solver->gradient_tolerance = 1e-7;

	return std::move(solver);
}

int cumulative_iterations   = 0;
int cumulative_evalutations = 0;
double cumulative_time      = 0;

template<typename Functor, int dimension>
double run_test_with_factorization_method(double* var, Solver* solver)
{
	Function f;
	f.add_variable(var, dimension);
	f.add_term(std::make_shared<AutoDiffTerm<Functor, dimension>>(), var);

	SolverResults results;
	global_string_stream.str("");
	solver->solve(f, &results);
	INFO(global_string_stream.str());
	INFO(results);

	std::stringstream sout;
	for (int i = 0; i < dimension; ++i) {
		sout << "x" << i + 1 << " = " << var[i] << ",  ";
	}
	INFO(sout.str());

	EXPECT_TRUE(results.exit_condition == SolverResults::GRADIENT_TOLERANCE);

	cumulative_evalutations += f.evaluations_with_gradient;
	cumulative_time         += results.total_time - results.log_time;
	INFO("Cumulative evaluations: " << cumulative_evalutations);
	INFO("Cumulative time       : " << cumulative_time);

	return f.evaluate();
}

class Powell3D;

template<typename Functor, int dimension>
double run_test(double* var, Solver* solver_input = 0)
{
	auto own_solver = create_solver();
	if (solver_input == 0) {
		solver_input = own_solver.get();
	}
	auto solver = dynamic_cast<NewtonSolver*>(solver_input);
	spii_assert(solver);

	// First, test that the iterative factorization method
	// achieves convergence. 
	std::vector<double> var_copy(var, var + dimension);
	solver->factorization_method = NewtonSolver::ITERATIVE;
	run_test_with_factorization_method<Functor, dimension>(&var_copy[0], solver);

	// Then, test the BKP factorization and return the results using
	// this method.
	solver->factorization_method = NewtonSolver::BKP;

	//--------------------
	//TODO: Investigate further.
	//
	if (typeid(Functor) == typeid(Powell3D)) {
		solver->gradient_tolerance = 1e-1;
	}
	//--------------------

	return run_test_with_factorization_method<Functor, dimension>(var, solver);
}

#include "suite_more_et_al.h"
#include "suite_test_opt.h"
#include "suite_uctp.h"

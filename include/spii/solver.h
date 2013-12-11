// Petter Strandmark 2012.
#ifndef SPII_SOLVER_H
#define SPII_SOLVER_H
// The Solver class is a lightweight class defining settings
// for a solver.
//
// The member function Solver::solve uses minimizes a Function
// using the settings in the Solver.
//

#include <functional>
#include <iostream>
#include <string>

#include <spii/spii.h>
#include <spii/function.h>

namespace spii {

// SolverResults contains the result of a call to Solver::solve.
struct SPII_API SolverResults
{
	SolverResults();

	// The exit condition specifies how the solver terminated.
	enum {GRADIENT_TOLERANCE, // Gradient tolerance reached.
	      FUNCTION_TOLERANCE, // Function improvement tolerance reached.
	      ARGUMENT_TOLERANCE, // Argument improvement tolerance reached.
	      NO_CONVERGENCE,     // Maximum number of iterations reached.
	      FUNCTION_NAN,       // Nan encountered.
	      FUNCTION_INFINITY,  // Infinity encountered.
		  USER_ABORT,         // The callback function aborted the optimization.
	      INTERNAL_ERROR,     // Internal error.
	      NA} exit_condition;

	// Returns true if the exit_condition indicates convergence.
	bool exit_success() const
	{
		return exit_condition == GRADIENT_TOLERANCE ||
		       exit_condition == FUNCTION_TOLERANCE ||
		       exit_condition == ARGUMENT_TOLERANCE;
	}

	double startup_time;
	double function_evaluation_time;
	double stopping_criteria_time;
	double matrix_factorization_time;
	double lbfgs_update_time;
	double linear_solver_time;
	double backtracking_time;
	double log_time;
	double total_time;

	// The minimum value of the function being minimized is
	// in this interval. These members are only set by global
	// optmization solvers.
	double optimum_lower;
	double optimum_upper;
};

SPII_API std::ostream& operator<<(std::ostream& out, const SolverResults& results);

// Holds information provided by the solver via a
// callback function. 
//
// Note: All pointers in the struct may be
//       nullptr, depending on the solver.
struct CallbackInformation
{
	double objective_value = std::numeric_limits<double>::quiet_NaN();
	// The currently evaluated point.
	const Eigen::VectorXd* x        = nullptr;
	// The gradient at x.
	const Eigen::VectorXd* g        = nullptr;
	// The dense Hessian at x.
	const Eigen::MatrixXd* H_dense  = nullptr;
	// The sparse Hessian at x.
	const Eigen::SparseMatrix<double>* H_sparse = nullptr; 
};

struct FactorizationCacheInternal;
class FactorizationCache
{
public:
	FactorizationCache(int n);
	~FactorizationCache();
	FactorizationCacheInternal* data;
};

#ifdef _WIN32
	SPII_API_EXTERN_TEMPLATE template class SPII_API std::function<void(const std::string&)>;
	SPII_API_EXTERN_TEMPLATE template class SPII_API std::function<bool(const CallbackInformation&)>;
#endif

class SPII_API Solver
{
public:
	Solver();

	// Specifies which method to use when minimizing
	// a function.
	enum Method {
	             // Newton's method. It requires first and
	             // second-order derivatives. Generally converges
	             // quickly. It is slow and requires a lot of
	             // memory if the Hessian is dense.
	             NEWTON,
	             // L-BFGS. Requires only first-order derivatives
	             // and generally converges quickly. Always uses
	             // relatively little memory.
	             LBFGS,
	             // Nelder-Mead requires no derivatives. It generally
	             // produces slightly more inaccurate solutions in many
	             // more iterations.
	             NELDER_MEAD,
	             // For most problems, there is no reason to choose
	             // pattern search over Nelder-Mead.
	             PATTERN_SEARCH,
				 // (Experimental) Global optimization using interval
				 // arithmetic.
				 GLOBAL
	            };

	// Minimizes a function. The results of the minimization will
	// be stored in results.
	void solve(const Function& function,
	           Method method,
	           SolverResults* results) const;

	void solve_newton(const Function& function,
	                  SolverResults* results) const;

	void solve_lbfgs(const Function& function,
	                 SolverResults* results) const;

	void solve_nelder_mead(const Function& function,
	                       SolverResults* results) const;

	void solve_pattern_search(const Function& function,
	                          SolverResults* results) const;

	void solve_global(const Function& function,
	                  const IntervalVector& start_box,
	                  SolverResults* results) const;

	// Mode of operation. How the Hessian is stored.
	// Default: AUTO.
	enum {DENSE, SPARSE, AUTO} sparsity_mode;

	// Function called every time the solver emits a log message.
	// Default: print to std::cerr.
	std::function<void(const std::string& log_message)> log_function;

	// Function called each iteration.
	// If the function returns false, the solver should abort the optimization.
	// Default: none.
	std::function<bool(const CallbackInformation& information)> callback_function;

	// Maximum number of iterations. Default: 100.
	int maximum_iterations;

	// Gradient tolerance. The solver terminates if
	// ||g|| / ||g0|| < tol, where ||.|| is the maximum
	// norm. Default: 1e-12.
	double gradient_tolerance;

	// Function improvement tolerance. The solver terminates
	// if |df| / (|f| + tol) < tol. Default: 1e-12.
	double function_improvement_tolerance;

	// Argument improvement tolerance. The solver terminates
	// if ||dx|| / (||x|| + tol) < tol. Default: 1e-12.
	double argument_improvement_tolerance;

	// Area tolerance (Nelder-Mead) The solver terminates if
	// ||a|| / ||a0|| < tol, where ||.|| is the maximum
	// norm. Default: 0 (i.e. not used).
	double area_tolerance;

	// Length tolerance (Nelder-Mead) The solver terminates if
	// ||a|| / ||a0|| < tol, where ||.|| is the maximum
	// norm. Default: 1e-12.
	double length_tolerance;

	// Number of vectors L-BFGS should save in its history.
	// Default: 10.
	int lbfgs_history_size;

	// If the relative function improvement is less than this
	// value, L-BFGS will discard its history and restart.
	// Default: 1e-6.
	double lbfgs_restart_tolerance;

	// The line search is completed when
	//   f(x + alpha * p) <= f(x) + c * alpha * gTp.
	// In each iteration, alpha *= rho.
	double line_search_c;    // default: 1e-4.
	double line_search_rho;  // default: 0.5.

	// The default factorization method is the BKP block
	// diagonal modification (Nocedal and Wright, p. 55).
	// Alternatively, it is possible to use iterative diagonal
	// modification of the Hessian. This is also used for
	// sparse systems.
	enum {BKP, ITERATIVE} factorization_method;
private:

	// Computes a Newton step given a function, a gradient and a
	// Hessian.
	bool check_exit_conditions(const double fval,
	                           const double fprev,
	                           const double gnorm,
	                           const double gnorm0,
	                           const double xnorm,
	                           const double dxnorm,
	                           const bool last_iteration_successful,
	                           SolverResults* results) const;

	// Performs a line search from x along direction p. Returns
	// alpha, the multiple of p to get to the new point.
	double perform_linesearch(const Function& function,
	                          const Eigen::VectorXd& x,
	                          const double fval,
	                          const Eigen::VectorXd& g,
	                          const Eigen::VectorXd& p,
	                          Eigen::VectorXd* scratch,
	                          const double start_alpha = 1.0) const;

	// Performs a BKP block diagonal factorization, modifies it, and
	// solvers the linear system.
	void BKP_dense(const Eigen::MatrixXd& H,
	               const Eigen::VectorXd& g,
	               const FactorizationCache& cache,
	               Eigen::VectorXd* p,
	               SolverResults* results) const;
};

}  // namespace spii

#endif

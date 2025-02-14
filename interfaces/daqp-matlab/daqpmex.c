#include "mex.h"
#include "api.h"
#include "utils.h"
#include <string.h>

const char* INFO_FIELDS[] = {
  "lambda",
  "setup_time",           
  "solve_time",           
  "iter",           
  "nodes",
  "soft_slack"}; 

const char* SETTINGS_FIELDS[] = {
  "primal_tol",           
  "dual_tol",           
  "zero_tol",           
  "pivot_tol",
  "progress_tol",
  "cycle_tol",
  "iter_limit",
  "fval_bound",
  "eps_prox",
  "eta_prox",
  "rho_soft"}; 


/* The gateway function */
void mexFunction( int nlhs, mxArray *plhs[],
				  int nrhs, const mxArray *prhs[])
{

  // RHS 

  // Extract command
  char cmd[64];
  mxGetString(prhs[0], cmd, sizeof(cmd));
  
  // Extract workspace pointer 
  DAQPWorkspace *work;
  long long *work_i;
  union{long long i; void *ptr;} work_ptr; // Used for int64 & pointer juggling..
  if(nrhs>1){// Pointer always second argument (stored as int64)
	work_i = (long long *)mxGetData(prhs[1]);
	work_ptr.i  = *work_i; 
	work = work_ptr.ptr;
  }

	if (!strcmp("new", cmd)) {
	  // Allocate new workspace and return pointer to it
	  work_ptr.ptr = calloc(1,sizeof(DAQPWorkspace));
	  // Return pointer as int64
	  plhs[0] = mxCreateNumericMatrix(1, 1, mxINT64_CLASS, mxREAL);
	  work_i = (long long *) mxGetData(plhs[0]);
	  *work_i = work_ptr.i;
	  return;
	}
	else if (!strcmp("delete", cmd)) {
	  // Free workspace 
	  free_daqp_workspace(work);
	  free_daqp_ldp(work);
	  if(work->qp) free(work->qp);
	  free(work);
	  return;
	}
	else if (!strcmp("setup", cmd)) {

	  if(work->qp) mexErrMsgTxt("Setup already completed.");
	  DAQPProblem *qp = calloc(1,sizeof(DAQPProblem));
	  // Extract data
	  int error_flag;
	  
	  // Get dimensions 
	  int n = mxGetM(prhs[4]);
	  int m = mxGetM(prhs[5]);
	  int ms = m-mxGetN(prhs[4]);
	  int nb = mxGetM(prhs[8]);
	  
	  // Setup QP struct
	  qp->n = n;
	  qp->m = m;
	  qp->ms = ms;
	  qp->H= mxIsEmpty(prhs[2]) ? NULL : mxGetPr(prhs[2]);
	  qp->f= mxIsEmpty(prhs[3]) ? NULL : mxGetPr(prhs[3]);
	  qp->A= mxGetPr(prhs[4]);
	  qp->bupper= mxGetPr(prhs[5]);
	  qp->blower= mxGetPr(prhs[6]);
	  qp->sense= (int *)mxGetPr(prhs[7]);
	  qp->bin_ids= (int *)mxGetPr(prhs[8]);
	  qp->nb=nb; 
	  
	  double solve_time;
	  error_flag = setup_daqp(qp,work,&solve_time);
	  if(error_flag < 0){
		free(work->qp);
		work->qp = NULL;
	  }


	  plhs[0] = mxCreateDoubleScalar(error_flag);
	  plhs[1] = mxCreateDoubleScalar(solve_time);
	}
	else if (!strcmp("solve", cmd)) {
	  double *fval;
	  int *exitflag;
	  DAQPResult result;
	  if(work->qp == NULL) mexErrMsgTxt("No problem to solve");
	  // Update QP pointers 
	  work->qp->H= mxIsEmpty(prhs[2]) ? NULL : mxGetPr(prhs[2]);
	  work->qp->f= mxIsEmpty(prhs[3]) ? NULL : mxGetPr(prhs[3]);
	  work->qp->A= mxGetPr(prhs[4]);
	  work->qp->bupper= mxGetPr(prhs[5]);
	  work->qp->blower= mxGetPr(prhs[6]);
	  work->qp->sense= (int *)mxGetPr(prhs[7]);
	  // Setup output 
	  plhs[0] = mxCreateDoubleMatrix((mwSize)work->n,1,mxREAL); // x_star
	  plhs[1] = mxCreateDoubleMatrix(1,1,mxREAL); // fval
	  plhs[2] = mxCreateNumericMatrix(1, 1, mxINT32_CLASS, mxREAL); //Exit flag
	  plhs[3] = mxCreateDoubleMatrix(1,1,mxREAL); //CPU time
	  plhs[0] = mxCreateDoubleMatrix((mwSize)work->n,1,mxREAL); // x_star
	  mxArray* lam = mxCreateDoubleMatrix((mwSize)work->m,1,mxREAL); // lambda

	  result.x = mxGetPr(plhs[0]);
	  result.lam = mxGetPr(lam);
	  fval= mxGetPr(plhs[1]);
	  exitflag = (int *)mxGetPr(plhs[2]);
	  
	  // Solve problem
	  daqp_solve(&result,work); 
	  // Extract solution information
	  exitflag[0] = result.exitflag; 
	  fval[0] = result.fval;

	  // Package info struct
	  int n_info = sizeof(INFO_FIELDS)/sizeof(INFO_FIELDS[0]);
	  mxArray* info_struct = mxCreateStructMatrix(1,1,n_info,INFO_FIELDS);
	  mxSetField(info_struct, 0, "lambda", lam);
	  mxSetField(info_struct, 0, "solve_time", mxCreateDoubleScalar(result.solve_time));
	  mxSetField(info_struct, 0, "setup_time", mxCreateDoubleScalar(result.setup_time));
	  mxSetField(info_struct, 0, "iter", mxCreateDoubleScalar(result.iter));
	  if(work->bnb != NULL)
		mxSetField(info_struct, 0, "nodes", mxCreateDoubleScalar(work->bnb->nodecount));
	  else
		mxSetField(info_struct, 0, "nodes", mxCreateDoubleScalar(1));
	  mxSetField(info_struct, 0, "soft_slack", mxCreateDoubleScalar(result.soft_slack));
	  plhs[3] = info_struct;
	}
	else if (!strcmp("set_default_settings", cmd)){
	  if(work->settings == NULL) work->settings = malloc(sizeof(DAQPSettings));
	  daqp_default_settings(work->settings);
	}
	else if (!strcmp("get_settings", cmd)) {
	  if(work->settings != NULL){
		int n_settings = sizeof(SETTINGS_FIELDS)/sizeof(SETTINGS_FIELDS[0]);
		mxArray* s = mxCreateStructMatrix(1,1,n_settings,SETTINGS_FIELDS);
		mxSetField(s, 0, "primal_tol", mxCreateDoubleScalar(work->settings->primal_tol));
		mxSetField(s, 0, "dual_tol", mxCreateDoubleScalar(work->settings->dual_tol));
		mxSetField(s, 0, "zero_tol", mxCreateDoubleScalar(work->settings->zero_tol));
		mxSetField(s, 0, "pivot_tol", mxCreateDoubleScalar(work->settings->pivot_tol));
		mxSetField(s, 0, "progress_tol", mxCreateDoubleScalar(work->settings->progress_tol));
		mxSetField(s, 0, "cycle_tol", mxCreateDoubleScalar(work->settings->cycle_tol));
		mxSetField(s, 0, "iter_limit", mxCreateDoubleScalar(work->settings->iter_limit));
		mxSetField(s, 0, "fval_bound", mxCreateDoubleScalar(work->settings->fval_bound));
		mxSetField(s, 0, "eps_prox", mxCreateDoubleScalar(work->settings->eps_prox));
		mxSetField(s, 0, "eta_prox", mxCreateDoubleScalar(work->settings->eta_prox));
		mxSetField(s, 0, "rho_soft", mxCreateDoubleScalar(work->settings->rho_soft));
		plhs[0] = s;
	  }
	}
	else if (!strcmp("set_settings", cmd)) {
	  const mxArray* s = prhs[2];
	  work->settings->primal_tol = (c_float)mxGetScalar(mxGetField(s, 0, "primal_tol"));
	  work->settings->dual_tol =  (c_float)mxGetScalar(mxGetField(s, 0, "dual_tol"));
	  work->settings->zero_tol = (c_float)mxGetScalar(mxGetField(s, 0, "zero_tol"));
	  work->settings->pivot_tol = (c_float)mxGetScalar(mxGetField(s, 0, "pivot_tol"));
	  work->settings->progress_tol = (c_float)mxGetScalar(mxGetField(s, 0, "progress_tol"));
	  work->settings->cycle_tol = (int)mxGetScalar(mxGetField(s, 0, "cycle_tol"));
	  work->settings->iter_limit= (int)mxGetScalar(mxGetField(s, 0, "iter_limit"));
	  work->settings->fval_bound= (c_float)mxGetScalar(mxGetField(s, 0, "fval_bound"));
	  work->settings->eps_prox = (c_float)mxGetScalar(mxGetField(s, 0, "eps_prox"));
	  work->settings->eta_prox= (c_float)mxGetScalar(mxGetField(s, 0, "eta_prox"));
	  work->settings->rho_soft= (c_float)mxGetScalar(mxGetField(s, 0, "rho_soft"));
	}
	else if (!strcmp("update", cmd)) {
	  if(work->qp == NULL) mexErrMsgTxt("No problem to update");
	  // Update QP pointers 
	  work->qp->H= mxIsEmpty(prhs[2]) ? NULL : mxGetPr(prhs[2]);
	  work->qp->f= mxIsEmpty(prhs[3]) ? NULL : mxGetPr(prhs[3]);
	  work->qp->A= mxGetPr(prhs[4]);
	  work->qp->bupper= mxGetPr(prhs[5]);
	  work->qp->blower= mxGetPr(prhs[6]);
	  work->qp->sense= (int *)mxGetPr(prhs[7]);
	  // Update LDP with new QP data
	  const int update_mask = (int)mxGetScalar(prhs[8]);
	  update_ldp(update_mask,work);
	}

  // RHS
}

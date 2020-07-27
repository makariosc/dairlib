//
// Created by jianshu on 3/25/20.
//
#include "examples/goldilocks_models/find_models/initial_guess.h"

using std::cout;
using std::endl;
using std::string;
using std::to_string;

using Eigen::MatrixXd;
using Eigen::VectorXd;

namespace dairlib::goldilocks_models {
// edited by Jianshu to try a new way of setting initial guess

VectorXd GetThetaScale(const ReducedOrderModel& rom) {
  // considering the scale for theta doesn't have a significant impact on
  // improving the quality of the initial guess,set them all ones.
  return VectorXd::Ones(rom.n_y() + rom.n_yddot());
}

VectorXd GetGammaScale(const TasksGenerator* task_gen) {
  int gamma_dimension = task_gen->dim();
  VectorXd gamma_scale = VectorXd::Zero(gamma_dimension);
  // if not fixed task, we need to scale the gamma
  int dim = 0;
  double min;
  double max;
  for (dim = 0; dim < gamma_dimension; dim++) {
    min = task_gen->task_min(task_gen->names()[dim]);
    max = task_gen->task_max(task_gen->names()[dim]);
    if (!(min == max)) {
      // coefficient is different for different dimensions
      if (task_gen->names()[dim] == "turning_rate") {
        gamma_scale[dim] = 1.3 / (max - min);
      } else {
        gamma_scale[dim] = 1 / (max - min);
      }
    }
  }
  return gamma_scale;
}

// calculate the third power of L3 norm between two gammas
double GammaDistanceCalculation(const VectorXd& past_gamma,
    const VectorXd& current_gamma,
    const VectorXd& gamma_scale){
  VectorXd dif_gamma =
      (past_gamma - current_gamma).array().abs() * gamma_scale.array();
  VectorXd dif_gamma2 = dif_gamma.array().pow(2);
  double distance_gamma = (dif_gamma.transpose() * dif_gamma2)(0, 0);

  return distance_gamma;
}

// calculate the interpolation weight; update weight vector and solution matrix
void InterpolateAmongDifferentTasks(const string& dir, string prefix,
                                    const VectorXd& current_gamma,
                                    const VectorXd& gamma_scale,
                                    VectorXd& weight_vector,
                                    MatrixXd& solution_matrix) {
  // check if this sample is success
  int is_success = (readCSV(dir + prefix + string("_is_success.csv")))(0, 0);
  if (is_success == 1) {
    // extract past gamma
    VectorXd past_gamma = readCSV(dir + prefix + string("_task.csv"));
    double distance_gamma = GammaDistanceCalculation(past_gamma,
        current_gamma,gamma_scale);

    // extract the solution
    VectorXd w_to_interpolate = readCSV(dir + prefix + string("_w.csv"));
    // concatenate the weight and solution for further calculation
    solution_matrix.conservativeResize(w_to_interpolate.rows(),
                                       solution_matrix.cols() + 1);
    solution_matrix.col(solution_matrix.cols() - 1) = w_to_interpolate;
    weight_vector.conservativeResize(weight_vector.rows() + 1);
    weight_vector(weight_vector.rows() - 1) = 1 / distance_gamma;
  }
}

// calculate interpolated initial guess using weight vector and solution matrix
VectorXd CalculateInterpolation(const VectorXd& weight_vector,
                                const MatrixXd& solution_matrix) {
  DRAKE_DEMAND(weight_vector.rows() > 0);
  // normalize the weight vector by L1 norm and interpolate
  VectorXd interpolated_solution = solution_matrix * weight_vector/weight_vector.sum();
  return interpolated_solution;
}

// calculate the distance between two past tasks with current tasks and return
// the prefix of the closer task
string CompareTwoTasks(const string& dir, string prefix1,string prefix2,
                       const VectorXd& current_gamma,
                       const VectorXd& gamma_scale){
  string prefix_closer_task = prefix1;
  int is_success = (readCSV(dir + prefix2 + string("_is_success.csv")))(0, 0);
  if (is_success == 1){
    // extract past tasks
    VectorXd past_gamma1 = readCSV(dir + prefix1 + string("_task.csv"));
    VectorXd past_gamma2 = readCSV(dir + prefix2 + string("_task.csv"));
    // calculate the distance between two gammas
    double distance_gamma1 = GammaDistanceCalculation(past_gamma1,
                                                     current_gamma,gamma_scale);
    double distance_gamma2 = GammaDistanceCalculation(past_gamma2,
                                                     current_gamma,gamma_scale);
    if(distance_gamma2<distance_gamma1){
      prefix_closer_task = prefix2;
    }
  }
  return prefix_closer_task;
}


string SetInitialGuessByInterpolation(const string& directory, int iter,
                                      int sample,
                                      const TasksGenerator* task_gen,
                                      const Task& task,
                                      const ReducedOrderModel& rom) {
  // before iter_start_optimization, we extend the task space and use the extrapolation to
  // create initial guess
  DRAKE_DEMAND(iter >= (task_gen->iter_start_optimization()));
  /* define some parameters used in interpolation
   * theta_range :decide the range of theta to use in interpolation
   * theta_sclae,gamma_scale :used to scale the theta and gamma in interpolation
   */
  double theta_range =
      0.004;  // this is tuned by robot_option=1,rom_option=2,3d task space
  int total_sample_num = task_gen->total_sample_number();

  VectorXd theta_scale = GetThetaScale(rom);
  VectorXd gamma_scale = GetGammaScale(task_gen);
  //    initialize variables used for setting initial guess
  VectorXd initial_guess;
  string initial_file_name;
  //    get theta of current iteration and task of current sample
  VectorXd current_theta = rom.theta();
  VectorXd current_gamma =
      Eigen::Map<const VectorXd>(task.get().data(), task.get().size());

  if (iter==(task_gen->iter_start_optimization())) {
    // Use the samples from iteration 0 to iteration 99
    // Considering that the theta are all same for these iteration,
    // we only consider task in interpolation.

    // take out corresponding solution and store it in each column of w_gamma
    // calculate the interpolation weight and store it in weight_gamma
    MatrixXd w_gamma;
    VectorXd weight_gamma;

    int past_iter;
    int sample_num;
    string prefix;

    /*
     * calculate the weighted sum of past solutions
     */
//    for (past_iter = iter - 1; past_iter > 0; past_iter--){
//      for (sample_num = 0; sample_num < total_sample_num; sample_num++) {
//        prefix = to_string(past_iter) + string("_") + to_string(sample_num);
//        InterpolateAmongDifferentTasks(directory, prefix, current_gamma,
//                                       gamma_scale, weight_gamma, w_gamma);
//      }
//    }
//    // calculate the weighted sum of all past iterations
//    initial_guess = CalculateInterpolation(weight_gamma, w_gamma);
//    //    save initial guess and set init file
//    initial_file_name = to_string(iter) + "_" + to_string(sample) +
//        string("_initial_guess.csv");
//    writeCSV(directory + initial_file_name, initial_guess);

    /*
    * find the closest sample and set it as initial guess
    */
    string prefix_cloest_task = to_string(iter-1) + string("_") + to_string(0);
    for (past_iter = iter - 1; past_iter > 0; past_iter--){
      for (sample_num = 0; sample_num < total_sample_num; sample_num++) {
        prefix = to_string(past_iter) + string("_") + to_string(sample_num);
        prefix_cloest_task = CompareTwoTasks(directory,prefix_cloest_task,
            prefix,current_gamma,gamma_scale);
      }
    }
    initial_file_name = prefix_cloest_task+string("_w.csv");

  } else {
    // There are two-stage interpolation here.
    // Get interpolated results using solutions of different tasks for each
    // theta. Then calculate interpolation using results from different theta.

    VectorXd weight_theta;  // each element corresponds to a weight
    MatrixXd w_theta;       // each column stores a interpolated result
    int past_iter;
    int sample_num;
    int iter_start = (task_gen->iter_start_optimization());
    string prefix;
    for (past_iter = iter - 1; past_iter >= iter_start; past_iter--) {
      // find useful theta according to the difference between previous theta
      // and new theta
      VectorXd past_theta_s =
          readCSV(directory + to_string(past_iter) + string("_theta_y.csv"));
      VectorXd past_theta_sDDot = readCSV(directory + to_string(past_iter) +
                                          string("_theta_yddot.csv"));
      VectorXd past_theta(past_theta_s.rows() + past_theta_sDDot.rows());
      past_theta << past_theta_s, past_theta_sDDot;
      double theta_diff =
          (past_theta - current_theta).norm() / current_theta.norm();
      if ( (theta_diff < theta_range) ) {
        // take out corresponding solution and store it in each column of
        // w_gamma calculate the interpolation weight and store it in
        // weight_gamma
        MatrixXd w_gamma;
        VectorXd weight_gamma;
        if(past_iter==(task_gen->iter_start_optimization()))
        {
          // Considering that theta for iteration 0 to 100 are the same,
          // we need to use the solutions from all these iterations to
          // create interpolated solution for this theta instead of using only
          // solutions from one iteration

          // calculate the weighted sum of past solutions
          int iter_same_theta;
          for (iter_same_theta = past_iter-1; past_iter > 0; past_iter--){
            for (sample_num = 0; sample_num < total_sample_num; sample_num++) {
              prefix = to_string(iter_same_theta) + string("_") + to_string(sample_num);
              InterpolateAmongDifferentTasks(directory, prefix, current_gamma,
                                             gamma_scale, weight_gamma, w_gamma);
            }
          }
        }
        else {
          // calculate the weighted sum of solutions from one iteration
          for (sample_num = 0; sample_num < total_sample_num; sample_num++) {
            prefix = to_string(past_iter) + string("_") + to_string(sample_num);
            InterpolateAmongDifferentTasks(directory, prefix, current_gamma,
                                           gamma_scale, weight_gamma, w_gamma);
          }
        }
        // calculate the weighted sum of solutions for this theta
        VectorXd w_to_interpolate =
            CalculateInterpolation(weight_gamma, w_gamma);
        // calculate the weight for the result above using the difference
        // between past theta and current theta
        VectorXd dif_theta =
            (past_theta - current_theta).array().abs() * theta_scale.array();
        double distance_theta = (dif_theta.transpose() * dif_theta)(0, 0);
        // if theta in this iteration accidentally equals to current theta, no
        // need to interpolate and just use the solution from this iteration.
        if (distance_theta == 0) {
          w_theta.conservativeResize(w_to_interpolate.rows(), 1);
          w_theta << w_to_interpolate;
          weight_theta.conservativeResize(1);
          weight_theta << 1;
          break;
        }
          // else concatenate the weighted sum of this iteration and the weight
          // for it
        else {
          w_theta.conservativeResize(w_to_interpolate.rows(),
                                     w_theta.cols() + 1);
          w_theta.col(w_theta.cols() - 1) = w_to_interpolate;
          weight_theta.conservativeResize(weight_theta.rows() + 1);
          weight_theta(weight_theta.rows() - 1) = 1 / distance_theta;
        }
      }
    }
    initial_guess = CalculateInterpolation(weight_theta, w_theta);
    //    save initial guess and set init file
    initial_file_name = to_string(iter) + "_" + to_string(sample) +
                        string("_initial_guess.csv");
    writeCSV(directory + initial_file_name, initial_guess);
  }
  return initial_file_name;
}

// Use extrapolation to provide initial guesses while extending the task space
string SetInitialGuessByExtrapolation(const string& directory, int iter,
                                      int sample,
                                      const TasksGenerator* task_gen,
                                      const Task& task){
  // The algorithm here is based on the idea that the solution moves towards one
  // direction consistently.
  // S_n+1 = S_n+step_n
  // To predict the solution of iteration n+1, we need a estimation of step_n.
  // We use the results from previous iteration to estimate it.
  // step_n = w1(S1-S0)+w2(S2-S1)+w3(S3-S2)+...+w_n(Sn-Sn-1)
  // Si is the interpolated solution from iteration i
  DRAKE_DEMAND(iter > 0);
  VectorXd initial_guess;
  string initial_file_name;
  int past_iter;
  int sample_num;
  int total_sample_num = task_gen->total_sample_number();
  VectorXd current_gamma =
      Eigen::Map<const VectorXd>(task.get().data(), task.get().size());
  VectorXd gamma_scale = GetGammaScale(task_gen);
  // calculate the weighted sum of solutions from iteration 0
  MatrixXd w_gamma;
  VectorXd weight_gamma;
  string prefix;
  for (sample_num = 0; sample_num < total_sample_num; sample_num++) {
    prefix = to_string(0) + string("_") + to_string(sample_num);
    InterpolateAmongDifferentTasks(directory, prefix, current_gamma,
                                   gamma_scale, weight_gamma, w_gamma);
  }
  VectorXd interpolated_solution_0 =
      CalculateInterpolation(weight_gamma, w_gamma);
  if(iter==1){
    //for iteration 1 use the interpolated solution from iteration 0
    initial_file_name = to_string(iter) + "_" + to_string(sample) +
        string("_initial_guess.csv");
    writeCSV(directory + initial_file_name, interpolated_solution_0);
  }
  else{
    // Considering that we doesn't change the theta during the expansion,
    // we only extrapolate using the task.
    MatrixXd w_iteration = MatrixXd::Ones(interpolated_solution_0.rows(),iter);
    w_iteration.col(0) = interpolated_solution_0;
    MatrixXd w_step = MatrixXd::Ones(interpolated_solution_0.rows(),iter-1);
    VectorXd weight_step = VectorXd::Ones(iter-1);
    for (past_iter = iter - 1; past_iter > 0; past_iter--){
      MatrixXd w_gamma;
      VectorXd weight_gamma;
      for (sample_num = 0; sample_num < total_sample_num; sample_num++) {
        prefix = to_string(past_iter) + string("_") + to_string(sample_num);
        InterpolateAmongDifferentTasks(directory, prefix, current_gamma,
                                       gamma_scale, weight_gamma, w_gamma);
      }
      // calculate the weighted sum for this iteration
      VectorXd interpolated_solution_i =
          CalculateInterpolation(weight_gamma, w_gamma);
      w_iteration.col(past_iter) = interpolated_solution_i;
    }
    //calculate the estimation of step with past interpolated solutions
    for (int i  = 0; i<w_step.cols(); i++){
      //calculate the difference between two adjacent interpolated solutions
      w_step.col(i) = w_iteration.col(i+1)-w_iteration.col(i);
      weight_step(i) = i+1;
    }
    VectorXd step = CalculateInterpolation(weight_step, w_step);
    VectorXd solution_last_iter = w_iteration.col(w_iteration.cols()-1);
    initial_guess = solution_last_iter+step;
    initial_file_name = to_string(iter) + "_" + to_string(sample) +
        string("_initial_guess.csv");
    writeCSV(directory + initial_file_name, initial_guess);
  }

  return initial_file_name;
}

}  // namespace dairlib::goldilocks_models
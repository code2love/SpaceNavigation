// fifth implementation, using Igatherv, Bcast and Gatherv

// includes
#include "vi_processor_impl_distr_05.h" // header
#include <mpi.h> // openmpi support

/**
 * define helper function to check if an error occured, print it then
 */
#define MPI_Error_Check(x) {const int err=x; if(x!=MPI_SUCCESS) { fprintf(stderr, "MPI ERROR %d at %d.", err, __LINE__);}}

/**
 * get name of implementation
 * @return name: name of implementation [String]
 */
std::string VI_Processor_Impl_Distr_05::GetName()
{
    return VI_Processor_Base::GetName() + "-" + std::to_string(comm_period);
}

/**
 * set parameter of implementation
 * @param param: param name to set [String]
 * @param value: value of param to set [String]
 * @return hasParam: whether implementation has parameters [=true] or not [=false] [Boolean]
 */
bool VI_Processor_Impl_Distr_05::SetParameter(std::string param, float value)
{
    if(param == "comm_period")
    {
        comm_period = value;
        return true;
    }

    return VI_Processor_Base::SetParameter(param, value);
}

/**
 * get parameter of implementation
 * @return parameters: mapped pair of "alpha" and "tolerance" [Pair]
 */
std::map<std::string, float> VI_Processor_Impl_Distr_05::GetParameters()
{
    std::map<std::string, float> parameters = VI_Processor_Base::GetParameters();
    parameters["comm_period"] = comm_period;
    return parameters;
}

/**
 * actual implementation of inherited abstract function for the communication scheme
 * @param Pi_out: initialized Pi [Integer vector reference]
 * @param J_out: initialized J [Float vector reference]
 * @param P: probability matrix [const Eigen SparseMatrix<RowMajor> reference]
 * @param max_iter: maximum number of iterations before stopping anyway [const unsigned Integer]
 */
void VI_Processor_Impl_Distr_05::value_iteration_impl(
        Eigen::Ref<Eigen::VectorXi> Pi,
        Eigen::Ref<Eigen::VectorXf> J,
        const Eigen::Ref<const SpMat_t> P,
        const unsigned int T)
{
    // init openmpi parameters
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // init the statusses of messages
    MPI_Status status;
    MPI_Request request;

    // get parameters for size of message
    int processor_workload = ceil(float(J.size()) / world_size);
    int processor_workload_last = J.size() - (world_size - 1) * processor_workload;
    // Split up states for each process (last process has a less data)
    int processor_start = processor_workload * world_rank;
    int processor_end = processor_workload * (world_rank + 1);

    // last processes end is size of J
    if (world_rank == world_size -1)
    {
        processor_end = J.size();
    }

    // init buffer for received J
    Eigen::VectorXf J_buffer(0);
    J_buffer = J.segment(0, J.size());

    // set number of states per process and displacements of subvectors
    std::vector<int> recvcounts;    // Number of states for each process
    std::vector<int> displs;        // Displacement of sub vectors for each process
    for(int i=0; i < world_size; i++)
    {
        if(i == world_size - 1)
        {
            recvcounts.push_back(processor_workload_last);
            displs.push_back(i*processor_workload);
        }

        else
        {
            recvcounts.push_back(processor_workload);
            displs.push_back(i*processor_workload);
        }
    }

    for(unsigned int t=0; t < T; ++t)
    {
        // Compute one value iteration step for a range of states
        iteration_step(Pi, J, P, processor_start, processor_end);

        // Exchange results every comm_period cycles
        if(t % comm_period == 0)
        {
            float* J_raw = J.data();
            MPI_Igatherv(&J_raw[processor_start],
                        processor_end - processor_start,
                        MPI_FLOAT,
                        J_raw,
                        recvcounts.data(),
                        displs.data(),
                        MPI_FLOAT,
                        root_id,
                        MPI_COMM_WORLD,
                        &request);

            ////////////////////////////////////////////////////
            // Do some additional computations here if needed //
            ////////////////////////////////////////////////////

            MPI_Wait(&request, &status);

            // get difference between J_recv and corresponding segment in J
            float deviation = (J_buffer-J).cwiseAbs().maxCoeff();
            // Broadcast error from root to all processes (that they can stop respectively), also synchronizes all processes
            MPI_Bcast(&deviation, 1, MPI_FLOAT, root_id, MPI_COMM_WORLD);
            // If convergence rate is below threshold stop
            if(deviation <= tolerance)
            {
                debug_message("Converged after " + std::to_string(t) + " iterations with communication period " + std::to_string(comm_period));
                break;
            }
            // Broadcast J again from root to all processes because we did not converge yet
            MPI_Bcast(J.data(), J.size(), MPI_FLOAT, root_id, MPI_COMM_WORLD);
            // update J
            J_buffer = J;
        }
    }
    // Merge Policy

    int* Pi_raw = Pi.data();
    MPI_Gatherv(&Pi_raw[processor_start],
                 processor_end - processor_start,
                 MPI_INT,
                 Pi_raw,
                 recvcounts.data(),
                 displs.data(),
                 MPI_INT,
                 root_id,
                 MPI_COMM_WORLD);
}
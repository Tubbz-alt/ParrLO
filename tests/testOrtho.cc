#include "MatrixClasses/Matrix_decl.hpp"
#include "MatrixClasses/Timer.hpp"
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <string>
#include <unistd.h>
#ifdef NCCL_COMM
#include "nccl.h"
#endif

#ifndef USE_MAGMA
#include "magma_v2.h"
#endif

int main(int argc, char** argv)
{

    int i = MPI_Init(&argc, &argv);

    if (i != MPI_SUCCESS)
    {
    }
    else
    {

        MPI_Comm lacomm;

        MPI_Comm_dup(MPI_COMM_WORLD, &lacomm);

        MPI_Barrier(lacomm);
        std::cout << "MPI SUCCESS" << i << std::endl;

        int comm_rank, comm_size;
        MPI_Comm_rank(lacomm, &comm_rank);
        MPI_Comm_size(lacomm, &comm_size);

#ifdef NCCL_COMM
        ncclUniqueId id;
        ncclComm_t nccl_world_comm;

        if (comm_rank == 0) ncclGetUniqueId(&id);
        MPI_Bcast((void*)&id, sizeof(id), MPI_BYTE, 0, MPI_COMM_WORLD);

        ncclCommInitRank(&nccl_world_comm, comm_size, id, comm_rank);
#else
        int nccl_world_comm = 0;
#endif

        std::string name = "test_ortho";
        Timer totime(name);
        totime.start();

#ifdef USE_MAGMA
        magma_init();
#else

#endif
        const int nrows = 20;
        const int ncols = 10;

        Matrix A(nrows, ncols, lacomm, nccl_world_comm);
        Matrix B(nrows, ncols, lacomm, nccl_world_comm);

        A.randomInitialize();
        A.scaleMatrix(0.01);
        A.printMatrix();

        B.identityInitialize();
        B.printMatrix();

        A.matrixSum(B);
        A.printMatrix();

        double dfo_before = 0.0;
        double dfo_after  = 0.0;
        const double toll = 1.e-8;

        // Perform the check on the departure from orthogonality before
        // re-orthogonalizing
        dfo_before = A.orthogonalityCheck();

        int count_iter
            = A.orthogonalize("iterative_method_single", false, 10, 1.e-4);

        dfo_after = A.orthogonalityCheck();

        if (dfo_after < toll and dfo_after < dfo_before)
        {
            if (comm_rank == 0)
                std::cout
                    << "Orthogonalized A with dfo before orthogonalizing: "
                    << dfo_before
                    << " and dfo after orthogonalizing: " << dfo_after
                    << std::endl
                    << " took " << count_iter << " to converge " << std::endl;
            A.printMatrix();

            // Print timers for operations performed on Replicated matrix
            Replicated::printTimers(std::cout);
            Matrix::printTimers(std::cout);

            MPI_Comm_free(&lacomm);

#ifdef USE_MAGMA
            magma_finalize();
#else

#endif
            totime.stop();
            totime.print(std::cout);

#ifdef NCCL_COMM
            ncclCommDestroy(nccl_world_comm);
#endif

            MPI_Finalize();

            return 0;
        }
        else
        {
            if (comm_rank == 0)
                std::cout << "Orthogonality test failed A with dfo before"
                             "orthogonalizing: "
                          << dfo_before
                          << " and dfo after orthogonalizing: " << dfo_after
                          << std::endl;
            MPI_Comm_free(&lacomm);

#ifdef USE_MAGMA
            magma_finalize();
#else

#endif
            totime.stop();
            totime.print(std::cout);

#ifdef NCCL_COMM
            ncclCommDestroy(nccl_world_comm);
#endif

            MPI_Finalize();

            return 1;
        }
    }
}

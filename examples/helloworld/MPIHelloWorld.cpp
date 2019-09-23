// A simple MPI code printing a message by each MPI rank

#include <iostream>
#include <mpi.h>


int main()
{
	int my_rank;
	int world_size;

	MPI_Init(NULL, NULL);

	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	std::cout << "Hello World from process " << my_rank << " out of " << world_size << " processes!!!" << std::endl;

	MPI_Finalize();
	return 0;
}

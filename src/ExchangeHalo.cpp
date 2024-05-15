
//@HEADER
// ***************************************************
//
// HPCG: High Performance Conjugate Gradient Benchmark
//
// Contact:
// Michael A. Heroux ( maherou@sandia.gov)
// Jack Dongarra     (dongarra@eecs.utk.edu)
// Piotr Luszczek    (luszczek@eecs.utk.edu)
//
// ***************************************************
//@HEADER

/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 @file ExchangeHalo.cpp

 HPCG routine
 */

// Compile this routine only if running with MPI
#ifndef HPCG_NO_MPI
#include "ExchangeHalo.hpp"
#include "Geometry.hpp"
#include <cstdlib>
#include <mpi.h>

extern p2p_comm_mode_t P2P_Mode;

/*!
  Communicates data that is at the border of the part of the domain assigned to this processor.

  @param[in]    A The known system matrix
  @param[inout] x On entry: the local vector entries followed by entries to be communicated; on exit: the vector with
  non-local entries updated by other processors
 */
void ExchangeHalo(const SparseMatrix& A, Vector& x, int use_ibarrier)
{

    // Extract Matrix pieces
    local_int_t localNumberOfRows = A.localNumberOfRows;
    int num_neighbors = A.numberOfSendNeighbors;
    local_int_t* receiveLength = A.receiveLength;
    local_int_t* sendLength = A.sendLength;
    int* neighbors = A.neighborsPhysical;
    double* sendBuffer = A.sendBuffer;
    local_int_t totalToBeSent = A.totalToBeSent;
    local_int_t* elementsToSend = A.elementsToSend;

    if (P2P_Mode == MPI_CPU)
    {
        double* const xv = x.values;
        double* x_external = (double*) xv + localNumberOfRows;
        int MPI_MY_TAG = 99;
        MPI_Request* request = new MPI_Request[num_neighbors];

        // Post receives first
        for (int i = 0; i < num_neighbors; i++)
        {
            local_int_t n_recv = receiveLength[i];
            MPI_Irecv(x_external, n_recv, MPI_DOUBLE, neighbors[i], MPI_MY_TAG, MPI_COMM_WORLD, request + i);
            x_external += n_recv;
        }

        for (local_int_t i = 0; i < totalToBeSent; i++)
            sendBuffer[i] = xv[elementsToSend[i]];

        //
        // Send to each neighbor
        //
        for (int i = 0; i < num_neighbors; i++)
        {
            local_int_t n_send = sendLength[i];
            MPI_Send(sendBuffer, n_send, MPI_DOUBLE, neighbors[i], MPI_MY_TAG, MPI_COMM_WORLD);
            sendBuffer += n_send;
        }

        //
        // Complete the reads issued above
        //

        MPI_Waitall(num_neighbors, request, MPI_STATUSES_IGNORE);

        if (use_ibarrier == 1)
            MPI_Ibarrier(MPI_COMM_WORLD, request);

        delete[] request;
    }
    else if (P2P_Mode == MPI_CPU_All2allv)
    {
        double* const xv = x.values;
        double* x_external = (double*) xv + localNumberOfRows;
        for (local_int_t i = 0; i < totalToBeSent; i++)
            sendBuffer[i] = xv[elementsToSend[i]];
        MPI_Alltoallv(
            sendBuffer, A.scounts, A.sdispls, MPI_DOUBLE, x_external, A.rcounts, A.rdispls, MPI_DOUBLE, MPI_COMM_WORLD);
    }
    return;
}
#endif
// ifndef HPCG_NO_MPI

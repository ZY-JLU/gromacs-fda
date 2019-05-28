/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2019, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \internal \file
 * \brief Run integration using CUDA
 *
 * Handles GPU data management and actual numerical integration.
 *
 * \author Artem Zhmurov <zhmurov@gmail.com>
 * \ingroup module_mdlib
 */
#include "gmxpre.h"

#include <assert.h>

#include <cmath>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "gromacs/gpu_utils/devicebuffer.cuh"
#include "gromacs/gpu_utils/gpu_utils.h"
#include "gromacs/math/vec.h"
#include "gromacs/mdlib/leapfrog_cuda.cuh"

#include "leapfrogtest.h"

namespace gmx
{
namespace test
{

void integrateLeapFrogCuda(const int        numAtoms,
                           const rvec      *h_x,
                           rvec            *h_xp,
                           rvec            *h_v,
                           const rvec      *h_f,
                           const real       dt,
                           const int        numSteps,
                           t_mdatoms        mdAtoms)
{
    float3 *d_x, *d_xp, *d_v, *d_f;

    allocateDeviceBuffer(&d_x,  numAtoms, nullptr);
    allocateDeviceBuffer(&d_xp, numAtoms, nullptr);
    allocateDeviceBuffer(&d_v,  numAtoms, nullptr);
    allocateDeviceBuffer(&d_f,  numAtoms, nullptr);

    copyToDeviceBuffer(&d_x,  (float3*)h_x,  0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);
    copyToDeviceBuffer(&d_xp, (float3*)h_xp, 0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);
    copyToDeviceBuffer(&d_v,  (float3*)h_v,  0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);
    copyToDeviceBuffer(&d_f,  (float3*)h_f,  0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);

    auto integrator = std::make_unique<LeapFrogCuda>();

    integrator->set(mdAtoms);

    for (int step = 0; step < numSteps; step++)
    {
        integrator->integrate(d_x, d_xp, d_v, d_f, dt);

        copyFromDeviceBuffer((float3*)h_xp, &d_xp, 0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);
        copyToDeviceBuffer(&d_x,    (float3*)h_xp, 0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);
    }

    copyFromDeviceBuffer((float3*)h_xp, &d_xp, 0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);
    copyFromDeviceBuffer((float3*)h_v,  &d_v,  0, numAtoms, nullptr, GpuApiCallBehavior::Sync, nullptr);

    freeDeviceBuffer(&d_x);
    freeDeviceBuffer(&d_xp);
    freeDeviceBuffer(&d_v);
    freeDeviceBuffer(&d_f);
}

}   // namespace test
}   // namespace gmx

/**********************************************************************
*                                                                     *
*  Copyright 2015 Max Planck Institute                                *
*                 for Dynamics and Self-Organization                  *
*                                                                     *
*  This file is part of bfps.                                         *
*                                                                     *
*  bfps is free software: you can redistribute it and/or modify       *
*  it under the terms of the GNU General Public License as published  *
*  by the Free Software Foundation, either version 3 of the License,  *
*  or (at your option) any later version.                             *
*                                                                     *
*  bfps is distributed in the hope that it will be useful,            *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
*  GNU General Public License for more details.                       *
*                                                                     *
*  You should have received a copy of the GNU General Public License  *
*  along with bfps.  If not, see <http://www.gnu.org/licenses/>       *
*                                                                     *
* Contact: Cristian.Lalescu@ds.mpg.de                                 *
*                                                                     *
**********************************************************************/

// code is generally compiled via setuptools, therefore NDEBUG is present
//#ifdef NDEBUG
//#undef NDEBUG
//#endif//NDEBUG


#include <cmath>
#include "base.hpp"
#include "fftw_tools.hpp"
#include "tracers.hpp"

template <class rnumber>
void tracers<rnumber>::jump_estimate(double *jump)
{
    DEBUG_MSG("entered jump_estimate\n");
    int deriv[] = {0, 0, 0};
    double *tjump = new double[this->nparticles];
    int *xg = new int[this->array_size];
    double *xx = new double[this->array_size];
    rnumber *vel = this->data + this->buffer_size;
    double tmp[3];
    /* get grid coordinates */
    this->get_grid_coordinates(this->state, xg, xx);
    DEBUG_MSG("finished get_grid_coordinate\n");

    std::fill_n(tjump, this->nparticles, 0.0);
    /* perform interpolation */
    for (int p=0; p<this->nparticles; p++) if (this->fs->rd->myrank == this->computing[p])
    {
        DEBUG_MSG("particle %d, about to call linear_interpolation\n", p);
        //this->linear_interpolation(vel, xg + p*3, xx + p*3, tmp, deriv);
        this->spline_formula(vel, xg + p*3, xx + p*3, tmp, deriv);
        tjump[p] = fabs(2*this->dt * tmp[2]);
    }
    delete[] xg;
    delete[] xx;
    MPI_Allreduce(
            tjump,
            jump,
            this->nparticles,
            MPI_DOUBLE,
            MPI_SUM,
            this->fs->rd->comm);
    delete[] tjump;
    DEBUG_MSG("exiting jump_estimate\n");
}

template <class rnumber>
void tracers<rnumber>::get_rhs(double *x, double *y)
{
    std::fill_n(y, this->array_size, 0.0);
    int deriv[] = {0, 0, 0};
    /* get grid coordinates */
    int *xg = new int[this->array_size];
    double *xx = new double[this->array_size];
    rnumber *vel = this->data + this->buffer_size;
    this->get_grid_coordinates(x, xg, xx);
    /* perform interpolation */
    for (int p=0; p<this->nparticles; p++) if (this->fs->rd->myrank == this->computing[p])
    {
        this->spline_formula(vel, xg + p*3, xx + p*3, y + p*3, deriv);
        DEBUG_MSG(
                "particle %d position %lg %lg %lg, i.e. %d %d %d %lg %lg %lg, found y %lg %lg %lg\n",
                p,
                x[p*3], x[p*3+1], x[p*3+2],
                xg[p*3], xg[p*3+1], xg[p*3+2],
                xx[p*3], xx[p*3+1], xx[p*3+2],
                y[p*3], y[p*3+1], y[p*3+2]);
    }
    delete[] xg;
    delete[] xx;
}

template<class rnumber>
void tracers<rnumber>::update_field(bool clip_on)
{
    if (clip_on)
        clip_zero_padding<rnumber>(this->fs->rd, this->source_data, 3);
    this->rFFTW_to_buffered(this->source_data, this->data);
}

/*****************************************************************************/
/* macro for specializations to numeric types compatible with FFTW           */

#define TRACERS_DEFINITIONS(FFTW, R, MPI_RNUM, MPI_CNUM) \
 \
template <> \
tracers<R>::tracers( \
                const char *NAME, \
                fluid_solver_base<R> *FSOLVER, \
                const int NPARTICLES, \
                base_polynomial_values BETA_POLYS, \
                const int NEIGHBOURS, \
                const int SMOOTHNESS, \
                const int TRAJ_SKIP, \
                const int INTEGRATION_STEPS, \
                R *SOURCE_DATA) : slab_field_particles<R>( \
                    NAME, \
                    FSOLVER, \
                    NPARTICLES, \
                    3, \
                    BETA_POLYS, \
                    NEIGHBOURS, \
                    SMOOTHNESS, \
                    TRAJ_SKIP, \
                    INTEGRATION_STEPS) \
{ \
    this->source_data = SOURCE_DATA; \
    this->data = FFTW(alloc_real)(this->buffered_field_descriptor->local_size); \
} \
 \
template<> \
tracers<R>::~tracers() \
{ \
    FFTW(free)(this->data); \
} \
 \
template <> \
void tracers<R>::sample_vec_field(R *vec_field, double *vec_values) \
{ \
    vec_field += this->buffer_size; \
    double *vec_local =  new double[this->array_size]; \
    std::fill_n(vec_local, this->array_size, 0.0); \
    int deriv[] = {0, 0, 0}; \
    /* get grid coordinates */ \
    int *xg = new int[this->array_size]; \
    double *xx = new double[this->array_size]; \
    this->get_grid_coordinates(this->state, xg, xx); \
    /* perform interpolation */ \
    for (int p=0; p<this->nparticles; p++) \
        if (this->fs->rd->myrank == this->computing[p]) \
        { \
            this->spline_formula(vec_field, \
                                 xg + p*3, \
                                 xx + p*3, \
                                 vec_local + p*3, \
                                 deriv); \
        } \
    MPI_Allreduce( \
            vec_local, \
            vec_values, \
            this->array_size, \
            MPI_DOUBLE, \
            MPI_SUM, \
            this->fs->rd->comm); \
    delete[] xg; \
    delete[] xx; \
    delete[] vec_local; \
} \

/*****************************************************************************/



/*****************************************************************************/
/* now actually use the macro defined above                                  */
TRACERS_DEFINITIONS(
        FFTW_MANGLE_FLOAT,
        float,
        MPI_FLOAT,
        MPI_COMPLEX)
TRACERS_DEFINITIONS(
        FFTW_MANGLE_DOUBLE,
        double,
        MPI_DOUBLE,
        BFPS_MPICXX_DOUBLE_COMPLEX)
/*****************************************************************************/



/*****************************************************************************/
/* finally, force generation of code                                         */
template class tracers<float>;
template class tracers<double>;
/*****************************************************************************/


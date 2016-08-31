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



//#define NDEBUG

#include <cassert>
#include <cmath>
#include <cstring>
#include "fluid_solver.hpp"
#include "fftw_tools.hpp"



template <class rnumber>
void fluid_solver<rnumber>::impose_zero_modes()
{
    if (this->cd->myrank == this->cd->rank[0])
    {
        std::fill_n((rnumber*)(this->cu), 6, 0.0);
        std::fill_n((rnumber*)(this->cv[0]), 6, 0.0);
        std::fill_n((rnumber*)(this->cv[1]), 6, 0.0);
        std::fill_n((rnumber*)(this->cv[2]), 6, 0.0);
    }
}
/*****************************************************************************/
/* macro for specializations to numeric types compatible with FFTW           */

#define FLUID_SOLVER_DEFINITIONS(FFTW, R, MPI_RNUM, MPI_CNUM) \
 \
template<> \
fluid_solver<R>::fluid_solver( \
        const char *NAME, \
        int nx, \
        int ny, \
        int nz, \
        double DKX, \
        double DKY, \
        double DKZ, \
        int DEALIAS_TYPE, \
        unsigned FFTW_PLAN_RIGOR) : fluid_solver_base<R>( \
                NAME, \
                nx , ny , nz, \
                DKX, DKY, DKZ, \
                DEALIAS_TYPE, \
                FFTW_PLAN_RIGOR) \
{ \
    this->cvorticity = FFTW(alloc_complex)(this->cd->local_size);\
    this->cvelocity  = FFTW(alloc_complex)(this->cd->local_size);\
    this->rvorticity = FFTW(alloc_real)(this->cd->local_size*2);\
    /*this->rvelocity  = (R*)(this->cvelocity);*/\
    this->rvelocity  = FFTW(alloc_real)(this->cd->local_size*2);\
 \
    this->ru = this->rvelocity;\
    this->cu = this->cvelocity;\
 \
    this->rv[0] = this->rvorticity;\
    this->rv[3] = this->rvorticity;\
    this->cv[0] = this->cvorticity;\
    this->cv[3] = this->cvorticity;\
 \
    this->cv[1] = FFTW(alloc_complex)(this->cd->local_size);\
    this->cv[2] = this->cv[1];\
    this->rv[1] = FFTW(alloc_real)(this->cd->local_size*2);\
    this->rv[2] = this->rv[1];\
 \
    this->c2r_vorticity = new FFTW(plan);\
    this->r2c_vorticity = new FFTW(plan);\
    this->c2r_velocity  = new FFTW(plan);\
    this->r2c_velocity  = new FFTW(plan);\
 \
    ptrdiff_t sizes[] = {nz, \
                         ny, \
                         nx};\
 \
    *(FFTW(plan)*)this->c2r_vorticity = FFTW(mpi_plan_many_dft_c2r)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->cvorticity, this->rvorticity, \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_IN); \
 \
    *(FFTW(plan)*)this->r2c_vorticity = FFTW(mpi_plan_many_dft_r2c)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->rvorticity, this->cvorticity, \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_OUT); \
 \
    *(FFTW(plan)*)this->c2r_velocity = FFTW(mpi_plan_many_dft_c2r)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->cvelocity, this->rvelocity, \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_IN); \
 \
    *(FFTW(plan)*)this->r2c_velocity = FFTW(mpi_plan_many_dft_r2c)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->rvelocity, this->cvelocity, \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_OUT); \
 \
    this->uc2r = this->c2r_velocity;\
    this->ur2c = this->r2c_velocity;\
    this->vc2r[0] = this->c2r_vorticity;\
    this->vr2c[0] = this->r2c_vorticity;\
 \
    this->vc2r[1] = new FFTW(plan);\
    this->vr2c[1] = new FFTW(plan);\
    this->vc2r[2] = new FFTW(plan);\
    this->vr2c[2] = new FFTW(plan);\
 \
    *(FFTW(plan)*)(this->vc2r[1]) = FFTW(mpi_plan_many_dft_c2r)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->cv[1], this->rv[1], \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_IN); \
 \
    *(FFTW(plan)*)this->vc2r[2] = FFTW(mpi_plan_many_dft_c2r)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->cv[2], this->rv[2], \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_IN); \
 \
    *(FFTW(plan)*)this->vr2c[1] = FFTW(mpi_plan_many_dft_r2c)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->rv[1], this->cv[1], \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_OUT); \
 \
    *(FFTW(plan)*)this->vr2c[2] = FFTW(mpi_plan_many_dft_r2c)( \
            3, sizes, 3, FFTW_MPI_DEFAULT_BLOCK, FFTW_MPI_DEFAULT_BLOCK, \
            this->rv[2], this->cv[2], \
            MPI_COMM_WORLD, this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_OUT); \
 \
    /* ``physical'' parameters etc, initialized here just in case */ \
 \
    this->nu = 0.1; \
    this->fmode = 1; \
    this->famplitude = 1.0; \
    this->fk0  = 0; \
    this->fk1 = 3.0; \
    /* initialization of fields must be done AFTER planning */ \
    std::fill_n((R*)this->cvorticity, this->cd->local_size*2, 0.0); \
    std::fill_n((R*)this->cvelocity, this->cd->local_size*2, 0.0); \
    std::fill_n(this->rvelocity, this->cd->local_size*2, 0.0); \
    std::fill_n(this->rvorticity, this->cd->local_size*2, 0.0); \
    std::fill_n((R*)this->cv[1], this->cd->local_size*2, 0.0); \
    std::fill_n(this->rv[1], this->cd->local_size*2, 0.0); \
    std::fill_n(this->rv[2], this->cd->local_size*2, 0.0); \
} \
 \
template<> \
fluid_solver<R>::~fluid_solver() \
{ \
    FFTW(destroy_plan)(*(FFTW(plan)*)this->c2r_vorticity);\
    FFTW(destroy_plan)(*(FFTW(plan)*)this->r2c_vorticity);\
    FFTW(destroy_plan)(*(FFTW(plan)*)this->c2r_velocity );\
    FFTW(destroy_plan)(*(FFTW(plan)*)this->r2c_velocity );\
    FFTW(destroy_plan)(*(FFTW(plan)*)this->vc2r[1]);\
    FFTW(destroy_plan)(*(FFTW(plan)*)this->vr2c[1]);\
    FFTW(destroy_plan)(*(FFTW(plan)*)this->vc2r[2]);\
    FFTW(destroy_plan)(*(FFTW(plan)*)this->vr2c[2]);\
 \
    delete (FFTW(plan)*)this->c2r_vorticity;\
    delete (FFTW(plan)*)this->r2c_vorticity;\
    delete (FFTW(plan)*)this->c2r_velocity ;\
    delete (FFTW(plan)*)this->r2c_velocity ;\
    delete (FFTW(plan)*)this->vc2r[1];\
    delete (FFTW(plan)*)this->vr2c[1];\
    delete (FFTW(plan)*)this->vc2r[2];\
    delete (FFTW(plan)*)this->vr2c[2];\
 \
    FFTW(free)(this->cv[1]);\
    FFTW(free)(this->rv[1]);\
    FFTW(free)(this->cvorticity);\
    FFTW(free)(this->rvorticity);\
    FFTW(free)(this->cvelocity);\
    FFTW(free)(this->rvelocity);\
} \
 \
template<> \
void fluid_solver<R>::compute_vorticity() \
{ \
    ptrdiff_t tindex; \
    CLOOP_K2( \
            this, \
            tindex = 3*cindex; \
            if (k2 <= this->kM2) \
            { \
                this->cvorticity[tindex+0][0] = -(this->ky[yindex]*this->cu[tindex+2][1] - this->kz[zindex]*this->cu[tindex+1][1]); \
                this->cvorticity[tindex+1][0] = -(this->kz[zindex]*this->cu[tindex+0][1] - this->kx[xindex]*this->cu[tindex+2][1]); \
                this->cvorticity[tindex+2][0] = -(this->kx[xindex]*this->cu[tindex+1][1] - this->ky[yindex]*this->cu[tindex+0][1]); \
                this->cvorticity[tindex+0][1] =  (this->ky[yindex]*this->cu[tindex+2][0] - this->kz[zindex]*this->cu[tindex+1][0]); \
                this->cvorticity[tindex+1][1] =  (this->kz[zindex]*this->cu[tindex+0][0] - this->kx[xindex]*this->cu[tindex+2][0]); \
                this->cvorticity[tindex+2][1] =  (this->kx[xindex]*this->cu[tindex+1][0] - this->ky[yindex]*this->cu[tindex+0][0]); \
            } \
            else \
                std::fill_n((R*)(this->cvorticity+tindex), 6, 0.0); \
            ); \
    this->symmetrize(this->cvorticity, 3); \
} \
 \
template<> \
void fluid_solver<R>::compute_velocity(FFTW(complex) *vorticity) \
{ \
    ptrdiff_t tindex; \
    CLOOP_K2( \
            this, \
            tindex = 3*cindex; \
            if (k2 <= this->kM2 && k2 > 0) \
            { \
                this->cu[tindex+0][0] = -(this->ky[yindex]*vorticity[tindex+2][1] - this->kz[zindex]*vorticity[tindex+1][1]) / k2; \
                this->cu[tindex+1][0] = -(this->kz[zindex]*vorticity[tindex+0][1] - this->kx[xindex]*vorticity[tindex+2][1]) / k2; \
                this->cu[tindex+2][0] = -(this->kx[xindex]*vorticity[tindex+1][1] - this->ky[yindex]*vorticity[tindex+0][1]) / k2; \
                this->cu[tindex+0][1] =  (this->ky[yindex]*vorticity[tindex+2][0] - this->kz[zindex]*vorticity[tindex+1][0]) / k2; \
                this->cu[tindex+1][1] =  (this->kz[zindex]*vorticity[tindex+0][0] - this->kx[xindex]*vorticity[tindex+2][0]) / k2; \
                this->cu[tindex+2][1] =  (this->kx[xindex]*vorticity[tindex+1][0] - this->ky[yindex]*vorticity[tindex+0][0]) / k2; \
            } \
            else \
                std::fill_n((R*)(this->cu+tindex), 6, 0.0); \
            ); \
    /*this->symmetrize(this->cu, 3);*/ \
} \
 \
template<> \
void fluid_solver<R>::ift_velocity() \
{ \
    FFTW(execute)(*((FFTW(plan)*)this->c2r_velocity )); \
} \
 \
template<> \
void fluid_solver<R>::ift_vorticity() \
{ \
    std::fill_n(this->rvorticity, this->cd->local_size*2, 0.0); \
    FFTW(execute)(*((FFTW(plan)*)this->c2r_vorticity )); \
} \
 \
template<> \
void fluid_solver<R>::dft_velocity() \
{ \
    FFTW(execute)(*((FFTW(plan)*)this->r2c_velocity )); \
} \
 \
template<> \
void fluid_solver<R>::dft_vorticity() \
{ \
    std::fill_n((R*)this->cvorticity, this->cd->local_size*2, 0.0); \
    FFTW(execute)(*((FFTW(plan)*)this->r2c_vorticity )); \
} \
 \
template<> \
void fluid_solver<R>::add_forcing(\
        FFTW(complex) *acc_field, FFTW(complex) *vort_field, R factor) \
{ \
    if (strcmp(this->forcing_type, "none") == 0) \
        return; \
    if (strcmp(this->forcing_type, "Kolmogorov") == 0) \
    { \
        ptrdiff_t cindex; \
        if (this->cd->myrank == this->cd->rank[this->fmode]) \
        { \
            cindex = ((this->fmode - this->cd->starts[0]) * this->cd->sizes[1])*this->cd->sizes[2]*3; \
            acc_field[cindex+2][0] -= this->famplitude*factor/2; \
        } \
        if (this->cd->myrank == this->cd->rank[this->cd->sizes[0] - this->fmode]) \
        { \
            cindex = ((this->cd->sizes[0] - this->fmode - this->cd->starts[0]) * this->cd->sizes[1])*this->cd->sizes[2]*3; \
            acc_field[cindex+2][0] -= this->famplitude*factor/2; \
        } \
        return; \
    } \
    if (strcmp(this->forcing_type, "linear") == 0) \
    { \
        double knorm; \
        CLOOP( \
                this, \
                knorm = sqrt(this->kx[xindex]*this->kx[xindex] + \
                             this->ky[yindex]*this->ky[yindex] + \
                             this->kz[zindex]*this->kz[zindex]); \
                if ((this->fk0 <= knorm) && \
                    (this->fk1 >= knorm)) \
                    for (int c=0; c<3; c++) \
                    for (int i=0; i<2; i++) \
                        acc_field[cindex*3+c][i] += this->famplitude*vort_field[cindex*3+c][i]*factor; \
             ); \
        return; \
    } \
} \
 \
template<> \
void fluid_solver<R>::omega_nonlin( \
        int src) \
{ \
    assert(src >= 0 && src < 3); \
    this->compute_velocity(this->cv[src]); \
    /* get fields from Fourier space to real space */ \
    FFTW(execute)(*((FFTW(plan)*)this->c2r_velocity ));  \
    FFTW(execute)(*((FFTW(plan)*)this->vc2r[src]));      \
    /* compute cross product $u \times \omega$, and normalize */ \
    R tmp[3][2]; \
    ptrdiff_t tindex; \
    RLOOP ( \
            this, \
            tindex = 3*rindex; \
            for (int cc=0; cc<3; cc++) \
                tmp[cc][0] = (this->ru[tindex+(cc+1)%3]*this->rv[src][tindex+(cc+2)%3] - \
                              this->ru[tindex+(cc+2)%3]*this->rv[src][tindex+(cc+1)%3]); \
            for (int cc=0; cc<3; cc++) \
                this->ru[(3*rindex)+cc] = tmp[cc][0] / this->normalization_factor; \
            ); \
    /* go back to Fourier space */ \
    this->clean_up_real_space(this->ru, 3); \
    FFTW(execute)(*((FFTW(plan)*)this->r2c_velocity )); \
    this->dealias(this->cu, 3); \
    /* $\imath k \times Fourier(u \times \omega)$ */ \
    CLOOP( \
            this, \
            tindex = 3*cindex; \
            { \
                tmp[0][0] = -(this->ky[yindex]*this->cu[tindex+2][1] - this->kz[zindex]*this->cu[tindex+1][1]); \
                tmp[1][0] = -(this->kz[zindex]*this->cu[tindex+0][1] - this->kx[xindex]*this->cu[tindex+2][1]); \
                tmp[2][0] = -(this->kx[xindex]*this->cu[tindex+1][1] - this->ky[yindex]*this->cu[tindex+0][1]); \
                tmp[0][1] =  (this->ky[yindex]*this->cu[tindex+2][0] - this->kz[zindex]*this->cu[tindex+1][0]); \
                tmp[1][1] =  (this->kz[zindex]*this->cu[tindex+0][0] - this->kx[xindex]*this->cu[tindex+2][0]); \
                tmp[2][1] =  (this->kx[xindex]*this->cu[tindex+1][0] - this->ky[yindex]*this->cu[tindex+0][0]); \
            } \
            for (int cc=0; cc<3; cc++) for (int i=0; i<2; i++) \
                this->cu[tindex+cc][i] = tmp[cc][i]; \
            ); \
    this->add_forcing(this->cu, this->cv[src], 1.0); \
    this->force_divfree(this->cu); \
} \
 \
template<> \
void fluid_solver<R>::step(double dt) \
{ \
    double factor0, factor1; \
    std::fill_n((R*)this->cv[1], this->cd->local_size*2, 0.0); \
    this->omega_nonlin(0); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2) \
            { \
                factor0 = exp(-this->nu * k2 * dt); \
                for (int cc=0; cc<3; cc++) for (int i=0; i<2; i++) \
                    this->cv[1][3*cindex+cc][i] = (this->cv[0][3*cindex+cc][i] + \
                                                   dt*this->cu[3*cindex+cc][i])*factor0; \
            } \
            ); \
 \
    this->omega_nonlin(1); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2) \
            { \
                factor0 = exp(-this->nu * k2 * dt/2); \
                factor1 = exp( this->nu * k2 * dt/2); \
                for (int cc=0; cc<3; cc++) for (int i=0; i<2; i++) \
                    this->cv[2][3*cindex+cc][i] = (3*this->cv[0][3*cindex+cc][i]*factor0 + \
                                                   (this->cv[1][3*cindex+cc][i] + \
                                                    dt*this->cu[3*cindex+cc][i])*factor1)*0.25; \
            } \
            ); \
 \
    this->omega_nonlin(2); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2) \
            { \
                factor0 = exp(-this->nu * k2 * dt * 0.5); \
                for (int cc=0; cc<3; cc++) for (int i=0; i<2; i++) \
                    this->cv[3][3*cindex+cc][i] = (this->cv[0][3*cindex+cc][i]*factor0 + \
                                                   2*(this->cv[2][3*cindex+cc][i] + \
                                                      dt*this->cu[3*cindex+cc][i]))*factor0/3; \
            } \
            ); \
 \
    this->force_divfree(this->cvorticity); \
    this->symmetrize(this->cvorticity, 3); \
    this->iteration++; \
} \
 \
template<> \
int fluid_solver<R>::read(char field, char representation) \
{ \
    char fname[512]; \
    int read_result; \
    if (field == 'v') \
    { \
        if (representation == 'c') \
        { \
            this->fill_up_filename("cvorticity", fname); \
            read_result = this->cd->read(fname, (void*)this->cvorticity); \
            if (read_result != EXIT_SUCCESS) \
                return read_result; \
        } \
        if (representation == 'r') \
        { \
            read_result = this->read_base("rvorticity", this->rvorticity); \
            if (read_result != EXIT_SUCCESS) \
                return read_result; \
            else \
                FFTW(execute)(*((FFTW(plan)*)this->r2c_vorticity )); \
        } \
        this->low_pass_Fourier(this->cvorticity, 3, this->kM); \
        this->force_divfree(this->cvorticity); \
        this->symmetrize(this->cvorticity, 3); \
        return EXIT_SUCCESS; \
    } \
    if ((field == 'u') && (representation == 'c')) \
    { \
        read_result = this->read_base("cvelocity", this->cvelocity); \
        this->low_pass_Fourier(this->cvelocity, 3, this->kM); \
        this->force_divfree(this->cvorticity); \
        this->symmetrize(this->cvorticity, 3); \
        return read_result; \
    } \
    if ((field == 'u') && (representation == 'r')) \
        return this->read_base("rvelocity", this->rvelocity); \
    return EXIT_FAILURE; \
} \
 \
template<> \
int fluid_solver<R>::write(char field, char representation) \
{ \
    char fname[512]; \
    if ((field == 'v') && (representation == 'c')) \
    { \
        this->fill_up_filename("cvorticity", fname); \
        return this->cd->write(fname, (void*)this->cvorticity); \
    } \
    if ((field == 'v') && (representation == 'r')) \
    { \
        FFTW(execute)(*((FFTW(plan)*)this->c2r_vorticity )); \
        clip_zero_padding<R>(this->rd, this->rvorticity, 3); \
        this->fill_up_filename("rvorticity", fname); \
        return this->rd->write(fname, this->rvorticity); \
    } \
    this->compute_velocity(this->cvorticity); \
    if ((field == 'u') && (representation == 'c')) \
    { \
        this->fill_up_filename("cvelocity", fname); \
        return this->cd->write(fname, this->cvelocity); \
    } \
    if ((field == 'u') && (representation == 'r')) \
    { \
        this->ift_velocity(); \
        clip_zero_padding<R>(this->rd, this->rvelocity, 3); \
        this->fill_up_filename("rvelocity", fname); \
        return this->rd->write(fname, this->rvelocity); \
    } \
    return EXIT_FAILURE; \
} \
 \
template<> \
int fluid_solver<R>::write_rTrS2() \
{ \
    char fname[512]; \
    this->fill_up_filename("rTrS2", fname); \
    FFTW(complex) *ca; \
    R *ra; \
    ca = FFTW(alloc_complex)(this->cd->local_size*3); \
    ra = (R*)(ca); \
    this->compute_velocity(this->cvorticity); \
    this->compute_vector_gradient(ca, this->cvelocity); \
    for (int cc=0; cc<3; cc++) \
    { \
        std::copy( \
                (R*)(ca + cc*this->cd->local_size), \
                (R*)(ca + (cc+1)*this->cd->local_size), \
                (R*)this->cv[1]); \
        FFTW(execute)(*((FFTW(plan)*)this->vc2r[1])); \
        std::copy( \
                this->rv[1], \
                this->rv[1] + this->cd->local_size*2, \
                ra + cc*this->cd->local_size*2); \
    } \
    /* velocity gradient is now stored, in real space, in ra */ \
    R *dx_u, *dy_u, *dz_u; \
    dx_u = ra; \
    dy_u = ra + 2*this->cd->local_size; \
    dz_u = ra + 4*this->cd->local_size; \
    R *trS2 = FFTW(alloc_real)((this->cd->local_size/3)*2); \
    double average_local = 0; \
    RLOOP( \
            this, \
            R AxxAxx; \
            R AyyAyy; \
            R AzzAzz; \
            R Sxy; \
            R Syz; \
            R Szx; \
            ptrdiff_t tindex = 3*rindex; \
            AxxAxx = dx_u[tindex+0]*dx_u[tindex+0]; \
            AyyAyy = dy_u[tindex+1]*dy_u[tindex+1]; \
            AzzAzz = dz_u[tindex+2]*dz_u[tindex+2]; \
            Sxy = dx_u[tindex+1]+dy_u[tindex+0]; \
            Syz = dy_u[tindex+2]+dz_u[tindex+1]; \
            Szx = dz_u[tindex+0]+dx_u[tindex+2]; \
            trS2[rindex] = (AxxAxx + AyyAyy + AzzAzz + \
                            (Sxy*Sxy + Syz*Syz + Szx*Szx)/2); \
            average_local += trS2[rindex]; \
            ); \
    double average; \
    MPI_Allreduce( \
            &average_local, \
            &average, \
            1, \
            MPI_DOUBLE, MPI_SUM, this->cd->comm); \
    DEBUG_MSG("average TrS2 is %g\n", average); \
    FFTW(free)(ca); \
    /* output goes here */ \
    int ntmp[3]; \
    ntmp[0] = this->rd->sizes[0]; \
    ntmp[1] = this->rd->sizes[1]; \
    ntmp[2] = this->rd->sizes[2]; \
    field_descriptor<R> *scalar_descriptor = new field_descriptor<R>(3, ntmp, MPI_RNUM, this->cd->comm); \
    clip_zero_padding<R>(scalar_descriptor, trS2, 1); \
    int return_value = scalar_descriptor->write(fname, trS2); \
    delete scalar_descriptor; \
    FFTW(free)(trS2); \
    return return_value; \
} \
 \
template<> \
int fluid_solver<R>::write_renstrophy() \
{ \
    char fname[512]; \
    this->fill_up_filename("renstrophy", fname); \
    R *enstrophy = FFTW(alloc_real)((this->cd->local_size/3)*2); \
    this->ift_vorticity(); \
    double average_local = 0; \
    RLOOP( \
            this, \
            ptrdiff_t tindex = 3*rindex; \
            enstrophy[rindex] = ( \
                this->rvorticity[tindex+0]*this->rvorticity[tindex+0] + \
                this->rvorticity[tindex+1]*this->rvorticity[tindex+1] + \
                this->rvorticity[tindex+2]*this->rvorticity[tindex+2] \
                )/2; \
            average_local += enstrophy[rindex]; \
            ); \
    double average; \
    MPI_Allreduce( \
            &average_local, \
            &average, \
            1, \
            MPI_DOUBLE, MPI_SUM, this->cd->comm); \
    DEBUG_MSG("average enstrophy is %g\n", average); \
    /* output goes here */ \
    int ntmp[3]; \
    ntmp[0] = this->rd->sizes[0]; \
    ntmp[1] = this->rd->sizes[1]; \
    ntmp[2] = this->rd->sizes[2]; \
    field_descriptor<R> *scalar_descriptor = new field_descriptor<R>(3, ntmp, MPI_RNUM, this->cd->comm); \
    clip_zero_padding<R>(scalar_descriptor, enstrophy, 1); \
    int return_value = scalar_descriptor->write(fname, enstrophy); \
    delete scalar_descriptor; \
    FFTW(free)(enstrophy); \
    return return_value; \
} \
 \
template<> \
void fluid_solver<R>::compute_pressure(FFTW(complex) *pressure) \
{ \
    /* assume velocity is already in real space representation */ \
    ptrdiff_t tindex; \
    \
    /* diagonal terms 11 22 33 */\
    RLOOP ( \
            this, \
            tindex = 3*rindex; \
            for (int cc=0; cc<3; cc++) \
                this->rv[1][tindex+cc] = this->ru[tindex+cc]*this->ru[tindex+cc]; \
            ); \
    this->clean_up_real_space(this->rv[1], 3); \
    FFTW(execute)(*((FFTW(plan)*)this->vr2c[1])); \
    this->dealias(this->cv[1], 3); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2 && k2 > 0) \
            { \
                tindex = 3*cindex; \
                for (int i=0; i<2; i++) \
                { \
                    pressure[cindex][i] = -(this->kx[xindex]*this->kx[xindex]*this->cv[1][tindex+0][i] + \
                                            this->ky[yindex]*this->ky[yindex]*this->cv[1][tindex+1][i] + \
                                            this->kz[zindex]*this->kz[zindex]*this->cv[1][tindex+2][i]); \
                } \
            } \
            else \
                std::fill_n((R*)(pressure+cindex), 2, 0.0); \
            ); \
    /* off-diagonal terms 12 23 31 */\
    RLOOP ( \
            this, \
            tindex = 3*rindex; \
            for (int cc=0; cc<3; cc++) \
                this->rv[1][tindex+cc] = this->ru[tindex+cc]*this->ru[tindex+(cc+1)%3]; \
            ); \
    this->clean_up_real_space(this->rv[1], 3); \
    FFTW(execute)(*((FFTW(plan)*)this->vr2c[1])); \
    this->dealias(this->cv[1], 3); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2 && k2 > 0) \
            { \
                tindex = 3*cindex; \
                for (int i=0; i<2; i++) \
                { \
                    pressure[cindex][i] -= 2*(this->kx[xindex]*this->ky[yindex]*this->cv[1][tindex+0][i] + \
                                              this->ky[yindex]*this->kz[zindex]*this->cv[1][tindex+1][i] + \
                                              this->kz[zindex]*this->kx[xindex]*this->cv[1][tindex+2][i]); \
                    pressure[cindex][i] /= this->normalization_factor*k2; \
                } \
            } \
            ); \
} \
 \
template<> \
void fluid_solver<R>::compute_gradient_statistics( \
        FFTW(complex) *vec, \
        double *gradu_moments, \
        double *trS2QR_moments, \
        ptrdiff_t *gradu_hist, \
        ptrdiff_t *trS2QR_hist, \
        ptrdiff_t *QR2D_hist, \
        double trS2QR_max_estimates[], \
        double gradu_max_estimates[], \
        int nbins, \
        int QR2D_nbins) \
{ \
    FFTW(complex) *ca; \
    R *ra; \
    ca = FFTW(alloc_complex)(this->cd->local_size*3); \
    ra = (R*)(ca); \
    this->compute_vector_gradient(ca, vec); \
    for (int cc=0; cc<3; cc++) \
    { \
        std::copy( \
                (R*)(ca + cc*this->cd->local_size), \
                (R*)(ca + (cc+1)*this->cd->local_size), \
                (R*)this->cv[1]); \
        FFTW(execute)(*((FFTW(plan)*)this->vc2r[1])); \
        std::copy( \
                this->rv[1], \
                this->rv[1] + this->cd->local_size*2, \
                ra + cc*this->cd->local_size*2); \
    } \
    /* velocity gradient is now stored, in real space, in ra */ \
    std::fill_n(this->rv[1], 2*this->cd->local_size, 0.0); \
    R *dx_u, *dy_u, *dz_u; \
    dx_u = ra; \
    dy_u = ra + 2*this->cd->local_size; \
    dz_u = ra + 4*this->cd->local_size; \
    double binsize[2]; \
    double tmp_max_estimate[3]; \
    tmp_max_estimate[0] = trS2QR_max_estimates[0]; \
    tmp_max_estimate[1] = trS2QR_max_estimates[1]; \
    tmp_max_estimate[2] = trS2QR_max_estimates[2]; \
    binsize[0] = 2*tmp_max_estimate[2] / QR2D_nbins; \
    binsize[1] = 2*tmp_max_estimate[1] / QR2D_nbins; \
    ptrdiff_t *local_hist = new ptrdiff_t[QR2D_nbins*QR2D_nbins]; \
    std::fill_n(local_hist, QR2D_nbins*QR2D_nbins, 0); \
    RLOOP( \
            this, \
            R AxxAxx; \
            R AyyAyy; \
            R AzzAzz; \
            R AxyAyx; \
            R AyzAzy; \
            R AzxAxz; \
            R Sxy; \
            R Syz; \
            R Szx; \
            ptrdiff_t tindex = 3*rindex; \
            AxxAxx = dx_u[tindex+0]*dx_u[tindex+0]; \
            AyyAyy = dy_u[tindex+1]*dy_u[tindex+1]; \
            AzzAzz = dz_u[tindex+2]*dz_u[tindex+2]; \
            AxyAyx = dx_u[tindex+1]*dy_u[tindex+0]; \
            AyzAzy = dy_u[tindex+2]*dz_u[tindex+1]; \
            AzxAxz = dz_u[tindex+0]*dx_u[tindex+2]; \
            this->rv[1][tindex+1] = - (AxxAxx + AyyAyy + AzzAzz)/2 - AxyAyx - AyzAzy - AzxAxz; \
            this->rv[1][tindex+2] = - (dx_u[tindex+0]*(AxxAxx/3 + AxyAyx + AzxAxz) + \
                                       dy_u[tindex+1]*(AyyAyy/3 + AxyAyx + AyzAzy) + \
                                       dz_u[tindex+2]*(AzzAzz/3 + AzxAxz + AyzAzy) + \
                                       dx_u[tindex+1]*dy_u[tindex+2]*dz_u[tindex+0] + \
                                       dx_u[tindex+2]*dy_u[tindex+0]*dz_u[tindex+1]); \
            int bin0 = int(floor((this->rv[1][tindex+2] + tmp_max_estimate[2]) / binsize[0])); \
            int bin1 = int(floor((this->rv[1][tindex+1] + tmp_max_estimate[1]) / binsize[1])); \
            if ((bin0 >= 0 && bin0 < QR2D_nbins) && \
                (bin1 >= 0 && bin1 < QR2D_nbins)) \
                local_hist[bin1*QR2D_nbins + bin0]++; \
            Sxy = dx_u[tindex+1]+dy_u[tindex+0]; \
            Syz = dy_u[tindex+2]+dz_u[tindex+1]; \
            Szx = dz_u[tindex+0]+dx_u[tindex+2]; \
            this->rv[1][tindex] = (AxxAxx + AyyAyy + AzzAzz + \
                                   (Sxy*Sxy + Syz*Syz + Szx*Szx)/2); \
            ); \
    MPI_Allreduce( \
            local_hist, \
            QR2D_hist, \
            QR2D_nbins * QR2D_nbins, \
            MPI_INT64_T, MPI_SUM, this->cd->comm); \
    delete[] local_hist; \
    this->compute_rspace_stats3( \
            this->rv[1], \
            trS2QR_moments, \
            trS2QR_hist, \
            tmp_max_estimate, \
            nbins); \
    double *tmp_moments = new double[10*3]; \
    ptrdiff_t *tmp_hist = new ptrdiff_t[nbins*3]; \
    for (int cc=0; cc<3; cc++) \
    { \
        tmp_max_estimate[0] = gradu_max_estimates[cc*3 + 0]; \
        tmp_max_estimate[1] = gradu_max_estimates[cc*3 + 1]; \
        tmp_max_estimate[2] = gradu_max_estimates[cc*3 + 2]; \
        this->compute_rspace_stats3( \
                dx_u, \
                tmp_moments, \
                tmp_hist, \
                tmp_max_estimate, \
                nbins); \
        for (int n = 0; n < 10; n++) \
        for (int i = 0; i < 3 ; i++) \
        { \
            gradu_moments[(n*3 + cc)*3 + i] = tmp_moments[n*3 + i]; \
        } \
        for (int n = 0; n < nbins; n++) \
        for (int i = 0; i < 3; i++) \
        { \
            gradu_hist[(n*3 + cc)*3 + i] = tmp_hist[n*3 + i]; \
        } \
    } \
    delete[] tmp_moments; \
    delete[] tmp_hist; \
    FFTW(free)(ca); \
} \
 \
template<> \
void fluid_solver<R>::compute_Lagrangian_acceleration(R (*acceleration)[2]) \
{ \
    ptrdiff_t tindex; \
    FFTW(complex) *pressure; \
    pressure = FFTW(alloc_complex)(this->cd->local_size/3); \
    this->compute_velocity(this->cvorticity); \
    this->ift_velocity(); \
    this->compute_pressure(pressure); \
    this->compute_velocity(this->cvorticity); \
    std::fill_n((R*)this->cv[1], 2*this->cd->local_size, 0.0); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2) \
            { \
                tindex = 3*cindex; \
                for (int cc=0; cc<3; cc++) \
                    for (int i=0; i<2; i++) \
                        this->cv[1][tindex+cc][i] = - this->nu*k2*this->cu[tindex+cc][i]; \
                if (strcmp(this->forcing_type, "linear") == 0) \
                { \
                    double knorm = sqrt(k2); \
                    if ((this->fk0 <= knorm) && \
                        (this->fk1 >= knorm)) \
                        for (int c=0; c<3; c++) \
                            for (int i=0; i<2; i++) \
                                this->cv[1][tindex+c][i] += this->famplitude*this->cu[tindex+c][i]; \
                } \
                this->cv[1][tindex+0][0] += this->kx[xindex]*pressure[cindex][1]; \
                this->cv[1][tindex+1][0] += this->ky[yindex]*pressure[cindex][1]; \
                this->cv[1][tindex+2][0] += this->kz[zindex]*pressure[cindex][1]; \
                this->cv[1][tindex+0][1] -= this->kx[xindex]*pressure[cindex][0]; \
                this->cv[1][tindex+1][1] -= this->ky[yindex]*pressure[cindex][0]; \
                this->cv[1][tindex+2][1] -= this->kz[zindex]*pressure[cindex][0]; \
            } \
            ); \
    std::copy( \
            (R*)this->cv[1], \
            (R*)(this->cv[1] + this->cd->local_size), \
            (R*)acceleration); \
    FFTW(free)(pressure); \
} \
 \
template<> \
void fluid_solver<R>::compute_Eulerian_acceleration(FFTW(complex) *acceleration) \
{ \
    std::fill_n((R*)(acceleration), 2*this->cd->local_size, 0.0); \
    ptrdiff_t tindex; \
    this->compute_velocity(this->cvorticity); \
    /* put in linear terms */ \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2) \
            { \
                tindex = 3*cindex; \
                for (int cc=0; cc<3; cc++) \
                    for (int i=0; i<2; i++) \
                        acceleration[tindex+cc][i] = - this->nu*k2*this->cu[tindex+cc][i]; \
                if (strcmp(this->forcing_type, "linear") == 0) \
                { \
                    double knorm = sqrt(k2); \
                    if ((this->fk0 <= knorm) && \
                        (this->fk1 >= knorm)) \
                    { \
                        for (int c=0; c<3; c++) \
                            for (int i=0; i<2; i++) \
                                acceleration[tindex+c][i] += this->famplitude*this->cu[tindex+c][i]; \
                    } \
                } \
            } \
            ); \
    this->ift_velocity(); \
    /* compute uu */ \
    /* 11 22 33 */ \
    RLOOP ( \
            this, \
            tindex = 3*rindex; \
            for (int cc=0; cc<3; cc++) \
                this->rv[1][tindex+cc] = this->ru[tindex+cc]*this->ru[tindex+cc] / this->normalization_factor; \
            ); \
    this->clean_up_real_space(this->rv[1], 3); \
    FFTW(execute)(*((FFTW(plan)*)this->vr2c[1])); \
    this->dealias(this->cv[1], 3); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2) \
            { \
                tindex = 3*cindex; \
                acceleration[tindex+0][0] += \
                         this->kx[xindex]*this->cv[1][tindex+0][1]; \
                acceleration[tindex+0][1] += \
                        -this->kx[xindex]*this->cv[1][tindex+0][0]; \
                acceleration[tindex+1][0] += \
                         this->ky[yindex]*this->cv[1][tindex+1][1]; \
                acceleration[tindex+1][1] += \
                        -this->ky[yindex]*this->cv[1][tindex+1][0]; \
                acceleration[tindex+2][0] += \
                         this->kz[zindex]*this->cv[1][tindex+2][1]; \
                acceleration[tindex+2][1] += \
                        -this->kz[zindex]*this->cv[1][tindex+2][0]; \
            } \
            ); \
    /* 12 23 31 */ \
    RLOOP ( \
            this, \
            tindex = 3*rindex; \
            for (int cc=0; cc<3; cc++) \
                this->rv[1][tindex+cc] = this->ru[tindex+cc]*this->ru[tindex+(cc+1)%3] / this->normalization_factor; \
            ); \
    this->clean_up_real_space(this->rv[1], 3); \
    FFTW(execute)(*((FFTW(plan)*)this->vr2c[1])); \
    this->dealias(this->cv[1], 3); \
    CLOOP_K2( \
            this, \
            if (k2 <= this->kM2) \
            { \
                tindex = 3*cindex; \
                acceleration[tindex+0][0] += \
                        (this->ky[yindex]*this->cv[1][tindex+0][1] + \
                         this->kz[zindex]*this->cv[1][tindex+2][1]); \
                acceleration[tindex+0][1] += \
                      - (this->ky[yindex]*this->cv[1][tindex+0][0] + \
                         this->kz[zindex]*this->cv[1][tindex+2][0]); \
                acceleration[tindex+1][0] += \
                        (this->kz[zindex]*this->cv[1][tindex+1][1] + \
                         this->kx[xindex]*this->cv[1][tindex+0][1]); \
                acceleration[tindex+1][1] += \
                      - (this->kz[zindex]*this->cv[1][tindex+1][0] + \
                         this->kx[xindex]*this->cv[1][tindex+0][0]); \
                acceleration[tindex+2][0] += \
                        (this->kx[xindex]*this->cv[1][tindex+2][1] + \
                         this->ky[yindex]*this->cv[1][tindex+1][1]); \
                acceleration[tindex+2][1] += \
                      - (this->kx[xindex]*this->cv[1][tindex+2][0] + \
                         this->ky[yindex]*this->cv[1][tindex+1][0]); \
            } \
            ); \
    if (this->cd->myrank == this->cd->rank[0]) \
        std::fill_n((R*)(acceleration), 6, 0.0); \
    this->force_divfree(acceleration); \
} \
 \
template<> \
void fluid_solver<R>::compute_Lagrangian_acceleration(R *acceleration) \
{ \
    this->compute_Lagrangian_acceleration((FFTW(complex)*)acceleration); \
    FFTW(execute)(*((FFTW(plan)*)this->vc2r[1])); \
    std::copy( \
            this->rv[1], \
            this->rv[1] + 2*this->cd->local_size, \
            acceleration); \
} \
 \
template<> \
int fluid_solver<R>::write_rpressure() \
{ \
    char fname[512]; \
    FFTW(complex) *pressure; \
    pressure = FFTW(alloc_complex)(this->cd->local_size/3); \
    this->compute_velocity(this->cvorticity); \
    this->ift_velocity(); \
    this->compute_pressure(pressure); \
    this->fill_up_filename("rpressure", fname); \
    R *rpressure = FFTW(alloc_real)((this->cd->local_size/3)*2); \
    FFTW(plan) c2r; \
    c2r = FFTW(mpi_plan_dft_c2r_3d)( \
            this->rd->sizes[0], this->rd->sizes[1], this->rd->sizes[2], \
            pressure, rpressure, this->cd->comm, \
            this->fftw_plan_rigor | FFTW_MPI_TRANSPOSED_IN); \
    FFTW(execute)(c2r); \
    /* output goes here */ \
    int ntmp[3]; \
    ntmp[0] = this->rd->sizes[0]; \
    ntmp[1] = this->rd->sizes[1]; \
    ntmp[2] = this->rd->sizes[2]; \
    field_descriptor<R> *scalar_descriptor = new field_descriptor<R>(3, ntmp, MPI_RNUM, this->cd->comm); \
    clip_zero_padding<R>(scalar_descriptor, rpressure, 1); \
    int return_value = scalar_descriptor->write(fname, rpressure); \
    delete scalar_descriptor; \
    FFTW(destroy_plan)(c2r); \
    FFTW(free)(pressure); \
    FFTW(free)(rpressure); \
    return return_value; \
} \

/*****************************************************************************/



/*****************************************************************************/
/* now actually use the macro defined above                                  */
FLUID_SOLVER_DEFINITIONS(
        FFTW_MANGLE_FLOAT,
        float,
        MPI_FLOAT,
        MPI_COMPLEX)
FLUID_SOLVER_DEFINITIONS(
        FFTW_MANGLE_DOUBLE,
        double,
        MPI_DOUBLE,
        BFPS_MPICXX_DOUBLE_COMPLEX)
/*****************************************************************************/



/*****************************************************************************/
/* finally, force generation of code for single precision                    */
template class fluid_solver<float>;
template class fluid_solver<double>;
/*****************************************************************************/


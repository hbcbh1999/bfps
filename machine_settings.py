#######################################################################
#                                                                     #
#  Copyright 2015 Max Planck Institute                                #
#                 for Dynamics and Self-Organization                  #
#                                                                     #
#  This file is part of bfps.                                         #
#                                                                     #
#  bfps is free software: you can redistribute it and/or modify       #
#  it under the terms of the GNU General Public License as published  #
#  by the Free Software Foundation, either version 3 of the License,  #
#  or (at your option) any later version.                             #
#                                                                     #
#  bfps is distributed in the hope that it will be useful,            #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of     #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      #
#  GNU General Public License for more details.                       #
#                                                                     #
#  You should have received a copy of the GNU General Public License  #
#  along with bfps.  If not, see <http://www.gnu.org/licenses/>       #
#                                                                     #
# Contact: Cristian.Lalescu@ds.mpg.de                                 #
#                                                                     #
#######################################################################



import os

########################################################################
#### these you're supposed to adapt to your environment
########################################################################

hostname = os.getenv('HOSTNAME')

extra_compile_args = ['-Wall', '-O2', '-g', '-mtune=native', '-ffast-math', '-std=c++11']
#extra_compile_args = ['-Wall', '-O0', '-g', '-std=c++11']
extra_libraries = ['hdf5']

if hostname == 'chichi-G':
    include_dirs = ['/usr/local/include',
                    '/usr/include/mpich']
    library_dirs = ['/usr/local/lib'
                    '/usr/lib/mpich']
    extra_libraries += ['mpich']

if hostname in ['frontend01', 'frontend02']:
    include_dirs = ['/usr/nld/mvapich2-1.9a2-gcc/include',
                    '/usr/nld/gcc-4.7.2/include',
                    '/usr/nld/hdf5-1.8.9/include',
                    '/usr/nld/fftw-3.3.3-mvapich2-1.9a2-gcc/include',
                    '/usr/nld/fftw-3.3.3-float-mvapich2-1.9a2-gcc/include']

    library_dirs = ['/usr/nld/mvapich2-1.9a2-gcc/lib',
                    '/usr/nld/gcc-4.7.2/lib64',
                    '/usr/nld/hdf5-1.8.9/lib',
                    '/usr/nld/fftw-3.3.3-mvapich2-1.9a2-gcc/lib',
                    '/usr/nld/fftw-3.3.3-float-mvapich2-1.9a2-gcc/lib']
    extra_libraries += ['mpich']
    extra_compile_args = ['-Wall',
                          '-O2',
                          '-g',
                          '-m64',
                          '-m80387',
                          '-mabi=sysv',
                          '-march=x86-64',
                          '-masm=intel',
                          '-masm=att',
                          '-mfancy-math-387',
                          '-mfpmath=sse+387',
                          '-mglibc',
                          '-mhard-float',
                          '-mieee-fp',
                          '-ffast-math',
#                          '-mlarge-data-threshold=65536',
                          '-mno-sse4',
                          '-mpush-args',
                          '-mred-zone',
                          '-msse4.2',
                          '-mstackrealign',
                          '-mtls-direct-seg-refs',
                          '-mtune=corei7',
                          '-std=c++11']

if hostname in ['tolima', 'misti']:
    local_install_dir = '/scratch.local/chichi/installs'

    include_dirs = ['/usr/lib64/mpi/gcc/openmpi/include',
                    os.path.join(local_install_dir, 'include')]

    library_dirs = ['/usr/lib64/mpi/gcc/openmpi/lib64',
                    os.path.join(local_install_dir, 'lib'),
                    os.path.join(local_install_dir, 'lib64')]
    extra_libraries += ['mpi_cxx', 'mpi']


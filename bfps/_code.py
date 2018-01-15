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
import sys
import shutil
import subprocess
import argparse
import h5py
from datetime import datetime
import math
import warnings

import bfps
from ._base import _base

class _code(_base):
    """This class is meant to stitch together the C++ code into a final source file,
    compile it, and handle all job launching.
    """
    def __init__(
            self,
            work_dir = './',
            simname = 'test'):
        _base.__init__(self, work_dir = work_dir, simname = simname)
        self.version_message = (
                '/***********************************************************************\n' +
                '* this code automatically generated by bfps\n' +
                '* version {0}\n'.format(bfps.__version__) +
                '***********************************************************************/\n\n\n')
        self.includes = """
                //begincpp
                #include "base.hpp"
                #include "fluid_solver.hpp"
                #include "scope_timer.hpp"
                #include "fftw_interface.hpp"
                #include <iostream>
                #include <hdf5.h>
                #include <string>
                #include <cstring>
                #include <fftw3-mpi.h>
				#include <omp.h>
                #include <fenv.h>
                #include <cstdlib>
                //endcpp
                """
        self.variables = 'int myrank, nprocs;\n'
        self.variables += 'int iteration;\n'
        self.variables += 'char simname[256], fname[256];\n'
        self.variables += ('hid_t parameter_file, stat_file, Cdset;\n')
        self.definitions = ''
        self.main_start = """
                //begincpp
                int main(int argc, char *argv[])
                {
                    if(getenv("BFPS_FPE_OFF") == nullptr || getenv("BFPS_FPE_OFF") != std::string("TRUE")){
                        feenableexcept(FE_INVALID | FE_OVERFLOW);
                    }
                    else{
                        std::cout << "FPE have been turned OFF" << std::endl;
                    }
                    if (argc != 2)
                    {
                        std::cerr << "Wrong number of command line arguments. Stopping." << std::endl;
                        MPI_Finalize();
                        return EXIT_SUCCESS;
                    }
                #ifdef NO_FFTWOMP
                    MPI_Init(&argc, &argv);
                    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
                    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
                    fftw_mpi_init();
                    fftwf_mpi_init();
                    DEBUG_MSG("There are %d processes\\n", nprocs);
                #else
                    int mpiprovided;
                    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &mpiprovided);
                    assert(mpiprovided >= MPI_THREAD_FUNNELED);
                    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
                    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
                    const int nbThreads = omp_get_max_threads();
                    DEBUG_MSG("Number of threads for the FFTW = %d\\n", nbThreads);
                    if (nbThreads > 1){
                        fftw_init_threads();
                        fftwf_init_threads();
                    }
                    fftw_mpi_init();
                    fftwf_mpi_init();
                    DEBUG_MSG("There are %d processes and %d threads\\n", nprocs, nbThreads);
                    if (nbThreads > 1){
                        fftw_plan_with_nthreads(nbThreads);
                        fftwf_plan_with_nthreads(nbThreads);
                    }
                #endif
                    strcpy(simname, argv[1]);
                    sprintf(fname, "%s.h5", simname);
                    parameter_file = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
                    Cdset = H5Dopen(parameter_file, "iteration", H5P_DEFAULT);
                    H5Dread(
                        Cdset,
                        H5T_NATIVE_INT,
                        H5S_ALL,
                        H5S_ALL,
                        H5P_DEFAULT,
                        &iteration);
                    DEBUG_MSG("simname is %s and iteration is %d\\n",
                              simname, iteration);
                    H5Dclose(Cdset);
                    H5Fclose(parameter_file);
                    read_parameters();
                    if (myrank == 0)
                    {
                        // set caching parameters
                        hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
                        herr_t cache_err = H5Pset_cache(fapl, 0, 521, 134217728, 1.0);
                        DEBUG_MSG("when setting stat_file cache I got %d\\n", cache_err);
                        stat_file = H5Fopen(fname, H5F_ACC_RDWR, fapl);
                    }
                    {
                        TIMEZONE("code::main_start");
                //endcpp
                """
        for ostream in ['cout', 'cerr']:
            self.main_start += 'if (myrank == 0) std::{1} << "{0}" << std::endl;'.format(
                    self.version_message, ostream).replace('\n', '\\n') + '\n'
        self.main_end = """
                //begincpp
                    }
                    // clean up
                    if (myrank == 0)
                    {
                        Cdset = H5Dopen(stat_file, "iteration", H5P_DEFAULT);
                        H5Dwrite(Cdset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &iteration);
                        H5Dclose(Cdset);
                        H5Fclose(stat_file);
                    }
                    fftwf_mpi_cleanup();
                    fftw_mpi_cleanup();
                #ifndef NO_FFTWOMP
                    if (nbThreads > 1){
                        fftw_cleanup_threads();
                        fftwf_cleanup_threads();
                    }
                #endif
                    #ifdef USE_TIMINGOUTPUT
                    global_timer_manager.show(MPI_COMM_WORLD);
                    global_timer_manager.showHtml(MPI_COMM_WORLD);
                    #endif
                    MPI_Finalize();
                    return EXIT_SUCCESS;
                }
                //endcpp
                """
        self.host_info = {'type'        : 'cluster',
                          'environment' : None,
                          'deltanprocs' : 1,
                          'queue'       : '',
                          'mail_address': '',
                          'mail_events' : None}
        self.main = ''
        return None
    def write_src(self):
        with open(self.name + '.cpp', 'w') as outfile:
            outfile.write(self.version_message)
            outfile.write(self.includes)
            outfile.write(self.cdef_pars())
            outfile.write(self.variables)
            outfile.write(self.cread_pars())
            outfile.write(self.definitions)
            outfile.write(self.main_start)
            outfile.write(self.main)
            outfile.write(self.main_end)
        return None
    def compile_code(self):
        # compile code
        if not os.path.isfile(os.path.join(bfps.header_dir, 'base.hpp')):
            raise IOError('header not there:\n' +
                          '{0}\n'.format(os.path.join(bfps.header_dir, 'base.hpp')) +
                          '{0}\n'.format(bfps.dist_loc))
        libraries = ['bfps']
        libraries += bfps.install_info['libraries']

        command_strings = [bfps.install_info['compiler']]
        command_strings += [self.name + '.cpp', '-o', self.name]
        command_strings += bfps.install_info['extra_compile_args']
        command_strings += ['-I' + idir for idir in bfps.install_info['include_dirs']]
        command_strings.append('-I' + bfps.header_dir)
        command_strings += ['-L' + ldir for ldir in bfps.install_info['library_dirs']]
        command_strings += ['-Wl,-rpath=' + ldir for ldir in bfps.install_info['library_dirs']]
        command_strings.append('-L' + bfps.lib_dir)
        command_strings.append('-Wl,-rpath=' + bfps.lib_dir)

        for libname in libraries:
            if libname[0] not in ['-', '/']:
                command_strings += ['-l' + libname]
            else:
                command_strings += [libname]

        command_strings += ['-fopenmp']

        self.write_src()
        print('compiling code with command\n' + ' '.join(command_strings))
        return subprocess.check_call(command_strings)
    def set_host_info(
            self,
            host_info = {}):
        self.host_info.update(host_info)
        return None
    def run(self,
            nb_processes,
            nb_threads_per_process,
            out_file = 'out_file',
            err_file = 'err_file',
            hours = 0,
            minutes = 10,
            njobs = 1,
            no_submit = False):
        self.read_parameters()
        with h5py.File(os.path.join(self.work_dir, self.simname + '.h5'), 'r') as data_file:
            iter0 = data_file['iteration'].value
        if not os.path.isdir(self.work_dir):
            os.makedirs(self.work_dir)
        if not os.path.exists(os.path.join(self.work_dir, self.name)):
            need_to_compile = True
        else:
            need_to_compile = (datetime.fromtimestamp(os.path.getctime(os.path.join(self.work_dir, self.name))) <
                               bfps.install_info['install_date'])
        if need_to_compile:
            assert self.compile_code() == 0
            if self.work_dir != os.path.realpath(os.getcwd()):
                shutil.copy(self.name, self.work_dir)
        if 'niter_todo' not in self.parameters.keys():
            self.parameters['niter_todo'] = 1
        current_dir = os.getcwd()
        os.chdir(self.work_dir)
        os.chdir(current_dir)
        command_atoms = ['mpirun',
                         '-np',
                         '{0}'.format(nb_processes),
                         '-x',
                         'OMP_NUM_THREADS={0}'.format(nb_threads_per_process),
                         './' + self.name,
                         self.simname]
        if self.host_info['type'] == 'cluster':
            job_name_list = []
            for j in range(njobs):
                suffix = self.simname + '_{0}'.format(iter0 + j*self.parameters['niter_todo'])
                qsub_script_name = 'run_' + suffix + '.sh'
                self.write_sge_file(
                    file_name     = os.path.join(self.work_dir, qsub_script_name),
                    nprocesses    = nb_processes*nb_threads_per_process,
                    name_of_run   = suffix,
                    command_atoms = command_atoms[5:],
                    hours         = hours,
                    minutes       = minutes,
                    out_file      = out_file + '_' + suffix,
                    err_file      = err_file + '_' + suffix)
                os.chdir(self.work_dir)
                qsub_atoms = ['qsub']
                if len(job_name_list) >= 1:
                    qsub_atoms += ['-hold_jid', job_name_list[-1]]
                subprocess.check_call(qsub_atoms + [qsub_script_name])
                os.chdir(current_dir)
                job_name_list.append(suffix)
        if self.host_info['type'] == 'SLURM':
            job_id_list = []
            for j in range(njobs):
                suffix = self.simname + '_{0}'.format(iter0 + j*self.parameters['niter_todo'])
                qsub_script_name = 'run_' + suffix + '.sh'
                self.write_slurm_file(
                    file_name     = os.path.join(self.work_dir, qsub_script_name),
                    name_of_run   = suffix,
                    command_atoms = command_atoms[5:],
                    hours         = hours,
                    minutes       = minutes,
                    out_file      = out_file + '_' + suffix,
                    err_file      = err_file + '_' + suffix,
                    nb_mpi_processes = nb_processes,
			        nb_threads_per_process = nb_threads_per_process)
                os.chdir(self.work_dir)
                qsub_atoms = ['sbatch']

                if not no_submit:
                    if len(job_id_list) >= 1:
                        qsub_atoms += ['--dependency=afterok:{0}'.format(job_id_list[-1])]
                    p = subprocess.Popen(
                        qsub_atoms + [qsub_script_name],
                        stdout = subprocess.PIPE)
                    out, err = p.communicate()
                    p.terminate()
                    job_id_list.append(int(out.split()[-1]))
                os.chdir(current_dir)
        elif self.host_info['type'] == 'IBMLoadLeveler':
            suffix = self.simname + '_{0}'.format(iter0)
            job_script_name = 'run_' + suffix + '.sh'
            if (njobs == 1):
                self.write_IBMLoadLeveler_file_single_job(
                    file_name     = os.path.join(self.work_dir, job_script_name),
                    name_of_run   = suffix,
                    command_atoms = command_atoms[5:],
                    hours         = hours,
                    minutes       = minutes,
                    out_file      = out_file + '_' + suffix,
                    err_file      = err_file + '_' + suffix,
                    nb_mpi_processes = nb_processes,
			        nb_threads_per_process = nb_threads_per_process)
            else:
                self.write_IBMLoadLeveler_file_many_job(
                    file_name     = os.path.join(self.work_dir, job_script_name),
                    name_of_run   = suffix,
                    command_atoms = command_atoms[5:],
                    hours         = hours,
                    minutes       = minutes,
                    out_file      = out_file + '_' + suffix,
                    err_file      = err_file + '_' + suffix,
                    njobs = njobs,
                    nb_mpi_processes = nb_processes,
			        nb_threads_per_process = nb_threads_per_process)
            submit_atoms = ['llsubmit']

            if not no_submit:
                subprocess.check_call(submit_atoms + [os.path.join(self.work_dir, job_script_name)])

        elif self.host_info['type'] == 'pc':
            os.chdir(self.work_dir)
            if os.getenv('LD_LIBRARY_PATH') != None:
                os.environ['LD_LIBRARY_PATH'] += ':{0}'.format(bfps.lib_dir)
                print('added to LD_LIBRARY_PATH the location {0}'.format(bfps.lib_dir))
            for j in range(njobs):
                suffix = self.simname + '_{0}'.format(iter0 + j*self.parameters['niter_todo'])
                print('running code with command\n' + ' '.join(command_atoms))
                subprocess.check_call(command_atoms,
                                stdout = open(out_file + '_' + suffix, 'w'),
                                stderr = open(err_file + '_' + suffix, 'w'))
            os.chdir(current_dir)
        return None
    def write_IBMLoadLeveler_file_single_job(
            self,
            file_name = None,
            nprocesses = None,
            name_of_run = None,
            command_atoms = [],
            hours = None,
            minutes = None,
            out_file = None,
            err_file = None,
			nb_mpi_processes = None,
			nb_threads_per_process = None):

        script_file = open(file_name, 'w')
        script_file.write('# @ shell=/bin/bash\n')
        # error file
        if type(err_file) == type(None):
            err_file = 'err.job.$(jobid)'
        script_file.write('# @ error = ' + os.path.join(self.work_dir, err_file) + '\n')
        # output file
        if type(out_file) == type(None):
            out_file = 'out.job.$(jobid)'
        script_file.write('# @ output = ' + os.path.join(self.work_dir, out_file) + '\n')

        # If Ibm is used should be : script_file.write('# @ job_type = parallel\n')
        script_file.write('# @ job_type = MPICH\n')
        assert(type(self.host_info['environment']) != type(None))
        script_file.write('# @ class = {0}\n'.format(self.host_info['environment']))

        script_file.write('# @ node_usage = not_shared\n')
        script_file.write('# @ notification = complete\n')
        script_file.write('# @ notify_user = $(user)@rzg.mpg.de\n')

        nb_cpus_per_node = self.host_info['deltanprocs']
        assert isinstance(nb_cpus_per_node, int) and \
               nb_cpus_per_node >= 1, \
               'nb_cpus_per_node is {}'.format(nb_cpus_per_node)

        # No more threads than the number of cores
        assert nb_threads_per_process <= nb_cpus_per_node, \
               "Cannot use more threads ({} asked) than the number of cores ({})".format(
                   nb_threads_per_process, nb_cpus_per_node)
        # Warn if some core will not be ued
        if nb_cpus_per_node%nb_threads_per_process != 0:
            warnings.warn("The number of threads is smaller than the number of cores (machine will be underused)",
                    UserWarning)

        nb_cpus = nb_mpi_processes*nb_threads_per_process
        if (nb_cpus < nb_cpus_per_node):
            # in case we use only a few process on a single node
            nb_nodes = 1
            nb_processes_per_node = nb_mpi_processes
            first_node_tasks = nb_mpi_processes
        else:
            nb_nodes = int((nb_cpus+nb_cpus_per_node-1) // nb_cpus_per_node)
            # if more than one node we requiere to have a multiple of deltanprocs
            nb_processes_per_node = int(nb_cpus_per_node // nb_threads_per_process)
            first_node_tasks = int(nb_mpi_processes - (nb_nodes-1)*nb_processes_per_node)

        script_file.write('# @ resources = ConsumableCpus({})\n'.format(nb_threads_per_process))
        script_file.write('# @ network.MPI = sn_all,not_shared,us\n')
        script_file.write('# @ wall_clock_limit = {0}:{1:0>2d}:00\n'.format(hours, minutes))
        script_file.write('# @ node = {0}\n'.format(nb_nodes))
        script_file.write('# @ tasks_per_node = {0}\n'.format(nb_processes_per_node))
        if (first_node_tasks > 0):
            script_file.write('# @ first_node_tasks = {0}\n'.format(first_node_tasks))
        script_file.write('# @ queue\n')


        script_file.write('source ~/.config/bfps/bashrc\n')
        script_file.write('module li\n')
        script_file.write('export OMP_NUM_THREADS={}\n'.format(nb_threads_per_process))

        script_file.write('LD_LIBRARY_PATH=' +
                          ':'.join([bfps.lib_dir] + bfps.install_info['library_dirs']) +
                          ':${LD_LIBRARY_PATH}\n')
        script_file.write('echo "Start time is `date`"\n')
        script_file.write('export HTMLOUTPUT={}.html\n'.format(command_atoms[-1]))
        script_file.write('cd ' + self.work_dir + '\n')

        script_file.write('export KMP_AFFINITY=compact,verbose\n')
        script_file.write('export I_MPI_PIN_DOMAIN=omp\n')
        script_file.write('mpiexec.hydra '
            + ' -np {} '.format(nb_mpi_processes)
            + ' -ppn {} '.format(nb_processes_per_node)
            + ' -ordered-output -prepend-rank '
            + os.path.join(
                self.work_dir,
                command_atoms[0]) +
            ' ' +
            ' '.join(command_atoms[1:]) +
            '\n')

        script_file.write('echo "End time is `date`"\n')
        script_file.write('exit 0\n')
        script_file.close()
        return None
    def write_IBMLoadLeveler_file_many_job(
            self,
            file_name = None,
            nprocesses = None,
            name_of_run = None,
            command_atoms = [],
            hours = None,
            minutes = None,
            out_file = None,
            err_file = None,
            njobs = 2,
			nb_mpi_processes = None,
			nb_threads_per_process = None):
        assert(type(self.host_info['environment']) != type(None))
        script_file = open(file_name, 'w')
        script_file.write('# @ shell=/bin/bash\n')
        # error file
        if type(err_file) == type(None):
            err_file = 'err.job.$(jobid).$(stepid)'
        script_file.write('# @ error = ' + os.path.join(self.work_dir, err_file) + '\n')
        # output file
        if type(out_file) == type(None):
            out_file = 'out.job.$(jobid).$(stepid)'
        script_file.write('# @ output = ' + os.path.join(self.work_dir, out_file) + '\n')
        # If Ibm is used should be : script_file.write('# @ job_type = parallel\n')
        script_file.write('# @ job_type = MPICH\n')
        assert(type(self.host_info['environment']) != type(None))
        script_file.write('# @ class = {0}\n'.format(self.host_info['environment']))
        script_file.write('# @ node_usage = not_shared\n')
        script_file.write('#\n')

        nb_cpus_per_node = self.host_info['deltanprocs']
        assert isinstance(nb_cpus_per_node, int) and \
               nb_cpus_per_node >= 1, \
               'nb_cpus_per_node is {}'.format(nb_cpus_per_node)

        # No more threads than the number of cores
        assert nb_threads_per_process <= nb_cpus_per_node, \
               "Cannot use more threads ({} asked) than the number of cores ({})".format(
                   nb_threads_per_process, nb_cpus_per_node)
        # Warn if some core will not be ued
        if nb_cpus_per_node%nb_threads_per_process != 0:
            warnings.warn("The number of threads is smaller than the number of cores (machine will be underused)",
                    UserWarning)

        nb_cpus = nb_mpi_processes*nb_threads_per_process
        if (nb_cpus < nb_cpus_per_node):
            # in case we use only a few process on a single node
            nb_nodes = 1
            nb_processes_per_node = nb_mpi_processes
            first_node_tasks = nb_mpi_processes
        else:
            nb_nodes = int((nb_cpus+nb_cpus_per_node-1) // nb_cpus_per_node)
            # if more than one node we requiere to have a multiple of deltanprocs
            nb_processes_per_node = int(nb_cpus_per_node // nb_threads_per_process)
            first_node_tasks = int(nb_mpi_processes - (nb_nodes-1)*nb_processes_per_node)

        for job in range(njobs):
            script_file.write('# @ step_name = {0}.{1}\n'.format(self.simname, job))
            if job > 0:
                script_file.write('# @ dependency = {0}.{1} == 0\n'.format(self.simname, job - 1))
            script_file.write('# @ resources = ConsumableCpus({})\n'.format(nb_threads_per_process))
            script_file.write('# @ network.MPI = sn_all,not_shared,us\n')
            script_file.write('# @ wall_clock_limit = {0}:{1:0>2d}:00\n'.format(hours, minutes))
            assert type(self.host_info['environment']) != type(None)
            script_file.write('# @ node = {0}\n'.format(nb_nodes))
            script_file.write('# @ tasks_per_node = {0}\n'.format(nb_processes_per_node))
            if (first_node_tasks > 0):
                script_file.write('# @ first_node_tasks = {0}\n'.format(first_node_tasks))
            script_file.write('# @ queue\n')

        script_file.write('source ~/.config/bfps/bashrc\n')
        script_file.write('module li\n')
        script_file.write('export OMP_NUM_THREADS={}\n'.format(nb_threads_per_process))

        script_file.write('LD_LIBRARY_PATH=' +
                          ':'.join([bfps.lib_dir] + bfps.install_info['library_dirs']) +
                          ':${LD_LIBRARY_PATH}\n')
        script_file.write('echo "Start time is `date`"\n')
        script_file.write('export HTMLOUTPUT={}.html\n'.format(command_atoms[-1]))
        script_file.write('cd ' + self.work_dir + '\n')

        script_file.write('export KMP_AFFINITY=compact,verbose\n')
        script_file.write('export I_MPI_PIN_DOMAIN=omp\n')

        script_file.write('mpiexec.hydra '
            + ' -np {} '.format(nb_mpi_processes)
            + ' -ppn {} '.format(nb_processes_per_node)
            + ' -ordered-output -prepend-rank '
            + os.path.join(
                self.work_dir,
                command_atoms[0]) +
            ' ' +
            ' '.join(command_atoms[1:]) +
            '\n')

        script_file.write('echo "End time is `date`"\n')
        script_file.write('exit 0\n')
        script_file.close()
        return None
    def write_sge_file(
            self,
            file_name = None,
            nprocesses = None,
            name_of_run = None,
            command_atoms = [],
            hours = None,
            minutes = None,
            out_file = None,
            err_file = None):
        script_file = open(file_name, 'w')
        script_file.write('#!/bin/bash\n')
        # export all environment variables
        script_file.write('#$ -V\n')
        # job name
        script_file.write('#$ -N {0}\n'.format(name_of_run))
        # use current working directory
        script_file.write('#$ -cwd\n')
        # error file
        if not type(err_file) == type(None):
            script_file.write('#$ -e ' + err_file + '\n')
        # output file
        if not type(out_file) == type(None):
            script_file.write('#$ -o ' + out_file + '\n')
        if not type(self.host_info['environment']) == type(None):
            envprocs = self.host_info['deltanprocs'] * int(math.ceil((nprocesses *1.0/ self.host_info['deltanprocs'])))
            script_file.write('#$ -pe {0} {1}\n'.format(
                    self.host_info['environment'],
                    envprocs))
        script_file.write('echo "got $NSLOTS slots."\n')
        script_file.write('echo "Start time is `date`"\n')
        script_file.write('mpiexec -machinefile $TMPDIR/machines ' +
                          '-genv LD_LIBRARY_PATH ' +
                          '"' +
                          ':'.join([bfps.lib_dir] + bfps.install_info['library_dirs']) +
                          '" ' +
                          '-n {0} {1}\n'.format(nprocesses, ' '.join(command_atoms)))
        script_file.write('echo "End time is `date`"\n')
        script_file.write('exit 0\n')
        script_file.close()
        return None
    def write_slurm_file(
            self,
            file_name = None,
            name_of_run = None,
            command_atoms = [],
            hours = None,
            minutes = None,
            out_file = None,
            err_file = None,
			nb_mpi_processes = None,
			nb_threads_per_process = None):
        script_file = open(file_name, 'w')
        script_file.write('#!/bin/bash -l\n')
        # job name
        script_file.write('#SBATCH -J {0}\n'.format(name_of_run))
        # use current working directory
        script_file.write('#SBATCH -D ./\n')
        # error file
        if not type(err_file) == type(None):
            script_file.write('#SBATCH -e ' + err_file + '\n')
        # output file
        if not type(out_file) == type(None):
            script_file.write('#SBATCH -o ' + out_file + '\n')
        script_file.write('#SBATCH --partition={0}\n'.format(
                self.host_info['environment']))

        nb_cpus_per_node = self.host_info['deltanprocs']
        assert isinstance(nb_cpus_per_node, int) \
               and nb_cpus_per_node >= 1, \
               'nb_cpus_per_node is {}'.format(nb_cpus_per_node)

        # No more threads than the number of cores
        assert nb_threads_per_process <= nb_cpus_per_node, \
               "Cannot use more threads ({} asked) than the number of cores ({})".format(
                   nb_threads_per_process, nb_cpus_per_node)
        # Warn if some core will not be ued
        if nb_cpus_per_node%nb_threads_per_process != 0:
            warnings.warn(
                    "The number of threads is smaller than the number of cores (machine will be underused)",
                    UserWarning)

        nb_cpus = nb_mpi_processes*nb_threads_per_process
        if (nb_cpus < nb_cpus_per_node):
            # in case we use only a few process on a single node
            nb_nodes = 1
            nb_processes_per_node = nb_mpi_processes
        else:
            nb_nodes = int((nb_cpus+nb_cpus_per_node-1) // nb_cpus_per_node)
            # if more than one node we requiere to have a multiple of deltanprocs
            nb_processes_per_node = int(nb_cpus_per_node // nb_threads_per_process)


        script_file.write('#SBATCH --nodes={0}\n'.format(nb_nodes))
        script_file.write('#SBATCH --ntasks-per-node={0}\n'.format(nb_processes_per_node))
        script_file.write('#SBATCH --cpus-per-task={0}\n'.format(nb_threads_per_process))

        script_file.write('#SBATCH --mail-type=none\n')
        script_file.write('#SBATCH --time={0}:{1:0>2d}:00\n'.format(hours, minutes))
        script_file.write('source ~/.config/bfps/bashrc\n')
        if nb_threads_per_process > 1:
            script_file.write('export OMP_NUM_THREADS={0}\n'.format(nb_threads_per_process))
            script_file.write('export OMP_PLACES=cores\n')

        script_file.write('LD_LIBRARY_PATH=' +
                          ':'.join([bfps.lib_dir] + bfps.install_info['library_dirs']) +
                          ':${LD_LIBRARY_PATH}\n')
        script_file.write('echo "Start time is `date`"\n')
        script_file.write('cd ' + self.work_dir + '\n')
        script_file.write('export HTMLOUTPUT={}.html\n'.format(command_atoms[-1]))
        script_file.write('srun {0}\n'.format(' '.join(command_atoms)))
        script_file.write('echo "End time is `date`"\n')
        script_file.write('exit 0\n')
        script_file.close()
        return None
    def prepare_launch(
            self,
            args = [],
            **kwargs):
        parser = argparse.ArgumentParser('bfps ' + type(self).__name__)
        self.add_parser_arguments(parser)
        opt = parser.parse_args(args)

        if opt.ncpu != -1:
            warnings.warn(
                    'ncpu should be replaced by np/ntpp',
                    DeprecationWarning)
            opt.nb_processes = opt.ncpu
            opt.nb_threads_per_process = 1

        self.set_host_info(bfps.host_info)
        if type(opt.environment) != type(None):
            self.host_info['environment'] = opt.environment
        return opt


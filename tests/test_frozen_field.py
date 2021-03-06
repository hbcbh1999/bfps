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



from base import *

class FrozenFieldParticles(bfps.NavierStokes):
    def __init__(
            self,
            name = 'FrozenFieldParticles',
            work_dir = './',
            simname = 'test',
            frozen_fields = True,
            fluid_precision = 'single',
            use_fftw_wisdom = False):
        super(FrozenFieldParticles, self).__init__(
                name = name,
                work_dir = work_dir,
                simname = simname,
                fluid_precision = fluid_precision,
                frozen_fields = True,
                use_fftw_wisdom = use_fftw_wisdom)
        return None

from test_convergence import convergence_test

if __name__ == '__main__':
    opt = parser.parse_args(
            ['-n', '16',
             '--run',
             '--initialize',
             '--ncpu', '2',
             '--nparticles', '1000',
             '--niter_todo', '32',
             '--precision', 'single',
             '--wd', 'data/single'] +
            sys.argv[1:])
    convergence_test(
            opt,
            launch,
            code_class = FrozenFieldParticles)


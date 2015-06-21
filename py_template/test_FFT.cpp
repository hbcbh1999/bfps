fluid_solver<float> *fs;
fs = new fluid_solver<float>(32, 32, 32);
DEBUG_MSG("fluid_solver object created\n");

vector_field<float> cv(fs->cd, fs->cvorticity);
vector_field<float> rv(fs->cd, fs->rvorticity);

fs->cd->read(
        "Kdata0",
        (void*)fs->cvorticity);
DEBUG_MSG("######### %g\n", fs->correl_vec(fs->cvorticity, fs->cvorticity));
fftwf_execute(*(fftwf_plan*)fs->c2r_vorticity);
fftwf_execute(*(fftwf_plan*)fs->r2c_vorticity);
cv = cv*(1. / (fs->normalization_factor));
DEBUG_MSG("######### %g\n", fs->correl_vec(fs->cvorticity, fs->cvorticity));

DEBUG_MSG("full size is %ld\n", fs->rd->full_size);

delete fs;
DEBUG_MSG("fluid_solver object deleted\n");


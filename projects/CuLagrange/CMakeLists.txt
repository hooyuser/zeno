cmake_minimum_required(VERSION 3.19)

if(NOT ZENO_WITH_CUDA)
  message(FATAL_ERROR "CUDA must be ON when CuLagrange is ON! "
    "Please specify: -DZENO_WITH_CUDA:BOOL=ON -DZENO_WITH_zenvdb:BOOL=ON -DZENO_WITH_ZenoFX:BOOL=ON")
endif()

target_link_libraries(zeno PRIVATE zshelper)

#[=[
target_sources(zeno PRIVATE
  test1.cu
)
#]=]
set_target_properties(zeno
  PROPERTIES # CUDA_EXTENSIONS ON
  CUDA_SEPARABLE_COMPILATION ON
  CUDA_RESOLVE_DEVICE_SYMBOLS ON
  CUDA_ARCHITECTURES OFF
)

#[=[
target_sources(zeno PRIVATE
  POC/jit.cu
)
#]=]

# mpm
target_sources(zeno PRIVATE
  mpm/Boundary.cu
  mpm/Generation.cu
  mpm/Generation.cpp
  mpm/Transfer.cu
  mpm/MPMPipeline.cu
  mpm/Maintenance.cu
  mpm/Temp.cu
)

# pbd
target_sources(zeno PRIVATE
  pbd/PBDInit.cu
  pbd/PBDPipeline.cu
)

# fem
target_sources(zeno PRIVATE
  fem/SolverInit.cu
  fem/SolverPipeline.cu
  fem/SolverLinearSystem.cu
  fem/SolverIPC.cu
  fem/BoundaryCondition.cu
  fem/Collisions.cu

  # fem/Check.cu
  fem/Generation.cpp
  # fem/test.cpp
  fem/QuasiStaticStepping.cu

  fem/FastQuasiStaticStepping.cu

  fem/FleshQuasiStepping.cu
)

# geometry
target_sources(zeno PRIVATE
  geometry/file_parser/read_vtk_mesh.hpp
  geometry/file_parser/write_vtk_unstructured_mesh.hpp
  geometry/file_parser/vtk_types.hpp
  geometry/linear_system/mfcg.hpp
  geometry/linear_system/active_set.hpp
  geometry/kernel/laplace_matrix.hpp
  geometry/kernel/gradient_field.hpp
  geometry/kernel/bary_centric_weights.hpp
  geometry/SolveLaplacian.cu
  geometry/VectorField.cu
  geometry/SetupSkinningWeight.cu
  geometry/Skinning.cu
  geometry/BaryCentricInterpolator.cu
  geometry/BiharmonicBoundedWeight.cu
  geometry/MeshIO.cu
  geometry/Jiggle.cu
)

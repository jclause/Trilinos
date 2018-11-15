# #2410: This passes in the CUDA-RELESE-CUDA build so it is okay to disable in
# this DEBUG build.  This is likey related to the mix-lanauage compiler defect
# reported in #1208.
ATDM_SET_ENABLE(TeuchosNumerics_LAPACK_test_MPI_1_DISABLE ON)

# Disable some unit tests that run too slow in this DEBUG build (#2827)
ATDM_SET_CACHE(KokkosContainers_UnitTest_Serial_MPI_1_EXTRA_ARGS
  "--gtest_filter=-serial.bitset:serial.scatterview"
  CACHE STRING )
ATDM_SET_CACHE(KokkosKernels_graph_serial_MPI_1_EXTRA_ARGS
  "--gtest_filter=-serial.graph_graph_color_d2_double_int_int_TestExecSpace"
  CACHE STRING )
ATDM_SET_CACHE(KokkosKernels_sparse_serial_MPI_1_EXTRA_ARGS
  "--gtest_filter=-serial.sparse_block_gauss_seidel_double_int_int_TestExecSpace:serial.sparse_block_gauss_seidel_double_int_size_t_TestExecSpace:serial.sparse_trsv_mv_double_int_int_LayoutLeft_TestExecSpace"
  CACHE STRING )

INCLUDE("${CMAKE_CURRENT_LIST_DIR}/CUDA_COMMON_TWEAKS.cmake")

INCLUDE("${CMAKE_CURRENT_LIST_DIR}/ALL_COMMON_TWEAKS.cmake")

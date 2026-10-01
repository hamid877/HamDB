# CMake generated Testfile for 
# Source directory: /home/hamid/Documents/project/HamDB/tests/optimizer
# Build directory: /home/hamid/Documents/project/HamDB/tests/optimizer
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[PredicatePushdownTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/predicate_pushdown_test")
set_tests_properties([=[PredicatePushdownTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;4;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[ProjectionPruningTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/projection_pruning_test")
set_tests_properties([=[ProjectionPruningTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;9;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[ConstantFoldingTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/constant_folding_test")
set_tests_properties([=[ConstantFoldingTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;14;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[IndexScanRuleTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/index_scan_rule_test")
set_tests_properties([=[IndexScanRuleTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;19;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[SortLimitRuleTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/sort_limit_rule_test")
set_tests_properties([=[SortLimitRuleTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;24;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[OptimizerTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/optimizer_test")
set_tests_properties([=[OptimizerTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;29;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[JoinSelectionTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/join_selection_test")
set_tests_properties([=[JoinSelectionTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;34;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[TopKOptimizationTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/top_k_optimization_test")
set_tests_properties([=[TopKOptimizationTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;39;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[CardinalityEstimatorTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/cardinality_estimator_test")
set_tests_properties([=[CardinalityEstimatorTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;44;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")
add_test([=[CostModelTest]=] "/home/hamid/Documents/project/HamDB/tests/optimizer/cost_model_test")
set_tests_properties([=[CostModelTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;49;add_test;/home/hamid/Documents/project/HamDB/tests/optimizer/CMakeLists.txt;0;")

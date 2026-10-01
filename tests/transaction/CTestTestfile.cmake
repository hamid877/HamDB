# CMake generated Testfile for 
# Source directory: /home/hamid/Documents/project/HamDB/tests/transaction
# Build directory: /home/hamid/Documents/project/HamDB/tests/transaction
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[TransactionTest]=] "/home/hamid/Documents/project/HamDB/tests/transaction/transaction_test")
set_tests_properties([=[TransactionTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/transaction/CMakeLists.txt;8;add_test;/home/hamid/Documents/project/HamDB/tests/transaction/CMakeLists.txt;0;")
add_test([=[LockManagerTest]=] "/home/hamid/Documents/project/HamDB/tests/transaction/lock_manager_test")
set_tests_properties([=[LockManagerTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/transaction/CMakeLists.txt;12;add_test;/home/hamid/Documents/project/HamDB/tests/transaction/CMakeLists.txt;0;")
add_test([=[MvccManagerTest]=] "/home/hamid/Documents/project/HamDB/tests/transaction/mvcc_manager_test")
set_tests_properties([=[MvccManagerTest]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/hamid/Documents/project/HamDB/tests/transaction/CMakeLists.txt;16;add_test;/home/hamid/Documents/project/HamDB/tests/transaction/CMakeLists.txt;0;")

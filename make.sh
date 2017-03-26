#!/bin/bash
clang++ reader.cpp  -o reader -I $BOOST_ROOT/include/ --std=c++11 -lboost_filesystem -L $BOOST_ROOT/lib/ -lboost_system
clang++ writer.cpp  -o writer -I $BOOST_ROOT/include/ --std=c++11 -lboost_filesystem -L $BOOST_ROOT/lib/ -lboost_system

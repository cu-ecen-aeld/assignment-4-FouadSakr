#!/bin/bash
# Script to clean buildroot output using distclean

cd "$(dirname "$0")/buildroot"
make distclean
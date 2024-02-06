#!/bin/bash

if [ ! -d debian ]; then
  echo "This script needs to be run from the repository root!"
  exit 1
fi

set -xe
git clone https://git.kernel.org/pub/scm/libs/ell/ell.git ell_tmp_git
mv ell_tmp_git/ell ell
rm -rf ell_tmp_git

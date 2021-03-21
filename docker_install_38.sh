#!/bin/bash
docker run -v `pwd`:/workspace/SAPIEN -it --rm \
       -u $(id -u ${USER}):$(id -g ${USER}) \
       fxiangucsd/sapien-build-env:1.4 bash -c "cd /workspace/SAPIEN && ./build.sh 38"
pip3 uninstall sapien -y && cd wheelhouse && pip3 install *
cd /tmp && rm stubs -rf && pybind11-stubgen sapien.core --ignore-invalid all
cp /tmp/stubs/sapien/core-stubs/__init__.pyi /home/fx/source/sapien/python/py_package/core
cp -r /tmp/stubs/sapien/core-stubs/pysapien /home/fx/source/sapien/python/py_package/core

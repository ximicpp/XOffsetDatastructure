# Toolchain for the CppCon 2024 comparison rerun (bench/cppcon2024/run.sh builds it).
# Compiler: the same GCC 16.2 image the 4.0 scorecard uses (tools/perf_gate/repro_scorecard.sh).
FROM mirror.gcr.io/library/gcc@sha256:27470de79376e1033091a1f2cbfa805a10d790dcbd980389a07c4d6dc3606240
# Rivals from Debian 13 (trixie): protobuf 3.21.12, Cap'n Proto 1.1.0,
# FlatBuffers 23.5.26, msgpack-cxx 7.0.0, zstd 1.5.7 (bytes only).
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
      cmake libprotobuf-dev=3.21.12-11+deb13u1 protobuf-compiler=3.21.12-11+deb13u1 \
      capnproto=1.1.0-2 libcapnp-dev=1.1.0-2 \
      flatbuffers-compiler=23.5.26+dfsg-4 libflatbuffers-dev=23.5.26+dfsg-4 \
      libmsgpack-cxx-dev=7.0.0-1+b1 libzstd-dev=1.5.7+dfsg-1 \
    && rm -rf /var/lib/apt/lists/*
# zpp_bits and cista are cloned on the host by run.sh and mounted at /deps.

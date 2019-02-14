#!/bin/bash

set -e -x

cd "$(dirname "${0}")/.."

## Change these

gem5_arch="Spandex"
gem5_protocol=
gem5_compile_args=
target_app="./apps/square" # omit .cpp ending
gem5_run_args=
gem5_debug_flags=
# "--debug-flags=ProtocolTrace,RubyQueue,RubySlicc --debug-start=22963000000"
# "--debug-break=22963006000"
# "--debug-flags=SimpleCPU"
#  --debug-break=60000
#  --debug-break=23000000
gem5_script_args="--num-cpus=2 --cpu-type=TimingSimpleCPU --num-compute-units=8 --mem-size=2GB --network=garnet2.0 --ruby"
# --FunctionalTLB 
gem5_script=./configs/example/apu_se.py

## compile gem5

gem5_binary="./build/${gem5_arch}/gem5.opt"

if [ -n "${gem5_protocol}" ]
then
	gem5_compile_args="${gem5_compile_args} PROTOCOL=${gem5_protocol}"
fi

if [ -z "${no_scons}" ]
then
	scons "${gem5_binary}" -j 80 ${gem5_compile_args}
fi
## compile target

hipcc "${target_app}.cpp" -o "${target_app}.exe" -I./include --amdgpu-target=gfx801
# g++ "${target_app}.cpp" -o "${target_app}.exe" -I./include -std=c++11 -pthread
# I use the suffix .exe for executables so that you can `rm *.exe` to clean executables

## run target

outdir=$(dirname "${target_app}")/output
mkdir -p "${outdir}"

if [ -n "${gem5_debug_flags}" ]
then
	log="${outdir}/log"
else
	log=/dev/stdout
fi

if [ -n "${gdb}" ]
then
	gdb -q "${gem5_binary}" -ex 'run '"${gem5_run_args}"' --outdir='"${outdir}"' '"${gem5_debug_flags}"' '"${gem5_script}"' '"${gem5_script_args}"' -c '"${target_app}.exe"''
else
	"${gem5_binary}" ${gem5_run_args} --outdir="${outdir}" ${gem5_debug_flags} ${gem5_script} ${gem5_script_args} -c "${target_app}.exe" &> ${log}
fi
